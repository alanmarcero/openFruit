#include "FruitEngine.h"
#include <algorithm>
#include <cmath>

namespace openfruit::core {

namespace {

constexpr double kTwoPi = 6.28318530717958647692;

float softClip(float x) noexcept {
    const float s = std::tanh(x);
    return s + 0.03f * s * std::abs(s);
}

uint32_t mixHash(uint32_t x) noexcept {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

double clampedUnitGaussian(uint32_t seed) noexcept {
    const uint32_t h1 = mixHash(seed);
    const uint32_t h2 = mixHash(seed ^ 0x9e3779b9u);
    const double u1 = std::max(1.0e-7, (static_cast<double>(h1 >> 8) + 0.5) / 16777216.0);
    const double u2 = static_cast<double>(h2 >> 8) / 16777216.0;
    const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(kTwoPi * u2);
    return std::clamp(z / 3.0, -1.0, 1.0);
}

float softKneeLimiterGain(float peak) noexcept {
    constexpr float kCeilingDb = -0.1f;
    constexpr float kHalfKneeDb = 1.2f;
    constexpr float kKneeStartDb = kCeilingDb - kHalfKneeDb;
    constexpr float kKneeEndDb = kCeilingDb + kHalfKneeDb;
    if (peak <= 0.86099f) {
        return 1.0f;
    }
    const float peakDb = 20.0f * std::log10(std::max(peak, 1.0e-6f));
    if (peakDb <= kKneeStartDb) {
        return 1.0f;
    }
    if (peakDb < kKneeEndDb) {
        const float delta = peakDb - kKneeStartDb;
        const float gainDb = -(delta * delta) / (4.0f * kHalfKneeDb);
        return std::pow(10.0f, gainDb * 0.05f);
    }
    return std::pow(10.0f, (kCeilingDb - peakDb) * 0.05f);
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
    settings.filterResonance = filterResonance;
    settings.filterKeyTrack = filterKeyTrack;
    settings.drive = drive;
    settings.keyNormalizePitch = settings.keyNormalizePitch || keyNormalizePitch;
    settings.attackSeconds = amplitudeAttackSeconds;
    settings.decaySeconds = amplitudeDecaySeconds;
    settings.sustainLevel = amplitudeSustain;
    settings.releaseSeconds = amplitudeReleaseSeconds;
    settings.midiNote = midiNote;

    int chosenIndex = -1;
    for (int i = 0; i < kMaxVoicesPerTrack; ++i) {
        const int candidate = (nextVoiceIndex + i) % kMaxVoicesPerTrack;
        if (!voices[static_cast<size_t>(candidate)].isPlaying()
            && !voices[static_cast<size_t>(candidate)].hasActiveTail()) {
            chosenIndex = candidate;
            break;
        }
    }
    if (chosenIndex < 0) {
        for (int i = 0; i < kMaxVoicesPerTrack; ++i) {
            const int candidate = (nextVoiceIndex + i) % kMaxVoicesPerTrack;
            if (!voices[static_cast<size_t>(candidate)].isPlaying()) {
                chosenIndex = candidate;
                break;
            }
        }
    }
    if (chosenIndex < 0) {
        chosenIndex = nextVoiceIndex;
    }

    voices[static_cast<size_t>(chosenIndex)].start(
        sampleToPlay,
        velocity,
        semitoneOffset,
        baseTune,
        sampleRate,
        volume,
        pan,
        settings
    );
    nextVoiceIndex = (chosenIndex + 1) % kMaxVoicesPerTrack;
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
    publishPattern();
}

void FruitEngine::prepare(double sampleRate, int /*maxBlockSize*/) {
    sampleRate_ = std::max(8000.0, sampleRate);
    masterVolumeSmoothAlpha_ = 1.0f - std::exp(-1.0f / std::max(1.0f, static_cast<float>(sampleRate_ * 0.005)));
    limiterAttackCoeff_ = std::exp(-1.0f / std::max(1.0f, static_cast<float>(sampleRate_ * 0.0006)));
    limiterReleaseCoeff_ = std::exp(-1.0f / std::max(1.0f, static_cast<float>(sampleRate_ * 0.050)));
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
    for (auto& track : tracks_) {
        for (auto& voice : track.voices) {
            voice.stop();
        }
        track.currentStep = 0;
        track.lastTriggeredStep = -1;
    }
    previewVoice_.stop();
    const float initialMasterVol = getMasterVolume();
    masterVolumeCurrent_ = std::isfinite(initialMasterVol) ? std::clamp(initialMasterVol, 0.0f, 2.0f) : 1.0f;
    limiterEnvGain_ = 1.0f;
    masterUp1L_ = {};
    masterUp1R_ = {};
    masterUp2L_ = {};
    masterUp2R_ = {};
    masterDown2L_ = {};
    masterDown2R_ = {};
    masterDown1L_ = {};
    masterDown1R_ = {};
    visualPpq_ = 0.0;
    displayBarPhase_.store(0.0f, std::memory_order_relaxed);
    internalPpq_ = 0.0;
    lastPpq_ = 0.0;
    manualStart_.store(false, std::memory_order_relaxed);
    transportReset_.store(false, std::memory_order_relaxed);
    wasHostPlaying_ = false;
}

void FruitEngine::setTrackCountMode(TrackCountMode mode) noexcept {
    trackCountMode_.store(static_cast<int>(mode), std::memory_order_relaxed);
}

void FruitEngine::setPlaying(bool playing) noexcept {
    const bool wasPlaying = isPlaying_.load(std::memory_order_relaxed);
    if ((playing && !wasPlaying) || !playing)
        transportReset_.store(true, std::memory_order_release);
    manualStart_.store(playing, std::memory_order_release);
    isPlaying_.store(playing, std::memory_order_release);
    hostSyncArmed_.store(playing, std::memory_order_release);
}

void FruitEngine::setTrackMidiChannel(int trackIndex, int channel1to16) noexcept {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
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
    return tracks_[static_cast<size_t>(trackIndex)].midiChannel;
}

void FruitEngine::setActivePage(PageIndex page) noexcept {
    activePage_.store(static_cast<int>(page), std::memory_order_relaxed);
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
    return getTrack(trackIndex).sample;
}

void FruitEngine::loadSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    liveSample_[static_cast<size_t>(trackIndex)].store(sample, std::memory_order_release);
    tracks_[static_cast<size_t>(trackIndex)].sample = std::move(sample);
}

void FruitEngine::setAuditionSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    liveAudition_[static_cast<size_t>(trackIndex)].store(sample, std::memory_order_release);
    tracks_[static_cast<size_t>(trackIndex)].auditionSample = std::move(sample);
}

void FruitEngine::clearAuditionSampleForTrack(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    liveAudition_[static_cast<size_t>(trackIndex)].store({}, std::memory_order_release);
    tracks_[static_cast<size_t>(trackIndex)].auditionSample.reset();
}

void FruitEngine::commitAuditionSample(int trackIndex) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks) {
        return;
    }
    auto& track = tracks_[static_cast<size_t>(trackIndex)];
    if (track.auditionSample) {
        liveSample_[static_cast<size_t>(trackIndex)].store(track.auditionSample, std::memory_order_release);
        track.sample = track.auditionSample;
        liveAudition_[static_cast<size_t>(trackIndex)].store({}, std::memory_order_release);
        track.auditionSample.reset();
    }
}

