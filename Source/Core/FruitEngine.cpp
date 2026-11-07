#include "FruitEngine.h"
#include <algorithm>
#include <cmath>

namespace openfruit::core {

namespace {

float softClip(float x) noexcept {
    // Smooth tanh-like soft saturation to prevent harsh clipping
    if (x > 1.25f) return 1.0f;
    if (x < -1.25f) return -1.0f;
    return x - (0.1066f * x * x * x);
}

} // namespace

void TrackState::triggerVoice(std::shared_ptr<const AudioSampleBuffer> sampleToPlay,
                              float velocity,
                              int semitoneOffset,
                              double sampleRate) {
    if (!sampleToPlay || !sampleToPlay->isLoaded) {
        return;
    }

    voices[static_cast<size_t>(nextVoiceIndex)].start(
        sampleToPlay,
        velocity,
        semitoneOffset,
        baseTune,
        sampleRate,
        volume,
        pan
    );
    nextVoiceIndex = (nextVoiceIndex + 1) % kMaxVoicesPerTrack;
}

FruitEngine::FruitEngine() {
    for (int i = 0; i < kMaxTracks; ++i) {
        tracks_[static_cast<size_t>(i)].trackIndex = i;
        tracks_[static_cast<size_t>(i)].midiChannel = i + 1;
        tracks_[static_cast<size_t>(i)].name = "Track " + std::to_string(i + 1);
        tracks_[static_cast<size_t>(i)].timing = StepTiming::Timing1_16;
        tracks_[static_cast<size_t>(i)].volume = 1.0f;
        tracks_[static_cast<size_t>(i)].pan = 0.0f;
    }
}

void FruitEngine::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = std::max(8000.0, sampleRate);
    reset();
}

void FruitEngine::reset() {
    const std::lock_guard<std::mutex> lock(engineMutex_);
    for (auto& track : tracks_) {
        for (auto& voice : track.voices) {
            voice.stop();
        }
        track.currentStep = 0;
        track.lastTriggeredStep = -1;
    }
    previewVoice_.stop();
    internalPpq_ = 0.0;
    lastPpq_ = 0.0;
}

void FruitEngine::setTrackCountMode(TrackCountMode mode) noexcept {
    trackCountMode_ = mode;
}

void FruitEngine::setInputControlMode(InputControlMode mode) noexcept {
    inputMode_ = mode;
}

void FruitEngine::setActivePage(PageIndex page) noexcept {
    activePage_ = page;
}

TrackState& FruitEngine::getTrack(int trackIndex) {
    const int idx = std::clamp(trackIndex, 0, kMaxTracks - 1);
    return tracks_[static_cast<size_t>(idx)];
}

const TrackState& FruitEngine::getTrack(int trackIndex) const {
    const int idx = std::clamp(trackIndex, 0, kMaxTracks - 1);
    return tracks_[static_cast<size_t>(idx)];
}

void FruitEngine::loadSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].sample = std::move(sample);
}

void FruitEngine::setAuditionSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].auditionSample = std::move(sample);
}

void FruitEngine::clearAuditionSampleForTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].auditionSample.reset();
}

void FruitEngine::commitAuditionSample(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    if (tracks_[static_cast<size_t>(trackIndex)].auditionSample) {
        tracks_[static_cast<size_t>(trackIndex)].sample = tracks_[static_cast<size_t>(trackIndex)].auditionSample;
        tracks_[static_cast<size_t>(trackIndex)].auditionSample.reset();
    }
}

void FruitEngine::setStepActive(int trackIndex, PageIndex page, int stepIndex, bool active) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].active = active;
}

void FruitEngine::setStepVelocity(int trackIndex, PageIndex page, int stepIndex, float velocity) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].velocity =
        std::clamp(velocity, 0.0f, 1.0f);
}

void FruitEngine::setStepTune(int trackIndex, PageIndex page, int stepIndex, int semitoneOffset) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].semitoneOffset =
        std::clamp(semitoneOffset, kMinSemitoneTune, kMaxSemitoneTune);
}

StepData FruitEngine::getStep(int trackIndex, PageIndex page, int stepIndex) const {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return {};
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    return tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)];
}

int FruitEngine::getCurrentStep(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return 0;
    }
    return tracks_[static_cast<size_t>(trackIndex)].currentStep;
}

void FruitEngine::triggerTrackManual(int trackIndex, float velocity, int semitoneOffset) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    auto& track = tracks_[static_cast<size_t>(trackIndex)];

    // If live auditioning in browser, trigger the audition sample; else regular track sample
    auto sampleToPlay = track.auditionSample ? track.auditionSample : track.sample;
    if (sampleToPlay) {
        track.triggerVoice(sampleToPlay, velocity, semitoneOffset, sampleRate_);
    }
}

