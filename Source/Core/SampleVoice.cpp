#include "SampleVoice.h"
#include <algorithm>
#include <cmath>

namespace openfruit::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

float SampleVoice::interpolateCubic(const float* buffer, int64_t length, double pos) noexcept {
    const int64_t i = static_cast<int64_t>(std::floor(pos));
    const float frac = static_cast<float>(pos - static_cast<double>(i));

    const auto getSample = [buffer, length](int64_t idx) -> float {
        if (idx < 0) return 0.0f;
        if (idx >= length) return 0.0f;
        return buffer[idx];
    };

    const float y0 = getSample(i - 1);
    const float y1 = getSample(i);
    const float y2 = getSample(i + 1);
    const float y3 = getSample(i + 2);

    const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float a2 = -0.5f * y0 + 0.5f * y2;
    const float a3 = y1;

    return ((a0 * frac + a1) * frac + a2) * frac + a3;
}

void SampleVoice::start(std::shared_ptr<const AudioSampleBuffer> sample,
                        float velocity,
                        int semitoneOffset,
                        int trackBaseTune,
                        double hostSampleRate,
                        float volumeGain,
                        float pan) {
    if (!sample || !sample->isLoaded || sample->numSamples <= 0 || hostSampleRate <= 1000.0) {
        isPlaying_ = false;
        return;
    }

    sample_ = std::move(sample);
    readPosition_ = 0.0;
    isPlaying_ = true;

    // Total semitones: clamped to -48 .. +48 safe musical bounds
    const int totalSemitones = std::clamp(semitoneOffset + trackBaseTune, -48, 48);
    const double pitchFactor = std::pow(2.0, static_cast<double>(totalSemitones) / 12.0);
    speedRatio_ = pitchFactor * (sample_->sampleRate / hostSampleRate);

    // Pan law (equal power)
    const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const float panAngle = (clampedPan + 1.0f) * 0.25f * kPi; // 0 .. pi/2
    const float leftPanGain = std::cos(panAngle);
    const float rightPanGain = std::sin(panAngle);

    const float totalAmp = std::clamp(velocity, 0.0f, 1.0f) * std::max(0.0f, volumeGain);
    gainLeft_ = totalAmp * leftPanGain;
    gainRight_ = totalAmp * rightPanGain;
}

void SampleVoice::stop() {
    isPlaying_ = false;
    sample_.reset();
}

void SampleVoice::renderNextBlock(float* leftOut, float* rightOut, int numFrames) {
    if (!isPlaying_ || !sample_ || sample_->numSamples <= 0) {
        return;
    }

    const int64_t totalLength = sample_->numSamples;
    const int numChannels = sample_->numChannels;
    const float* leftBuf = sample_->channels[0].data();
    const float* rightBuf = (numChannels > 1) ? sample_->channels[1].data() : leftBuf;

    for (int frame = 0; frame < numFrames; ++frame) {
        if (readPosition_ >= static_cast<double>(totalLength)) {
            isPlaying_ = false;
            sample_.reset();
            break;
        }

        const float sampL = interpolateCubic(leftBuf, totalLength, readPosition_);
        const float sampR = interpolateCubic(rightBuf, totalLength, readPosition_);

        leftOut[frame] += sampL * gainLeft_;
        rightOut[frame] += sampR * gainRight_;

        readPosition_ += speedRatio_;
    }
}

} // namespace openfruit::core