void FruitEngine::publishPattern() {
    if (patternBatch_ > 0) return;
    auto snapshot = std::make_shared<PatternSnapshot>();
    for (int track = 0; track < kMaxTracks; ++track)
        snapshot->tracks[static_cast<size_t>(track)] = tracks_[static_cast<size_t>(track)].patternPages;
    pattern_.store(std::move(snapshot), std::memory_order_release);
}

void FruitEngine::beginPatternBatch() noexcept { ++patternBatch_; }

void FruitEngine::endPatternBatch() {
    if (patternBatch_ > 0) --patternBatch_;
    if (patternBatch_ == 0) publishPattern();
}

std::shared_ptr<const AudioSampleBuffer> FruitEngine::audioSampleForTrack(int trackIndex) const {
    if (auto audition = liveAudition_[static_cast<size_t>(trackIndex)].load(std::memory_order_acquire))
        return audition;
    return liveSample_[static_cast<size_t>(trackIndex)].load(std::memory_order_acquire);
}

void FruitEngine::drainManualHits() {
    int read = manualRead_.load(std::memory_order_relaxed);
    const int write = manualWrite_.load(std::memory_order_acquire);
    while (read != write) {
        const auto hit = manualQueue_[static_cast<size_t>(read)];
        read = (read + 1) % kManualQueue;
        if (hit.track >= 0 && hit.track < kMaxTracks) {
            auto& track = tracks_[static_cast<size_t>(hit.track)];
            if (auto sample = audioSampleForTrack(hit.track))
                track.triggerVoice(sample, hit.velocity, hit.semitone, sampleRate_);
        }
    }
    manualRead_.store(read, std::memory_order_release);
}

