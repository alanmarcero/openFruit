#include "FruitEngine.h"
#include <algorithm>
#include <cmath>

namespace openfruit::core {

namespace {

float softClip(float x) noexcept {
    // Smooth C-infinity analog tape saturation with second-harmonic warmth
    const float s = std::tanh(x);
    return s + 0.03f * s * std::abs(s);
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
    justManuallyStarted_ = false;
    wasHostPlaying_ = false;
}

void FruitEngine::setTrackCountMode(TrackCountMode mode) noexcept {
    trackCountMode_ = mode;
}

void FruitEngine::setPlaying(bool playing) noexcept {
    const std::lock_guard<std::mutex> lock(engineMutex_);
    if (playing && !isPlaying_.load()) {
        for (auto& track : tracks_) {
            track.lastTriggeredStep = -1;
        }
    }
    if (!playing) {
        for (auto& track : tracks_) {
            track.lastTriggeredStep = -1;
        }
    }
    justManuallyStarted_ = playing;
    isPlaying_.store(playing);
    hostSyncArmed_.store(playing);
}

void FruitEngine::setTrackMidiChannel(int trackIndex, int channel1to16) noexcept {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].midiChannel = std::clamp(channel1to16, 1, 16);
}

int FruitEngine::getTrackMidiChannel(int trackIndex) const noexcept {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return 1;
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    return tracks_[static_cast<size_t>(trackIndex)].midiChannel;
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
    if (channel1to16 < 1 || channel1to16 > 16) {
        return;
    }

    const std::lock_guard<std::mutex> lock(engineMutex_);
    const int activeTracks = getActiveTrackCount();
    const int semitoneOffset = std::clamp(midiNote - 60, kMinSemitoneTune, kMaxSemitoneTune);

    for (int t = 0; t < activeTracks; ++t) {
        auto& track = tracks_[static_cast<size_t>(t)];
        if (track.midiChannel != channel1to16) {
            continue;
        }
        auto sampleToPlay = track.auditionSample ? track.auditionSample : track.sample;
        if (!sampleToPlay) {
            continue;
        }
        track.triggerVoice(sampleToPlay, velocity, semitoneOffset, sampleRate_);
    }
}

void FruitEngine::handleMidiNoteOff(int /*channel1to16*/, int /*midiNote*/) {
    // One-shot drum samples don't require note-off choke by default
}

void FruitEngine::advanceSequencer(double ppqStart, double ppqEnd, int /*numSamples*/, double /*bpm*/) {
    const int activeTracks = getActiveTrackCount();
    const size_t pageIdx = static_cast<size_t>(activePage_);

    for (int t = 0; t < activeTracks; ++t) {
        auto& track = tracks_[static_cast<size_t>(t)];
        const double spq = stepsPerQuarter(track.timing);

        const int64_t stepStart = static_cast<int64_t>(std::floor(ppqStart * spq));
        const int64_t stepEnd = static_cast<int64_t>(std::floor(ppqEnd * spq));

        const int step16 = static_cast<int>(stepEnd % kStepsPerPage);
        track.currentStep = (step16 >= 0) ? step16 : (step16 + kStepsPerPage);

        // Trigger initial step when playback first starts or loops
        if (track.lastTriggeredStep == -1) {
            int startIdx = static_cast<int>(stepStart % kStepsPerPage);
            if (startIdx < 0) startIdx += kStepsPerPage;
            track.lastTriggeredStep = startIdx;

            const auto& step = track.patternPages.pages[pageIdx][static_cast<size_t>(startIdx)];
            if (step.active) {
                auto sampleToPlay = track.auditionSample ? track.auditionSample : track.sample;
                if (sampleToPlay) {
                    track.triggerVoice(sampleToPlay, step.velocity, step.semitoneOffset, sampleRate_);
                }
            }
        }

        // Check if we advanced across a step boundary
        if (stepEnd > stepStart) {
            for (int64_t s = stepStart + 1; s <= stepEnd; ++s) {
                int sIdx = static_cast<int>(s % kStepsPerPage);
                if (sIdx < 0) sIdx += kStepsPerPage;
                track.lastTriggeredStep = sIdx;

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

    // 1. Handle host transport start/stop transitions
    if (justManuallyStarted_) {
        justManuallyStarted_ = false;
        wasHostPlaying_ = isHostPlaying;
    }

    const bool hostJustStarted = (!wasHostPlaying_ && isHostPlaying);
    const bool hostJustStopped = (wasHostPlaying_ && !isHostPlaying);
    wasHostPlaying_ = isHostPlaying;

    if (hostJustStopped) {
        isPlaying_.store(false);
        for (auto& track : tracks_) {
            track.lastTriggeredStep = -1;
        }
    }
    if (!hostJustStopped && hostJustStarted && (isPlaying_.load() || hostSyncArmed_.load())) {
        isPlaying_.store(true);
        for (auto& track : tracks_) {
            track.lastTriggeredStep = -1;
        }
    }

    // 2. Advance Sequencer Clock when Play is active
    if (isPlaying_.load()) {
        const double effectiveBpm = (bpm > 10.0) ? bpm : 120.0;
        const double quarterNotesPerSample = effectiveBpm / (sampleRate_ * 60.0);
        const double ppqStart = (isHostPlaying && ppqPosition >= 0.0) ? ppqPosition : internalPpq_;
        const double ppqEnd = ppqStart + (static_cast<double>(numSamples) * quarterNotesPerSample);

        if (isHostPlaying && ppqPosition >= 0.0 && ppqStart < lastPpq_ - 1e-4) {
            for (auto& track : tracks_) {
                track.lastTriggeredStep = -1;
            }
        }

        advanceSequencer(ppqStart, ppqEnd, numSamples, effectiveBpm);
        internalPpq_ = ppqEnd;
        lastPpq_ = ppqStart;
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
