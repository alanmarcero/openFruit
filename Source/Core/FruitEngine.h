#pragma once

#include "SequencerTypes.h"
#include "SampleVoice.h"
#include "SonicAnalyzer.h"
#include <algorithm>
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <string>

namespace openfruit::core {

// Cross-thread flags and counters. The audio callback only loads or stores these;
// it does not take a mutex.
struct RelaxedFlag {
    std::atomic<bool> value { false };
    RelaxedFlag() = default;
    explicit RelaxedFlag(bool initial) noexcept : value(initial) {}
    RelaxedFlag(const RelaxedFlag& other) noexcept : value(other.value.load(std::memory_order_relaxed)) {}
    RelaxedFlag& operator=(const RelaxedFlag& other) noexcept {
        value.store(other.value.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
    RelaxedFlag& operator=(bool next) noexcept {
        value.store(next, std::memory_order_relaxed);
        return *this;
    }
    operator bool() const noexcept { return value.load(std::memory_order_relaxed); }
};

struct RelaxedInt {
    std::atomic<int> value { 0 };
    RelaxedInt() = default;
    explicit RelaxedInt(int initial) noexcept : value(initial) {}
    RelaxedInt(const RelaxedInt& other) noexcept : value(other.value.load(std::memory_order_relaxed)) {}
    RelaxedInt& operator=(const RelaxedInt& other) noexcept {
        value.store(other.value.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
    RelaxedInt& operator=(int next) noexcept {
        value.store(next, std::memory_order_relaxed);
        return *this;
    }
    operator int() const noexcept { return value.load(std::memory_order_relaxed); }
};

template <typename T>
struct AtomicShared {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<std::shared_ptr<T>> value;
    std::shared_ptr<T> load(std::memory_order order) const noexcept { return value.load(order); }
    void store(std::shared_ptr<T> next, std::memory_order order) noexcept { value.store(std::move(next), order); }
#else
    std::shared_ptr<T> value;
    std::shared_ptr<T> load(std::memory_order order) const noexcept {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        return std::atomic_load_explicit(&value, order);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    }
    void store(std::shared_ptr<T> next, std::memory_order order) noexcept {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        std::atomic_store_explicit(&value, std::move(next), order);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    }
#endif
};

struct TrackState {
    int trackIndex = 0; // 0..15
    int midiChannel = 1; // 1..16
    std::string name;

    std::shared_ptr<const AudioSampleBuffer> sample;
    std::shared_ptr<const AudioSampleBuffer> auditionSample; // Live browsing audition buffer

    StepTiming timing = StepTiming::Timing1_16;
    float nudge = 0.0f; // -0.5 .. +0.5 of a step
    float groove = 0.0f; // additional delay on odd steps, 0 .. 0.5 of a step
    int baseTune = 0; // -24 .. +24 semitones
    float volume = 1.0f; // 0.0 .. 1.5
    float pan = 0.0f; // -1.0 .. +1.0
    RelaxedFlag mute;
    RelaxedFlag solo;
    float filterCutoffHz = 20000.0f;
    float highPassCutoffHz = 20.0f;
    float filterResonance = 0.0f;
    float filterKeyTrack = 0.0f;
    float drive = 0.0f;
    bool keyNormalizePitch = false;
    float amplitudeAttackSeconds = 0.0f;
    float amplitudeDecaySeconds = 0.0f;
    float amplitudeSustain = 1.0f;
    float amplitudeReleaseSeconds = 0.0f;

    TrackPatternPages patternPages;

    // Playback voices (polyphonic pool for clean release overlap)
    static constexpr int kMaxVoicesPerTrack = 4;
    std::array<SampleVoice, kMaxVoicesPerTrack> voices;
    int nextVoiceIndex = 0;

    RelaxedInt currentStep;
    int lastTriggeredStep = -1;

    void triggerVoice(std::shared_ptr<const AudioSampleBuffer> sampleToPlay,
                      float velocity,
                      int semitoneOffset,
                      double sampleRate,
                      VoiceSettings settings = {},
                      int midiNote = -1);
};

class FruitEngine {
public:
    FruitEngine();
    ~FruitEngine() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Process audio block and MIDI input
    void process(float* leftChannel,
                 float* rightChannel,
                 int numSamples,
                 double bpm,
                 double ppqPosition,
                 bool isHostPlaying);

    // Manual or MIDI note triggers
    void handleMidiNoteOn(int channel1to16, int midiNote, float velocity);
    void handleMidiNoteOff(int channel1to16, int midiNote);
    void triggerTrackManual(int trackIndex, float velocity = 0.85f, int semitoneOffset = 0);

    // Track Configuration
    void setTrackCountMode(TrackCountMode mode) noexcept;
    [[nodiscard]] TrackCountMode getTrackCountMode() const noexcept {
        return static_cast<TrackCountMode>(trackCountMode_.load(std::memory_order_relaxed));
    }
    [[nodiscard]] int getActiveTrackCount() const noexcept { return static_cast<int>(getTrackCountMode()); }

    // Sequencer Transport Control (Play / Pause with host sync)
    void setPlaying(bool playing) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_.load(); }

    [[nodiscard]] float displayBarPhase() const noexcept { return displayBarPhase_.load(std::memory_order_relaxed); }

    // Per-Track MIDI Channel (1..16)
    void setTrackMidiChannel(int trackIndex, int channel1to16) noexcept;
    [[nodiscard]] int getTrackMidiChannel(int trackIndex) const noexcept;

    void setActivePage(PageIndex page) noexcept;
    [[nodiscard]] PageIndex getActivePage() const noexcept {
        return static_cast<PageIndex>(activePage_.load(std::memory_order_relaxed));
    }

    // Track Access
    [[nodiscard]] TrackState& getTrack(int trackIndex);
    [[nodiscard]] const TrackState& getTrack(int trackIndex) const;

    // Load sample permanently to track
    [[nodiscard]] std::shared_ptr<const AudioSampleBuffer> loadedSample(int trackIndex) const;
    void loadSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample);

    // Set audition sample for live browsing / live MIDI auditioning
    void setAuditionSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample);
    void clearAuditionSampleForTrack(int trackIndex);
    void commitAuditionSample(int trackIndex);

    // Step Sequencer Editing
    void setStepActive(int trackIndex, PageIndex page, int stepIndex, bool active);
    void setStepVelocity(int trackIndex, PageIndex page, int stepIndex, float velocity);
    void setStepTune(int trackIndex, PageIndex page, int stepIndex, int semitoneOffset);
    void setStepData(int trackIndex, PageIndex page, int stepIndex, StepData data);
    void clearStepPatterns();
    void beginPatternBatch() noexcept;
    void endPatternBatch();
    [[nodiscard]] StepData getStep(int trackIndex, PageIndex page, int stepIndex) const;
    void setSwing(float amount) noexcept { swing_ = std::clamp(amount, 0.0f, 0.5f); }
    [[nodiscard]] float getSwing() const noexcept { return swing_; }
    void setHumanize(float amount) noexcept { humanize_ = std::clamp(std::isfinite(amount) ? amount : 0.0f, 0.0f, 1.0f); }
    [[nodiscard]] float getHumanize() const noexcept { return humanize_; }
    void setTrackNudge(int trackIndex, float amount) noexcept;
    void setTrackGroove(int trackIndex, float amount) noexcept;
    void setAccentuator(float amount, int everySteps) noexcept;
    void setSongMode(bool enabled) noexcept { songMode_ = enabled; }
    [[nodiscard]] bool getSongMode() const noexcept { return songMode_; }
    void setSongSection(int index, SongSection section) noexcept;
    [[nodiscard]] SongSection getSongSection(int index) const noexcept;
    [[nodiscard]] PageIndex getSongPageAtPpq(double ppq) const noexcept;

    // Global volume
    void setMasterVolume(float vol) noexcept { masterVolume_.store(vol, std::memory_order_relaxed); }
    [[nodiscard]] float getMasterVolume() const noexcept { return masterVolume_.load(std::memory_order_relaxed); }
    void setOutputDb(float decibels) noexcept;
    void setMasterOversampling(bool enabled) noexcept { masterOversampling_.store(enabled, std::memory_order_relaxed); }
    [[nodiscard]] bool getMasterOversampling() const noexcept { return masterOversampling_.load(std::memory_order_relaxed); }

    // Color theme
    void setColorTheme(ColorTheme theme) noexcept { colorTheme_ = theme; }
    [[nodiscard]] ColorTheme getColorTheme() const noexcept { return colorTheme_; }

    // Playhead visualization step for a track (0..15)
    [[nodiscard]] int getCurrentStep(int trackIndex) const;

private:
    struct PatternSnapshot {
        std::array<TrackPatternPages, kMaxTracks> tracks;
    };
    struct ManualHit {
        int track = 0;
        float velocity = 0.85f;
        int semitone = 0;
    };

    double sampleRate_ = 44100.0;
    std::atomic<float> masterVolume_ { 1.0f };
    std::atomic<bool> masterOversampling_ { false };
    float masterVolumeCurrent_ = 1.0f;
    float masterVolumeSmoothAlpha_ = 0.005f;
    float limiterEnvGain_ = 1.0f;
    float limiterAttackCoeff_ = 0.96f;
    float limiterReleaseCoeff_ = 0.9995f;
    Halfband2x masterUp1L_{};
    Halfband2x masterUp1R_{};
    Halfband2x masterUp2L_{};
    Halfband2x masterUp2R_{};
    Halfband2x masterDown2L_{};
    Halfband2x masterDown2R_{};
    Halfband2x masterDown1L_{};
    Halfband2x masterDown1R_{};
    float outputGainCurrent_ = 1.0f;
    float outputGainTarget_ = 1.0f;
    float outputGainStep_ = 0.0f;
    int outputGainRampRemaining_ = 0;
    std::atomic<int> trackCountMode_ { static_cast<int>(TrackCountMode::Tracks8) };
    std::atomic<bool> isPlaying_{ false };
    std::atomic<bool> hostSyncArmed_{ false };
    std::atomic<bool> manualStart_ { false };
    std::atomic<bool> transportReset_ { false };
    bool wasHostPlaying_ = false;
    std::atomic<int> activePage_ { 0 };
    float swing_ = 0.0f;
    float humanize_ = 0.0f;
    float accentAmount_ = 0.0f;
    int accentEvery_ = 4;
    bool songMode_ = false;
    std::array<SongSection, kNumPages> songSections_ {};
    ColorTheme colorTheme_ = ColorTheme::CyberpunkNeon;

    std::array<TrackState, kMaxTracks> tracks_;
    std::array<AtomicShared<const AudioSampleBuffer>, kMaxTracks> liveSample_;
    std::array<AtomicShared<const AudioSampleBuffer>, kMaxTracks> liveAudition_;
    AtomicShared<const PatternSnapshot> pattern_;
    int patternBatch_ = 0;

    static constexpr int kManualQueue = 64;
    std::array<ManualHit, kManualQueue> manualQueue_ {};
    std::atomic<int> manualWrite_ { 0 };
    std::atomic<int> manualRead_ { 0 };

    void publishPattern();
    void drainManualHits();
    [[nodiscard]] std::shared_ptr<const AudioSampleBuffer> audioSampleForTrack(int trackIndex) const;

    struct ScheduledHit {
        int sampleOffset = 0;
        int trackIndex = 0;
        StepData step;
        double subSampleOffset = 0.0;
        float fineTuneCents = 0.0f;
        size_t sequence = 0;
    };
    static constexpr size_t kMaxScheduledHits = 4096;
    std::array<ScheduledHit, kMaxScheduledHits> scheduledHits_ {};
    size_t scheduledHitCount_ = 0;

    std::atomic<float> displayBarPhase_ {0.0f};
    double visualPpq_ = 0.0;
    double internalPpq_ = 0.0;
    double lastPpq_ = 0.0;

    // Audition preview voice (standalone preview button)
    SampleVoice previewVoice_;

    void scheduleSequencer(double ppqStart, double ppqEnd, int numSamples, double bpm);
    void triggerScheduledHit(const ScheduledHit& hit);
};

} // namespace openfruit::core