void FruitEngine::handleMidiNoteOn(int channel1to16, int midiNote, float velocity) {
    const int trackIdx = channel1to16 - 1;
    if (trackIdx < 0 || trackIdx >= getActiveTrackCount()) {
        return;
    }

    const std::lock_guard<std::mutex> lock(engineMutex_);
    auto& track = tracks_[static_cast<size_t>(trackIdx)];
    auto sampleToPlay = track.auditionSample ? track.auditionSample : track.sample;
    if (!sampleToPlay) {
        return;
    }

    // In MIDI Mode, note 60 (C4/C3) is baseline (offset 0), notes above/below tune +/- semitones
    int semitoneOffset = 0;
    if (inputMode_ == InputControlMode::MidiMode) {
        semitoneOffset = std::clamp(midiNote - 60, kMinSemitoneTune, kMaxSemitoneTune);
    }

    track.triggerVoice(sampleToPlay, velocity, semitoneOffset, sampleRate_);
}

void FruitEngine::handleMidiNoteOff(int /*channel1to16*/, int /*midiNote*/) {
    // One-shot drum samples don't require note-off choke by default
}

void FruitEngine::advanceSequencer(double ppqStart, double ppqEnd, int /*numSamples*/, double /*bpm*/) {
    if (inputMode_ == InputControlMode::MidiMode) {
        // Step sequencer disabled in MIDI Mode
        return;
    }

    const int activeTracks = getActiveTrackCount();
    const size_t pageIdx = static_cast<size_t>(activePage_);

    for (int t = 0; t < activeTracks; ++t) {
        auto& track = tracks_[static_cast<size_t>(t)];
        const double spq = stepsPerQuarter(track.timing);

        const int64_t stepStart = static_cast<int64_t>(std::floor(ppqStart * spq));
        const int64_t stepEnd = static_cast<int64_t>(std::floor(ppqEnd * spq));

        const int step16 = static_cast<int>(stepEnd % kStepsPerPage);
        track.currentStep = (step16 >= 0) ? step16 : (step16 + kStepsPerPage);

        // Check if we advanced across a step boundary
        if (stepEnd > stepStart) {
            for (int64_t s = stepStart + 1; s <= stepEnd; ++s) {
                int sIdx = static_cast<int>(s % kStepsPerPage);
                if (sIdx < 0) sIdx += kStepsPerPage;

                const auto& step = track.patternPages.pages[pageIdx][static_cast<size_t>(sIdx)];
                if (step.active) {
                    auto sampleToPlay = track.auditionSample ? track.auditionSample : track.sample;
                    if (sampleToPlay) {
                        track.triggerVoice(sampleToPlay, step.velocity, step.semitoneOffset, sampleRate_);
                    }
                }
            }
        }
    }
}

void FruitEngine::process(float* leftChannel,
                          float* rightChannel,
                          int numSamples,
                          double bpm,
                          double ppqPosition,
                          bool isHostPlaying) {
    if (leftChannel == nullptr || rightChannel == nullptr || numSamples <= 0) {
        return;
    }

    const std::lock_guard<std::mutex> lock(engineMutex_);

    // 1. Advance Sequencer Clock if transport is running
    if (isHostPlaying && bpm > 10.0) {
        const double quarterNotesPerSample = bpm / (sampleRate_ * 60.0);
        const double ppqStart = (ppqPosition >= 0.0) ? ppqPosition : internalPpq_;
        const double ppqEnd = ppqStart + (static_cast<double>(numSamples) * quarterNotesPerSample);
        advanceSequencer(ppqStart, ppqEnd, numSamples, bpm);
        internalPpq_ = ppqEnd;
    }

    // 2. Determine Solo state
    const int activeTracks = getActiveTrackCount();
    bool anySolo = false;
    for (int t = 0; t < activeTracks; ++t) {
        if (tracks_[static_cast<size_t>(t)].solo) {
            anySolo = true;
            break;
        }
    }

    // 3. Render Voices
    for (int t = 0; t < activeTracks; ++t) {
        auto& track = tracks_[static_cast<size_t>(t)];
        if (track.mute) {
            continue;
        }
        if (anySolo && !track.solo) {
            continue;
        }

        for (auto& voice : track.voices) {
            if (voice.isPlaying()) {
                voice.renderNextBlock(leftChannel, rightChannel, numSamples);
            }
        }
    }

    // Render Preview Voice
    if (previewVoice_.isPlaying()) {
        previewVoice_.renderNextBlock(leftChannel, rightChannel, numSamples);
    }

    // 4. Apply Master Volume and Soft Saturation
    const float mVol = masterVolume_;
    for (int i = 0; i < numSamples; ++i) {
        leftChannel[i] = softClip(leftChannel[i] * mVol);
        rightChannel[i] = softClip(rightChannel[i] * mVol);
    }
}

} // namespace openfruit::core
