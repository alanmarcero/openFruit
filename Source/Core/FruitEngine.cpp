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
                              double sampleRate,
                              VoiceSettings settings,
                              int midiNote) {
    if (!sampleToPlay || !sampleToPlay->isLoaded) {
        return;
    }

    settings.filterCutoffHz = filterCutoffHz;
    settings.highPassCutoffHz = highPassCutoffHz;
    settings.drive = drive;
    settings.attackSeconds = amplitudeAttackSeconds;
    settings.decaySeconds = amplitudeDecaySeconds;
    settings.sustainLevel = amplitudeSustain;
    settings.releaseSeconds = amplitudeReleaseSeconds;
    settings.midiNote = midiNote;
    voices[static_cast<size_t>(nextVoiceIndex)].start(
        sampleToPlay,
        velocity,
        semitoneOffset,
        baseTune,
        sampleRate,
        volume,
        pan,
        settings
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
    for (int i = 0; i < kNumPages; ++i)
        songSections_[static_cast<size_t>(i)].page = static_cast<PageIndex>(i);
}

void FruitEngine::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = std::max(8000.0, sampleRate);
    outputGainCurrent_ = outputGainTarget_;
    outputGainStep_ = 0.0f;
    outputGainRampRemaining_ = 0;
    reset();
}

void FruitEngine::setOutputDb(float decibels) noexcept {
    const float safeDb = std::clamp(std::isfinite(decibels) ? decibels : 0.0f, -24.0f, 12.0f);
    const float target = std::pow(10.0f, safeDb / 20.0f);
    if (target == outputGainTarget_) {
        return;
    }

    outputGainTarget_ = target;
    outputGainRampRemaining_ = std::max(1, static_cast<int>(sampleRate_ * 0.01));
    outputGainStep_ = (outputGainTarget_ - outputGainCurrent_) / static_cast<float>(outputGainRampRemaining_);
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
    visualPpq_ = 0.0;
    displayBarPhase_.store(0.0f, std::memory_order_relaxed);
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

void FruitEngine::setTrackNudge(int trackIndex, float amount) noexcept {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) return;
    tracks_[static_cast<size_t>(trackIndex)].nudge = std::clamp(std::isfinite(amount) ? amount : 0.0f, -0.5f, 0.5f);
}

void FruitEngine::setTrackGroove(int trackIndex, float amount) noexcept {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) return;
    tracks_[static_cast<size_t>(trackIndex)].groove = std::clamp(std::isfinite(amount) ? amount : 0.0f, 0.0f, 0.5f);
}

void FruitEngine::setAccentuator(float amount, int everySteps) noexcept {
    accentAmount_ = std::clamp(std::isfinite(amount) ? amount : 0.0f, 0.0f, 1.0f);
    accentEvery_ = std::clamp(everySteps, 2, 16);
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

std::shared_ptr<const AudioSampleBuffer> FruitEngine::loadedSample(int trackIndex) const {
    const std::lock_guard<std::mutex> lock(engineMutex_);
    return getTrack(trackIndex).sample;
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

void FruitEngine::setStepData(int trackIndex, PageIndex page, int stepIndex, StepData data) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) return;
    const int pageIndex = std::clamp(static_cast<int>(page), 0, kNumPages - 1);
    data.velocity = std::clamp(std::isfinite(data.velocity) ? data.velocity : 0.85f, 0.0f, 1.0f);
    data.semitoneOffset = std::clamp(data.semitoneOffset, kMinSemitoneTune, kMaxSemitoneTune);
    data.probability = std::clamp(std::isfinite(data.probability) ? data.probability : 1.0f, 0.0f, 1.0f);
    data.ratchets = std::clamp(data.ratchets, 1, 8);
    data.timingOffset = std::clamp(std::isfinite(data.timingOffset) ? data.timingOffset : 0.0f, -0.5f, 0.5f);
    data.sampleStart = std::clamp(std::isfinite(data.sampleStart) ? data.sampleStart : 0.0f, 0.0f, 0.999f);
    data.sampleEnd = std::clamp(std::isfinite(data.sampleEnd) ? data.sampleEnd : 1.0f, data.sampleStart + 0.001f, 1.0f);
    data.chokeGroup = std::clamp(data.chokeGroup, 0, 4);
    const std::lock_guard<std::mutex> lock(engineMutex_);
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(pageIndex)][static_cast<size_t>(stepIndex)] = data;
}

void FruitEngine::clearStepPatterns() {
    const std::lock_guard<std::mutex> lock(engineMutex_);
    for (auto& track : tracks_) track.patternPages = TrackPatternPages{};
}

StepData FruitEngine::getStep(int trackIndex, PageIndex page, int stepIndex) const {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return {};
    }
    const std::lock_guard<std::mutex> lock(engineMutex_);
    const int pageIndex = std::clamp(static_cast<int>(page), 0, kNumPages - 1);
    return tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(pageIndex)][static_cast<size_t>(stepIndex)];
}