void FruitEngine::setStepActive(int trackIndex, PageIndex page, int stepIndex, bool active) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].active = active;
    publishPattern();
}

void FruitEngine::setStepVelocity(int trackIndex, PageIndex page, int stepIndex, float velocity) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].velocity =
        std::clamp(velocity, 0.0f, 1.0f);
    publishPattern();
}

void FruitEngine::setStepTune(int trackIndex, PageIndex page, int stepIndex, int semitoneOffset) {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return;
    }
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(page)][static_cast<size_t>(stepIndex)].semitoneOffset =
        std::clamp(semitoneOffset, kMinSemitoneTune, kMaxSemitoneTune);
    publishPattern();
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
    tracks_[static_cast<size_t>(trackIndex)].patternPages.pages[static_cast<size_t>(pageIndex)][static_cast<size_t>(stepIndex)] = data;
    publishPattern();
}

void FruitEngine::clearStepPatterns() {
    for (auto& track : tracks_) track.patternPages = TrackPatternPages{};
    publishPattern();
}

StepData FruitEngine::getStep(int trackIndex, PageIndex page, int stepIndex) const {
    if (trackIndex < 0 || trackIndex >= kMaxTracks || stepIndex < 0 || stepIndex >= kStepsPerPage) {
        return {};
    }
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
    if (!songMode_ || !std::isfinite(ppq) || ppq < 0.0) return getActivePage();
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
    const int write = manualWrite_.load(std::memory_order_relaxed);
    const int next = (write + 1) % kManualQueue;
    if (next == manualRead_.load(std::memory_order_acquire)) return;
    manualQueue_[static_cast<size_t>(write)] = { trackIndex, velocity, semitoneOffset };
    manualWrite_.store(next, std::memory_order_release);
}

void FruitEngine::handleMidiNoteOn(int channel1to16, int midiNote, float velocity) {
    if (channel1to16 < 1 || channel1to16 > 16) {
        return;
    }

    const int activeTracks = getActiveTrackCount();
    const int semitoneOffset = std::clamp(midiNote - 60, kMinSemitoneTune, kMaxSemitoneTune);

    for (int t = 0; t < activeTracks; ++t) {
        auto& track = tracks_[static_cast<size_t>(t)];
        if (track.midiChannel != channel1to16) {
            continue;
        }
        if (auto sampleToPlay = audioSampleForTrack(t))
            track.triggerVoice(sampleToPlay, velocity, semitoneOffset, sampleRate_, {}, midiNote);
    }
}

void FruitEngine::handleMidiNoteOff(int channel1to16, int midiNote) {
    if (channel1to16 < 1 || channel1to16 > 16 || midiNote < 0 || midiNote > 127) return;
    for (auto& track : tracks_) {
        if (track.midiChannel != channel1to16) continue;
        for (auto& voice : track.voices)
            if (voice.isPlaying() && voice.midiNote() == midiNote) voice.noteOff();
    }
}

