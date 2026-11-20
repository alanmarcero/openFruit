#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "SonicAnalyzer.h"

namespace openfruit::core {

struct Halfband2x {
    static constexpr std::array<float, 3> kEvenCoeffs { 0.0798664262f, 0.5453536511f, 0.8800086975f };
    static constexpr std::array<float, 3> kOddCoeffs { 0.2838293449f, 0.7330289328f, 0.9653220863f };

    std::array<float, 3> evenX{};
    std::array<float, 3> evenY{};
    std::array<float, 3> oddX{};
    std::array<float, 3> oddY{};

    static float processAllpass(float input,
                                const std::array<float, 3>& coeffs,
                                std::array<float, 3>& stateX,
                                std::array<float, 3>& stateY) noexcept {
        float signal = input;
        for (size_t stage = 0; stage < 3; ++stage) {
            const float y = coeffs[stage] * (signal - stateY[stage]) + stateX[stage];
            stateX[stage] = signal;
            stateY[stage] = y;
            signal = y;
        }
        return signal;
    }

    std::array<float, 2> upsample(float input) noexcept {
        return {
            processAllpass(input, kEvenCoeffs, evenX, evenY),
            processAllpass(input, kOddCoeffs, oddX, oddY)
        };
    }

    float downsample(float first, float second) noexcept {
        const float dnEven = processAllpass(second, kEvenCoeffs, evenX, evenY);
        const float dnOdd = processAllpass(first, kOddCoeffs, oddX, oddY);
        return 0.5f * (dnEven + dnOdd);
    }
};

struct AudioSampleBuffer {
    struct MipLevel {
        int decimation = 1;
        int64_t numSamples = 0;
        std::vector<std::vector<float>> channels;
    };

    std::string filePath;
    std::string sampleName;
    int numChannels = 0;
    double sampleRate = 44100.0;
    int64_t numSamples = 0;
    std::vector<std::vector<float>> channels; // [ch][sample]
    std::vector<MipLevel> mipLevels;
    ToneAnalysisResult toneResult;
    bool isLoaded = false;

    void buildMipPyramid();
};

struct VoiceSettings {
    float sampleStart = 0.0f;
    float sampleEnd = 1.0f;
    bool reverse = false;
    float attackSeconds = 0.0f;
    float decaySeconds = 0.0f;
    float sustainLevel = 1.0f;
    float releaseSeconds = 0.0f;
    float filterCutoffHz = 20000.0f;
    float highPassCutoffHz = 20.0f;
    float filterResonance = 0.0f;
    float filterKeyTrack = 0.0f;
    float drive = 0.0f;
    float fineTuneCents = 0.0f;
    double subSampleOffset = 0.0;
    bool keyNormalizePitch = false;
    int chokeGroup = 0;
    int midiNote = -1;
};

class SampleVoice {
public:
    SampleVoice() = default;

    void start(std::shared_ptr<const AudioSampleBuffer> sample,
               float velocity,
               int semitoneOffset,
               int trackBaseTune,
               double hostSampleRate,
               float volumeGain = 1.0f,
               float pan = 0.0f,
               VoiceSettings settings = {});

    void stop();
    void noteOff() noexcept;
    void choke() noexcept;
    void updateFilterSettings(float lowPassCutoffHz,
                              float highPassCutoffHz,
                              float resonance = 0.0f,
                              float keyTrack = 0.0f) noexcept;
    [[nodiscard]] bool isPlaying() const noexcept { return isPlaying_; }
    [[nodiscard]] bool hasActiveTail() const noexcept { return tailPos_ < tailLength_; }
    [[nodiscard]] int chokeGroup() const noexcept { return chokeGroup_; }
    [[nodiscard]] int midiNote() const noexcept { return midiNote_; }
    [[nodiscard]] int activeMipDecimation() const noexcept { return mipDecimation_; }