void FruitEngine::setSongSection(int index, SongSection section) noexcept {
    if (index < 0 || index >= kNumPages) return;
    section.page = static_cast<PageIndex>(std::clamp(static_cast<int>(section.page), 0, kNumPages - 1));
    section.bars = std::clamp(section.bars, 1, 32);
    songSections_[static_cast<size_t>(index)] = section;
}

SongSection FruitEngine::getSongSection(int index) const noexcept {
    if (index < 0 || index >= kNumPages) return {};
    return songSections_[static_cast<size_t>(index)];
}

PageIndex FruitEngine::getSongPageAtPpq(double ppq) const noexcept {
    if (!songMode_ || !std::isfinite(ppq) || ppq < 0.0) return activePage_;
    int totalBars = 0;
    for (const auto& section : songSections_) totalBars += std::clamp(section.bars, 1, 32);
    const int barInSong = static_cast<int>(std::floor(ppq / 4.0)) % totalBars;
    int sectionStart = 0;
    for (const auto& section : songSections_) {
        const int bars = std::clamp(section.bars, 1, 32);
        if (barInSong < sectionStart + bars) {
            if (section.fill && barInSong == sectionStart + bars - 1) return PageIndex::PageD;
            return section.page;
        }
        sectionStart += bars;
    }
    return songSections_.back().page;
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
        track.triggerVoice(sampleToPlay, velocity, semitoneOffset, sampleRate_, {}, midiNote);
    }
}

void FruitEngine::handleMidiNoteOff(int channel1to16, int midiNote) {
    if (channel1to16 < 1 || channel1to16 > 16 || midiNote < 0 || midiNote > 127) return;
    const std::lock_guard<std::mutex> lock(engineMutex_);
    for (auto& track : tracks_) {
        if (track.midiChannel != channel1to16) continue;
        for (auto& voice : track.voices)
            if (voice.isPlaying() && voice.midiNote() == midiNote) voice.noteOff();
    }
}

void FruitEngine::scheduleSequencer(double ppqStart, double ppqEnd, int numSamples, double bpm) {
    scheduledHitCount_ = 0;
    const int activeTracks = getActiveTrackCount();
    const double samplesPerQuarter = sampleRate_ * 60.0 / bpm;
    bool full = false;

    for (int trackIndex = 0; trackIndex < activeTracks && !full; ++trackIndex) {
        auto& track = tracks_[static_cast<size_t>(trackIndex)];
        const double stepsPerBeat = stepsPerQuarter(track.timing);
        const int64_t currentStep = static_cast<int64_t>(std::floor(ppqEnd * stepsPerBeat));
        const int stepInPage = static_cast<int>((currentStep % kStepsPerPage + kStepsPerPage) % kStepsPerPage);
        track.currentStep = stepInPage;
        track.lastTriggeredStep = stepInPage;

        int64_t firstStep = static_cast<int64_t>(std::floor(ppqStart * stepsPerBeat)) - 1;
        const int64_t lastStep = static_cast<int64_t>(std::ceil(ppqEnd * stepsPerBeat)) + 1;
        if (lastStep - firstStep > 256) firstStep = lastStep - 256;

        for (int64_t absoluteStep = firstStep; absoluteStep <= lastStep && !full; ++absoluteStep) {
            const int patternStep = static_cast<int>((absoluteStep % kStepsPerPage + kStepsPerPage) % kStepsPerPage);
            const double stepStartPpq = static_cast<double>(absoluteStep) / stepsPerBeat;
            const auto page = getSongPageAtPpq(stepStartPpq);
            const auto& step = track.patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(patternStep)];
            if (!step.active || step.probability <= 0.0f) continue;

            uint32_t randomState = static_cast<uint32_t>(absoluteStep) ^ (static_cast<uint32_t>(trackIndex + 1) * 0x9e3779b9u);
            randomState ^= randomState >> 16;
            randomState *= 0x7feb352du;
            randomState ^= randomState >> 15;
            randomState *= 0x846ca68bu;
            randomState ^= randomState >> 16;
            const float randomValue = static_cast<float>(randomState >> 8) / 16777216.0f;
            if (step.probability < 1.0f && randomValue >= step.probability) continue;

            const double stepDuration = 1.0 / stepsPerBeat;
            const double swungOffset = (patternStep % 2 == 1) ? (swing_ + track.groove) : 0.0;
            const double stepOffset = static_cast<double>(step.timingOffset) + swungOffset + track.nudge;
            StepData scheduledStep = step;
            if (accentAmount_ > 0.0f && patternStep % accentEvery_ == 0)
                scheduledStep.velocity = std::clamp(step.velocity * (1.0f + accentAmount_), 0.0f, 1.0f);
            if (humanize_ > 0.0f) {
                uint32_t humState = randomState ^ 0x85ebca6bu;
                humState ^= humState >> 13;
                humState *= 0xc2b2ae35u;
                humState ^= humState >> 16;
                const float bipolar = (static_cast<float>(humState >> 8) / 8388608.0f) - 1.0f;
                scheduledStep.velocity = std::clamp(scheduledStep.velocity + bipolar * 0.25f * humanize_, 0.05f, 1.0f);
            }
            for (int ratchet = 0; ratchet < step.ratchets; ++ratchet) {
                const double eventPpq = stepStartPpq + (stepOffset + static_cast<double>(ratchet) / step.ratchets) * stepDuration;
                if (eventPpq < ppqStart || eventPpq >= ppqEnd) continue;
                if (scheduledHitCount_ >= kMaxScheduledHits) {
                    full = true;
                    break;
                }
                const int offset = static_cast<int>(std::floor((eventPpq - ppqStart) * samplesPerQuarter + 0.5));
                if (offset < 0 || offset >= numSamples) continue;
                scheduledHits_[scheduledHitCount_] = {offset, trackIndex, scheduledStep, scheduledHitCount_};
                ++scheduledHitCount_;
            }
        }
    }

    std::sort(scheduledHits_.begin(), scheduledHits_.begin() + scheduledHitCount_,
        [](const ScheduledHit& left, const ScheduledHit& right) {
            if (left.sampleOffset != right.sampleOffset) return left.sampleOffset < right.sampleOffset;
            return left.sequence < right.sequence;
        });
}

