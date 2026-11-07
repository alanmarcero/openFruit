#pragma once

#include "SequencerTypes.h"
#include "SampleVoice.h"
#include "SonicAnalyzer.h"
#include <vector>
#include <array>
#include <memory>
#include <atomic>
#include <string>
#include <mutex>

namespace openfruit::core {

struct TrackState {
    int trackIndex = 0; // 0..15
    int midiChannel = 1; // 1..16
    std::string name;

    std::shared_ptr<const AudioSampleBuffer> sample;
    std::shared_ptr<const AudioSampleBuffer> auditionSample; // Live browsing audition buffer

    StepTiming timing = StepTiming::Timing1_16;
    int baseTune = 0; // -24 .. +24 semitones
    float volume = 1.0f; // 0.0 .. 1.5
    float pan = 0.0f; // -1.0 .. +1.0
    bool mute = false;
    bool solo = false;

    TrackPatternPages patternPages;

    // Playback voices (polyphonic pool for clean release overlap)
    static constexpr int kMaxVoicesPerTrack = 4;
    std::array<SampleVoice, kMaxVoicesPerTrack> voices;
    int nextVoiceIndex = 0;

    int currentStep = 0;
    int lastTriggeredStep = -1;

    void triggerVoice(std::shared_ptr<const AudioSampleBuffer> sampleToPlay,
                      float velocity,
                      int semitoneOffset,
                      double sampleRate);
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
    [[nodiscard]] TrackCountMode getTrackCountMode() const noexcept { return trackCountMode_; }
    [[nodiscard]] int getActiveTrackCount() const noexcept { return static_cast<int>(trackCountMode_); }

    void setInputControlMode(InputControlMode mode) noexcept;
    [[nodiscard]] InputControlMode getInputControlMode() const noexcept { return inputMode_; }

    void setActivePage(PageIndex page) noexcept;
    [[nodiscard]] PageIndex getActivePage() const noexcept { return activePage_; }

    // Track Access
    [[nodiscard]] TrackState& getTrack(int trackIndex);
    [[nodiscard]] const TrackState& getTrack(int trackIndex) const;

    // Load sample permanently to track
    void loadSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample);

    // Set audition sample for live browsing / live MIDI auditioning
    void setAuditionSampleForTrack(int trackIndex, std::shared_ptr<const AudioSampleBuffer> sample);
    void clearAuditionSampleForTrack(int trackIndex);
    void commitAuditionSample(int trackIndex);

    // Step Sequencer Editing
    void setStepActive(int trackIndex, PageIndex page, int stepIndex, bool active);
    void setStepVelocity(int trackIndex, PageIndex page, int stepIndex, float velocity);
    void setStepTune(int trackIndex, PageIndex page, int stepIndex, int semitoneOffset);
    [[nodiscard]] StepData getStep(int trackIndex, PageIndex page, int stepIndex) const;

    // Global volume
    void setMasterVolume(float vol) noexcept { masterVolume_ = vol; }
    [[nodiscard]] float getMasterVolume() const noexcept { return masterVolume_; }

    // Color theme
    void setColorTheme(ColorTheme theme) noexcept { colorTheme_ = theme; }
    [[nodiscard]] ColorTheme getColorTheme() const noexcept { return colorTheme_; }

    // Playhead visualization step for a track (0..15)
    [[nodiscard]] int getCurrentStep(int trackIndex) const;

private:
    double sampleRate_ = 44100.0;
    float masterVolume_ = 1.0f;
    TrackCountMode trackCountMode_ = TrackCountMode::Tracks16;
    InputControlMode inputMode_ = InputControlMode::SequencerPads;
    PageIndex activePage_ = PageIndex::PageA;
    ColorTheme colorTheme_ = ColorTheme::CyberpunkNeon;

    mutable std::mutex engineMutex_;
    std::array<TrackState, kMaxTracks> tracks_;

    double internalPpq_ = 0.0;
    double lastPpq_ = 0.0;

    // Audition preview voice (standalone preview button)
    SampleVoice previewVoice_;

    void advanceSequencer(double ppqStart, double ppqEnd, int numSamples, double bpm);
};

} // namespace openfruit::core
