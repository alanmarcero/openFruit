#include "SampleVoice.h"
#include <algorithm>
#include <cmath>

namespace openfruit::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;

} // namespace

float SampleVoice::interpolateCubic(const float* buffer,
                                    int64_t length,
                                    double pos,
                                    int64_t regionStart,
                                    int64_t regionEnd) noexcept {
    const int64_t i = static_cast<int64_t>(std::floor(pos));
    const float frac = static_cast<float>(pos - static_cast<double>(i));

    const auto getSample = [buffer, length, regionStart, regionEnd](int64_t idx) -> float {
        if (idx < 0 || idx >= length || idx < regionStart || idx >= regionEnd) return 0.0f;
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
                        float pan,
                        VoiceSettings settings) {
    if (!sample || !sample->isLoaded || sample->numSamples <= 0 || hostSampleRate <= 1000.0) {
        isPlaying_ = false;
        return;
    }

    sample_ = std::move(sample);
    hostSampleRate_ = hostSampleRate;
    sampleStart_ = static_cast<int64_t>(std::floor(std::clamp(settings.sampleStart, 0.0f, 1.0f) * static_cast<float>(sample_->numSamples)));
    sampleEnd_ = static_cast<int64_t>(std::ceil(std::clamp(settings.sampleEnd, 0.0f, 1.0f) * static_cast<float>(sample_->numSamples)));
    sampleStart_ = std::clamp<int64_t>(sampleStart_, 0, sample_->numSamples - 1);
    sampleEnd_ = std::clamp<int64_t>(sampleEnd_, sampleStart_ + 1, sample_->numSamples);
    direction_ = settings.reverse ? -1 : 1;
    readPosition_ = settings.reverse ? static_cast<double>(sampleEnd_ - 1) : static_cast<double>(sampleStart_);
    isPlaying_ = true;
    envelopeElapsedSamples_ = 0.0;
    releaseElapsedSamples_ = 0.0;
    releaseStartLevel_ = 1.0;
    released_ = false;
    filterLeft_ = filterRight_ = 0.0f;
    attackSeconds_ = std::clamp(settings.attackSeconds, 0.0f, 10.0f);
    decaySeconds_ = std::clamp(settings.decaySeconds, 0.0f, 10.0f);
    sustainLevel_ = std::clamp(settings.sustainLevel, 0.0f, 1.0f);
    releaseSeconds_ = std::clamp(settings.releaseSeconds, 0.0f, 20.0f);
    chokeGroup_ = std::clamp(settings.chokeGroup, 0, 4);
    midiNote_ = settings.midiNote;
    const float cutoff = std::isfinite(settings.filterCutoffHz) ? settings.filterCutoffHz : 20000.0f;
    lowPassCoefficient_ = cutoff >= hostSampleRate * 0.45
        ? 1.0
        : 1.0 - std::exp(-2.0 * static_cast<double>(kPi) * std::clamp(static_cast<double>(cutoff), 20.0, hostSampleRate * 0.45) / hostSampleRate);

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

float SampleVoice::envelopeLevel() const noexcept {
    if (released_) {
        const double releaseSamples = releaseSeconds_ * hostSampleRate_;
        if (releaseSamples <= 0.0) return 0.0f;
        return releaseStartLevel_ * static_cast<float>(std::max(0.0, 1.0 - releaseElapsedSamples_ / releaseSamples));
    }

    const double attackSamples = attackSeconds_ * hostSampleRate_;
    const double decaySamples = decaySeconds_ * hostSampleRate_;
    if (attackSamples > 0.0 && envelopeElapsedSamples_ < attackSamples)
        return static_cast<float>(envelopeElapsedSamples_ / attackSamples);
    if (decaySamples > 0.0 && envelopeElapsedSamples_ < attackSamples + decaySamples) {
        const double decayPhase = (envelopeElapsedSamples_ - attackSamples) / decaySamples;
        return static_cast<float>(1.0 - (1.0 - sustainLevel_) * decayPhase);
    }
    return sustainLevel_;
}

void SampleVoice::noteOff() noexcept {
    if (!isPlaying_ || released_) return;
    releaseStartLevel_ = envelopeLevel();
    released_ = true;
    releaseElapsedSamples_ = 0.0;
    if (releaseSeconds_ <= 0.0f) stop();
}

void SampleVoice::choke() noexcept {
    stop();
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
        if ((direction_ > 0 && readPosition_ >= static_cast<double>(sampleEnd_))
            || (direction_ < 0 && readPosition_ < static_cast<double>(sampleStart_))) {
            stop();
            break;
        }

        if (released_ && releaseElapsedSamples_ >= releaseSeconds_ * hostSampleRate_) {
            stop();
            break;
        }

        float sampL = interpolateCubic(leftBuf, totalLength, readPosition_, sampleStart_, sampleEnd_);
        float sampR = interpolateCubic(rightBuf, totalLength, readPosition_, sampleStart_, sampleEnd_);
        filterLeft_ += static_cast<float>(lowPassCoefficient_) * (sampL - filterLeft_);
        filterRight_ += static_cast<float>(lowPassCoefficient_) * (sampR - filterRight_);
        sampL = filterLeft_;
        sampR = filterRight_;
        const float ampEnvelope = envelopeLevel();

        leftOut[frame] += sampL * gainLeft_ * ampEnvelope;
        rightOut[frame] += sampR * gainRight_ * ampEnvelope;

        readPosition_ += speedRatio_ * direction_;
        envelopeElapsedSamples_ += 1.0;
        if (released_) releaseElapsedSamples_ += 1.0;
        if ((direction_ > 0 && readPosition_ >= static_cast<double>(sampleEnd_))
            || (direction_ < 0 && readPosition_ < static_cast<double>(sampleStart_))) {
            stop();
        }
    }
}

} // namespace openfruit::core