void FruitEngine::scheduleSequencer(double ppqStart, double ppqEnd, int numSamples, double bpm) {
    scheduledHitCount_ = 0;
    const auto pattern = pattern_.load(std::memory_order_acquire);
    if (pattern == nullptr) return;
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

        int64_t firstStep = static_cast<int64_t>(std::floor(ppqStart * stepsPerBeat)) - 2;
        const int64_t lastStep = static_cast<int64_t>(std::ceil(ppqEnd * stepsPerBeat)) + 2;
        if (lastStep - firstStep > 256) firstStep = lastStep - 256;

        for (int64_t absoluteStep = firstStep; absoluteStep <= lastStep && !full; ++absoluteStep) {
            const int patternStep = static_cast<int>((absoluteStep % kStepsPerPage + kStepsPerPage) % kStepsPerPage);
            const double stepStartPpq = static_cast<double>(absoluteStep) / stepsPerBeat;
            const auto page = getSongPageAtPpq(stepStartPpq);
            const auto& step = pattern->tracks[static_cast<size_t>(trackIndex)].pages[static_cast<size_t>(page)][static_cast<size_t>(patternStep)];
            if (!step.active || step.probability <= 0.0f) continue;

            const uint32_t randomState = mixHash(
                static_cast<uint32_t>(absoluteStep) ^ (static_cast<uint32_t>(trackIndex + 1) * 0x9e3779b9u));
            const float randomValue = static_cast<float>(randomState >> 8) / 16777216.0f;
            if (step.probability < 1.0f && randomValue >= step.probability) continue;

            const double stepDuration = 1.0 / stepsPerBeat;
            double swungOffset = 0.0;
            if (patternStep % 2 == 1) {
                swungOffset = static_cast<double>(swing_ + track.groove);
            }
            const double stepOffset = static_cast<double>(step.timingOffset) + swungOffset + track.nudge;
            StepData baseScheduledStep = step;
            if (accentAmount_ > 0.0f && patternStep % accentEvery_ == 0)
                baseScheduledStep.velocity = std::clamp(step.velocity * (1.0f + accentAmount_), 0.0f, 1.0f);

            for (int ratchet = 0; ratchet < step.ratchets; ++ratchet) {
                const double baseEventPpq = stepStartPpq + (stepOffset + static_cast<double>(ratchet) / step.ratchets) * stepDuration;
                if (ppqStart <= 0.0 && baseEventPpq < -1.0e-9) continue;

                StepData ratchetStep = baseScheduledStep;
                double timingJitterSamples = 0.0;
                float pitchJitterCents = 0.0f;
                if (humanize_ > 0.0f) {
                    const uint32_t hitSeed = mixHash(randomState ^ (static_cast<uint32_t>(ratchet + 1) * 0x85ebca6bu));
                    const float velGauss = static_cast<float>(clampedUnitGaussian(hitSeed));
                    const double timeGauss = clampedUnitGaussian(hitSeed ^ 0xc2b2ae35u);
                    const float centGauss = static_cast<float>(clampedUnitGaussian(hitSeed ^ 0x27d4eb2fu));
                    ratchetStep.velocity = std::clamp(ratchetStep.velocity + velGauss * 0.25f * humanize_, 0.05f, 1.0f);
                    timingJitterSamples = timeGauss * 0.003 * sampleRate_ * static_cast<double>(humanize_);
                    pitchJitterCents = centGauss * 12.0f * humanize_;
                }

                double exactOffset = (baseEventPpq - ppqStart) * samplesPerQuarter + timingJitterSamples;
                if (ppqStart <= 0.0 && baseEventPpq >= -1.0e-9 && exactOffset < 0.0) {
                    exactOffset = 0.0;
                }
                const double ceiled = std::ceil(exactOffset - 1.0e-9);
                if (ceiled < 0.0 || ceiled >= static_cast<double>(numSamples)) continue;
                if (scheduledHitCount_ >= kMaxScheduledHits) {
                    full = true;
                    break;
                }
                const int offset = static_cast<int>(ceiled);
                const double frac = std::clamp(ceiled - exactOffset, 0.0, 0.999999);
                scheduledHits_[scheduledHitCount_] = {
                    offset, trackIndex, ratchetStep, frac, pitchJitterCents, scheduledHitCount_
                };
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
    auto sampleToPlay = audioSampleForTrack(hit.trackIndex);
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
    settings.subSampleOffset = hit.subSampleOffset;
    settings.fineTuneCents = hit.fineTuneCents;
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

    scheduledHitCount_ = 0;
    drainManualHits();

    // 1. Handle host transport start/stop transitions
    if (manualStart_.exchange(false, std::memory_order_acq_rel))
        wasHostPlaying_ = isHostPlaying;
    if (transportReset_.exchange(false, std::memory_order_acq_rel)) {
        for (auto& track : tracks_)
            track.lastTriggeredStep = -1;
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
            for (auto& voice : track.voices) {
                if (!voice.isPlaying() && !voice.hasActiveTail()) continue;
                voice.updateFilterSettings(track.filterCutoffHz, track.highPassCutoffHz, track.filterResonance, track.filterKeyTrack);
                voice.renderNextBlock(leftChannel + start, rightChannel + start, end - start);
            }
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

    // Smooth masterVolume per sample, then run 2 ms lookahead soft-knee limiter + bus clipper.
    const float rawTargetVol = getMasterVolume();
    const float targetMasterVol = std::isfinite(rawTargetVol) ? std::clamp(rawTargetVol, 0.0f, 2.0f) : 1.0f;
    for (int i = 0; i < numSamples; ++i) {
        masterVolumeCurrent_ += masterVolumeSmoothAlpha_ * (targetMasterVol - masterVolumeCurrent_);
        leftChannel[i] *= masterVolumeCurrent_;
        rightChannel[i] *= masterVolumeCurrent_;
    }

    constexpr size_t kDequeMask = 511;
    std::array<int, 512> lookaheadIdx{};
    std::array<float, 512> lookaheadVal{};
    size_t dequeHead = 0;
    size_t dequeSize = 0;
    const int lookaheadSamples = std::clamp(static_cast<int>(std::round(0.002 * sampleRate_)), 1, 500);
    int scanPos = 0;
    const bool oversampleBus = getMasterOversampling();

    for (int i = 0; i < numSamples; ++i) {
        const int maxWindowIdx = std::min(numSamples - 1, i + lookaheadSamples);
        while (scanPos <= maxWindowIdx) {
            const float p = std::max(std::abs(leftChannel[scanPos]), std::abs(rightChannel[scanPos]));
            while (dequeSize > 0) {
                const size_t backSlot = (dequeHead + dequeSize - 1) & kDequeMask;
                if (lookaheadVal[backSlot] > p) {
                    break;
                }
                --dequeSize;
            }
            const size_t pushSlot = (dequeHead + dequeSize) & kDequeMask;
            lookaheadIdx[pushSlot] = scanPos;
            lookaheadVal[pushSlot] = p;
            ++dequeSize;
            ++scanPos;
        }
        while (dequeSize > 0 && lookaheadIdx[dequeHead] < i) {
            dequeHead = (dequeHead + 1) & kDequeMask;
            --dequeSize;
        }

        const float instantPeak = std::max(std::abs(leftChannel[i]), std::abs(rightChannel[i]));
        float windowPeak = instantPeak;
        if (dequeSize > 0) {
            windowPeak = std::max(windowPeak, lookaheadVal[dequeHead]);
        }
        const float targetLimiterGain = softKneeLimiterGain(windowPeak);
        float coeff = limiterReleaseCoeff_;
        if (targetLimiterGain < limiterEnvGain_) {
            coeff = limiterAttackCoeff_;
        }
        limiterEnvGain_ = targetLimiterGain + coeff * (limiterEnvGain_ - targetLimiterGain);
        const float limGain = std::min(limiterEnvGain_, softKneeLimiterGain(instantPeak));

        float busL = leftChannel[i] * limGain;
        float busR = rightChannel[i] * limGain;

        if (!oversampleBus) {
            busL = std::clamp(softClip(busL), -1.0f, 1.0f);
            busR = std::clamp(softClip(busR), -1.0f, 1.0f);
        }
        if (oversampleBus) {
            const auto up2L = masterUp1L_.upsample(busL);
            const auto up2R = masterUp1R_.upsample(busR);

            std::array<float, 2> dn2L{};
            std::array<float, 2> dn2R{};
            for (size_t s = 0; s < 2; ++s) {
                const auto u4L = masterUp2L_.upsample(up2L[s]);
                const float s4L0 = std::clamp(softClip(u4L[0]), -1.0f, 1.0f);
                const float s4L1 = std::clamp(softClip(u4L[1]), -1.0f, 1.0f);
                dn2L[s] = masterDown2L_.downsample(s4L0, s4L1);

                const auto u4R = masterUp2R_.upsample(up2R[s]);
                const float s4R0 = std::clamp(softClip(u4R[0]), -1.0f, 1.0f);
                const float s4R1 = std::clamp(softClip(u4R[1]), -1.0f, 1.0f);
                dn2R[s] = masterDown2R_.downsample(s4R0, s4R1);
            }

            busL = std::clamp(masterDown1L_.downsample(dn2L[0], dn2L[1]), -1.0f, 1.0f);
            busR = std::clamp(masterDown1R_.downsample(dn2R[0], dn2R[1]), -1.0f, 1.0f);
        }

        if (outputGainRampRemaining_ > 0) {
            outputGainCurrent_ += outputGainStep_;
            if (--outputGainRampRemaining_ == 0) {
                outputGainCurrent_ = outputGainTarget_;
                outputGainStep_ = 0.0f;
            }
        }
        leftChannel[i] = busL * outputGainCurrent_;
        rightChannel[i] = busR * outputGainCurrent_;
    }
}

} // namespace openfruit::core