void FruitEngine::triggerScheduledHit(const ScheduledHit& hit) {
    auto& target = tracks_[static_cast<size_t>(hit.trackIndex)];
    auto sampleToPlay = target.auditionSample ? target.auditionSample : target.sample;
    if (!sampleToPlay) return;

    if (hit.step.chokeGroup > 0) {
        for (auto& track : tracks_)
            for (auto& voice : track.voices)
                if (voice.isPlaying() && voice.chokeGroup() == hit.step.chokeGroup) voice.choke();
    }

    VoiceSettings settings;
    settings.sampleStart = hit.step.sampleStart;
    settings.sampleEnd = hit.step.sampleEnd;
    settings.reverse = hit.step.reverse;
    settings.chokeGroup = hit.step.chokeGroup;
    target.triggerVoice(sampleToPlay, hit.step.velocity, hit.step.semitoneOffset, sampleRate_, settings);
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
    scheduledHitCount_ = 0;

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

        scheduleSequencer(ppqStart, ppqEnd, numSamples, effectiveBpm);
        internalPpq_ = ppqEnd;
        lastPpq_ = ppqStart;
    }

    if (isHostPlaying || isPlaying_.load()) {
        const double quarter = isHostPlaying ? (std::isfinite(ppqPosition) ? ppqPosition : visualPpq_) : lastPpq_;
        const double beatEnd = quarter + numSamples * ((bpm > 10.0 ? bpm : 120.0) / (60.0 * sampleRate_));
        visualPpq_ = beatEnd;
        const double bar = beatEnd * 0.25;
        displayBarPhase_.store(static_cast<float>(bar - std::floor(bar)), std::memory_order_relaxed);
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

    // Render each interval before applying events at its ending sample.
    const auto renderVoices = [&](int start, int end) {
        if (end <= start) return;
        for (int t = 0; t < activeTracks; ++t) {
            auto& track = tracks_[static_cast<size_t>(t)];
            if (track.mute || (anySolo && !track.solo)) continue;
            for (auto& voice : track.voices)
                if (voice.isPlaying()) voice.renderNextBlock(leftChannel + start, rightChannel + start, end - start);
        }
        if (previewVoice_.isPlaying())
            previewVoice_.renderNextBlock(leftChannel + start, rightChannel + start, end - start);
    };

    int renderPosition = 0;
    for (size_t i = 0; i < scheduledHitCount_; ++i) {
        const auto& hit = scheduledHits_[i];
        renderVoices(renderPosition, hit.sampleOffset);
        triggerScheduledHit(hit);
        renderPosition = hit.sampleOffset;
    }
    renderVoices(renderPosition, numSamples);

    // 4. Apply Master Volume and Soft Saturation
    const float mVol = masterVolume_;
    for (int i = 0; i < numSamples; ++i) {
        if (outputGainRampRemaining_ > 0) {
            outputGainCurrent_ += outputGainStep_;
            if (--outputGainRampRemaining_ == 0) {
                outputGainCurrent_ = outputGainTarget_;
                outputGainStep_ = 0.0f;
            }
        }
        leftChannel[i] = softClip(leftChannel[i] * mVol) * outputGainCurrent_;
        rightChannel[i] = softClip(rightChannel[i] * mVol) * outputGainCurrent_;
    }
}

} // namespace openfruit::core