    // Renders up to numFrames into stereo left/right buffers, adding to existing contents.
    void renderNextBlock(float* leftOut, float* rightOut, int numFrames);

private:
    enum class EnvelopeStage : uint8_t {
        Attack = 0,
        Decay = 1,
        Sustain = 2,
        Release = 3
    };

    struct SvfChannelState {
        float ic1 = 0.0f;
        float ic2 = 0.0f;
    };

    static constexpr int kMaxShadowTailFrames = 768;

    std::shared_ptr<const AudioSampleBuffer> sample_;
    double readPosition_ = 0.0;
    double basePitchFactor_ = 1.0;
    double speedRatio_ = 1.0;
    double hostSampleRate_ = 44100.0;

    // RC exponential ADSR state
    EnvelopeStage envStage_ = EnvelopeStage::Sustain;
    double envLevel_ = 1.0;
    double envTarget_ = 1.0;
    double attackCoeff_ = 0.0;
    double decayCoeff_ = 0.0;
    double releaseCoeff_ = 0.0;

    // Per-voice ZDF TPT SVF state (LPF + HPF)
    SvfChannelState lpfL_{};
    SvfChannelState lpfR_{};
    SvfChannelState hpfL_{};
    SvfChannelState hpfR_{};
    float lpfLogGCurrent_ = 0.0f;
    float lpfLogGTarget_ = 0.0f;
    float hpfLogGCurrent_ = 0.0f;
    float hpfLogGTarget_ = 0.0f;
    float svfKCurrent_ = 1.41421356f;
    float svfKTarget_ = 1.41421356f;
    float filterSmoothCoeff_ = 0.25f;
    float lastLpfCutoffHz_ = -1.0f;
    float lastHpfCutoffHz_ = -1.0f;
    float lastResonance_ = -1.0f;
    float lastKeyTrack_ = -1.0f;
    bool lpfTargetActive_ = false;
    bool hpfTargetActive_ = false;

    // 2x oversampled first-order ADAA drive state
    Halfband2x upBandL_{};
    Halfband2x upBandR_{};
    Halfband2x downBandL_{};
    Halfband2x downBandR_{};
    double adaaPrevL_ = 0.0;
    double adaaPrevR_ = 0.0;
    bool adaaPrimed_ = false;

    // De-click shadow tail for choke & voice steal
    std::vector<float> tailLeft_ = std::vector<float>(static_cast<size_t>(kMaxShadowTailFrames), 0.0f);
    std::vector<float> tailRight_ = std::vector<float>(static_cast<size_t>(kMaxShadowTailFrames), 0.0f);
    int tailPos_ = 0;
    int tailLength_ = 0;

    int64_t sampleStart_ = 0;
    int64_t sampleEnd_ = 0;
    int trimStartFadeSamples_ = 0;
    int trimEndFadeSamples_ = 0;
    int direction_ = 1;
    float gainLeft_ = 1.0f;
    float gainRight_ = 1.0f;
    float drive_ = 0.0f;
    float driveGain_ = 1.0f;
    float driveNorm_ = 1.0f;
    float attackSeconds_ = 0.0f;
    float decaySeconds_ = 0.0f;
    float sustainLevel_ = 1.0f;
    float releaseSeconds_ = 0.0f;
    int chokeGroup_ = 0;
    int midiNote_ = -1;
    int activeMipIndex_ = -1;
    int mipDecimation_ = 1;
    bool isPlaying_ = false;

    static float sampleSinc(const float* buffer,
                            int64_t length,
                            double pos,
                            double cutoffScale,
                            int64_t regionStart,
                            int64_t regionEnd,
                            int startFadeLen,
                            int endFadeLen) noexcept;
    [[nodiscard]] float envelopeLevel() const noexcept { return static_cast<float>(std::clamp(envLevel_, 0.0, 1.0)); }
    void advanceEnvelope() noexcept;
    void stopPrimaryVoice() noexcept;
    void renderPrimaryFrames(float* leftOut, float* rightOut, int numFrames, bool applyWindowFade) noexcept;
};

} // namespace openfruit::core
