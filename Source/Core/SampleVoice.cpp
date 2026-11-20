#include "SampleVoice.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace openfruit::core {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr double kPiDouble = 3.14159265358979323846;
constexpr double kLn2 = 0.69314718055994530942;
constexpr int kSincHalfTaps = 16;
constexpr int kSincTaps = kSincHalfTaps * 2;
constexpr int kSincPhases = 512;
constexpr int kSincLobePoints = 4096;
constexpr int kFilterSubBlock = 16;
constexpr double kKaiserBeta = 7.857;
constexpr double kAttackOvershoot = 0.35;
constexpr double kDecayUndershoot = 0.02;
constexpr double kReleaseUndershoot = 0.002;

double besselI0(double x) noexcept {
    double sum = 1.0;
    double term = 1.0;
    const double halfSquared = 0.25 * x * x;
    for (int k = 1; k <= 24; ++k) {
        const double invK = 1.0 / static_cast<double>(k);
        term *= halfSquared * invK * invK;
        sum += term;
        if (term < sum * 1e-15) break;
    }
    return sum;
}

struct SincTables {
    std::array<std::array<float, kSincTaps>, kSincPhases + 1> polyphase{};
    std::array<float, kSincLobePoints + 1> lobe{};
    std::array<float, 8> halfbandOddTaps{};

    SincTables() noexcept {
        const double invI0Beta = 1.0 / besselI0(kKaiserBeta);
        const auto windowedSinc = [invI0Beta](double u, double halfWidth) noexcept -> double {
            if (std::abs(u) >= halfWidth) return 0.0;
            const double norm = u / halfWidth;
            const double window = besselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - norm * norm))) * invI0Beta;
            const double sincVal = (std::abs(u) < 1e-9) ? 1.0 : (std::sin(kPiDouble * u) / (kPiDouble * u));
            return sincVal * window;
        };

        for (int phase = 0; phase <= kSincPhases; ++phase) {
            const double frac = static_cast<double>(phase) / static_cast<double>(kSincPhases);
            double weightSum = 0.0;
            std::array<double, kSincTaps> raw{};
            for (int tap = 0; tap < kSincTaps; ++tap) {
                const double offset = static_cast<double>(tap - kSincHalfTaps + 1) - frac;
                raw[static_cast<size_t>(tap)] = windowedSinc(offset, static_cast<double>(kSincHalfTaps));
                weightSum += raw[static_cast<size_t>(tap)];
            }
            const double invSum = (std::abs(weightSum) > 1e-12) ? (1.0 / weightSum) : 0.0;
            for (int tap = 0; tap < kSincTaps; ++tap) {
                polyphase[static_cast<size_t>(phase)][static_cast<size_t>(tap)] =
                    static_cast<float>(raw[static_cast<size_t>(tap)] * invSum);
            }
        }

        for (int idx = 0; idx <= kSincLobePoints; ++idx) {
            const double u = (static_cast<double>(idx) / static_cast<double>(kSincLobePoints)) * static_cast<double>(kSincHalfTaps);
            lobe[static_cast<size_t>(idx)] = static_cast<float>(windowedSinc(u, static_cast<double>(kSincHalfTaps)));
        }

        double oddSum = 0.0;
        std::array<double, 8> oddRaw{};
        for (int k = 0; k < 8; ++k) {
            const int tapOffset = 2 * k + 1;
            const double norm = static_cast<double>(tapOffset) / 16.0;
            const double window = besselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - norm * norm))) * invI0Beta;
            const double sincVal = std::sin(0.5 * kPiDouble * static_cast<double>(tapOffset))
                                 / (kPiDouble * static_cast<double>(tapOffset));
            oddRaw[static_cast<size_t>(k)] = sincVal * window;
            oddSum += oddRaw[static_cast<size_t>(k)];
        }
        const double oddScale = (oddSum > 1e-12) ? (0.25 / oddSum) : 0.0;
        for (int k = 0; k < 8; ++k) {
            halfbandOddTaps[static_cast<size_t>(k)] = static_cast<float>(oddRaw[static_cast<size_t>(k)] * oddScale);
        }
    }
};

const SincTables& sincTables() noexcept {
    static const SincTables tables{};
    return tables;
}

double logCoshAntiderivative(double x) noexcept {
    const double absX = std::abs(x);
    return absX + std::log1p(std::exp(-2.0 * absX)) - kLn2;
}

float adaaTanhSample(double x1, double& prevX) noexcept {
    const double diff = x1 - prevX;
    const double out = (std::abs(diff) < 1e-5)
        ? std::tanh(0.5 * (prevX + x1))
        : ((logCoshAntiderivative(x1) - logCoshAntiderivative(prevX)) / diff);
    prevX = x1;
    return static_cast<float>(out);
}

} // namespace

void AudioSampleBuffer::buildMipPyramid() {
    mipLevels.clear();
    if (numChannels <= 0 || numSamples < 16 || static_cast<int>(channels.size()) < numChannels) {
        return;
    }

    const auto& oddTaps = sincTables().halfbandOddTaps;
    const std::vector<std::vector<float>>* prevChannels = &channels;
    int64_t prevLength = numSamples;

    for (int decimation : {2, 4, 8}) {
        const int64_t nextLength = prevLength / 2;
        if (nextLength < 8) break;

        MipLevel level;
        level.decimation = decimation;
        level.numSamples = nextLength;
        level.channels.resize(static_cast<size_t>(numChannels));

        for (int ch = 0; ch < numChannels; ++ch) {
            const auto& src = (*prevChannels)[static_cast<size_t>(ch)];
            auto& dst = level.channels[static_cast<size_t>(ch)];
            dst.resize(static_cast<size_t>(nextLength));
            for (int64_t i = 0; i < nextLength; ++i) {
                const int64_t center = i * 2;
                float acc = 0.5f * src[static_cast<size_t>(center)];
                for (int k = 0; k < 8; ++k) {
                    const int64_t tapDist = static_cast<int64_t>(2 * k + 1);
                    const int64_t leftIdx = std::clamp<int64_t>(center - tapDist, 0, prevLength - 1);
                    const int64_t rightIdx = std::clamp<int64_t>(center + tapDist, 0, prevLength - 1);
                    acc += oddTaps[static_cast<size_t>(k)] * (src[static_cast<size_t>(leftIdx)] + src[static_cast<size_t>(rightIdx)]);
                }
                dst[static_cast<size_t>(i)] = acc;
            }
        }

        mipLevels.push_back(std::move(level));
        prevChannels = &mipLevels.back().channels;
        prevLength = nextLength;
    }
}

float SampleVoice::sampleSinc(const float* buffer,
                              int64_t length,
                              double pos,
                              double cutoffScale,
                              int64_t regionStart,
                              int64_t regionEnd,
                              int startFadeLen,
                              int endFadeLen) noexcept {
    const auto& tables = sincTables();
    const auto getSample = [buffer, length, regionStart, regionEnd, startFadeLen, endFadeLen](int64_t idx) noexcept -> float {
        if (idx < 0 || idx >= length || idx < regionStart || idx >= regionEnd) return 0.0f;
        float val = buffer[idx];
        if (startFadeLen > 0 && idx - regionStart < startFadeLen) {
            const float phase = static_cast<float>(idx - regionStart) / static_cast<float>(startFadeLen);
            val *= 0.5f * (1.0f - std::cos(kPi * phase));
        }
        if (endFadeLen > 0 && (regionEnd - 1 - idx) < endFadeLen) {
            const float phase = static_cast<float>(regionEnd - 1 - idx) / static_cast<float>(endFadeLen);
            val *= 0.5f * (1.0f - std::cos(kPi * phase));
        }
        return val;
    };

    if (cutoffScale >= 0.999999) {
        const int64_t baseIndex = static_cast<int64_t>(std::floor(pos));
        const double frac = pos - static_cast<double>(baseIndex);
        const double scaledPhase = frac * static_cast<double>(kSincPhases);
        const int phaseIdx = std::clamp(static_cast<int>(scaledPhase), 0, kSincPhases - 1);
        const float phaseLerp = static_cast<float>(scaledPhase - static_cast<double>(phaseIdx));
        const auto& rowA = tables.polyphase[static_cast<size_t>(phaseIdx)];
        const auto& rowB = tables.polyphase[static_cast<size_t>(phaseIdx + 1)];

        float sum = 0.0f;
        const int64_t firstTap = baseIndex - kSincHalfTaps + 1;
        for (int tap = 0; tap < kSincTaps; ++tap) {
            const float weight = rowA[static_cast<size_t>(tap)]
                               + phaseLerp * (rowB[static_cast<size_t>(tap)] - rowA[static_cast<size_t>(tap)]);
            sum += getSample(firstTap + tap) * weight;
        }
        return sum;
    }

    const double clampedScale = std::clamp(cutoffScale, 1.0 / 32.0, 1.0);
    const double radius = static_cast<double>(kSincHalfTaps) / clampedScale;
    const int64_t firstIdx = static_cast<int64_t>(std::ceil(pos - radius));
    const int64_t lastIdx = static_cast<int64_t>(std::floor(pos + radius));
    const double lobeScale = clampedScale * (static_cast<double>(kSincLobePoints) / static_cast<double>(kSincHalfTaps));

    float weightedSum = 0.0f;
    float weightTotal = 0.0f;
    for (int64_t idx = firstIdx; idx <= lastIdx; ++idx) {
        const double tablePos = std::abs(static_cast<double>(idx) - pos) * lobeScale;
        if (tablePos >= static_cast<double>(kSincLobePoints)) continue;
        const int lobeIdx = static_cast<int>(tablePos);
        const float lobeFrac = static_cast<float>(tablePos - static_cast<double>(lobeIdx));
        const float w0 = tables.lobe[static_cast<size_t>(lobeIdx)];
        const float w1 = tables.lobe[static_cast<size_t>(lobeIdx + 1)];
        const float weight = w0 + lobeFrac * (w1 - w0);
        weightTotal += weight;
        weightedSum += getSample(idx) * weight;
    }
    if (weightTotal <= 1e-9f) return 0.0f;
    return weightedSum / weightTotal;
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
        stopPrimaryVoice();
        return;
    }

    if (isPlaying_) {
        choke();
    }

    sample_ = std::move(sample);
    hostSampleRate_ = hostSampleRate;
    sampleStart_ = static_cast<int64_t>(std::floor(std::clamp(settings.sampleStart, 0.0f, 1.0f) * static_cast<float>(sample_->numSamples)));
    sampleEnd_ = static_cast<int64_t>(std::ceil(std::clamp(settings.sampleEnd, 0.0f, 1.0f) * static_cast<float>(sample_->numSamples)));
    sampleStart_ = std::clamp<int64_t>(sampleStart_, 0, sample_->numSamples - 1);
    sampleEnd_ = std::clamp<int64_t>(sampleEnd_, sampleStart_ + 1, sample_->numSamples);
    direction_ = settings.reverse ? -1 : 1;

    const double normSemitones = settings.keyNormalizePitch ? sample_->toneResult.pitchNormalizationSemitones() : 0.0;
    const double fineSemitones = (std::isfinite(settings.fineTuneCents) ? static_cast<double>(settings.fineTuneCents) : 0.0) * 0.01;
    const double totalSemitones = std::clamp(
        static_cast<double>(semitoneOffset + trackBaseTune) + normSemitones + fineSemitones,
        -48.0,
        48.0
    );
    basePitchFactor_ = std::pow(2.0, totalSemitones / 12.0);
    speedRatio_ = basePitchFactor_ * (sample_->sampleRate / hostSampleRate);

    const double clampedSubSample = std::clamp(std::isfinite(settings.subSampleOffset) ? settings.subSampleOffset : 0.0, 0.0, 1.0);
    const double baseStartPos = settings.reverse ? static_cast<double>(sampleEnd_ - 1) : static_cast<double>(sampleStart_);
    readPosition_ = baseStartPos + static_cast<double>(direction_) * clampedSubSample * speedRatio_;

    activeMipIndex_ = -1;
    mipDecimation_ = 1;
    const int64_t regionSpan = sampleEnd_ - sampleStart_;
    for (size_t m = 0; m < sample_->mipLevels.size(); ++m) {
        const auto& mip = sample_->mipLevels[m];
        if (static_cast<double>(mip.decimation) <= speedRatio_ * 1.0001 && regionSpan / mip.decimation >= 8) {
            activeMipIndex_ = static_cast<int>(m);
            mipDecimation_ = mip.decimation;
        }
    }

    trimStartFadeSamples_ = 0;
    trimEndFadeSamples_ = 0;
    if (regionSpan >= 64) {
        bool nonZeroStart = false;
        bool nonZeroEnd = false;
        for (int ch = 0; ch < sample_->numChannels; ++ch) {
            const auto& chBuf = sample_->channels[static_cast<size_t>(ch)];
            if (sampleStart_ > 0 && std::abs(chBuf[static_cast<size_t>(sampleStart_)]) > 1e-5f) {
                nonZeroStart = true;
            }
            if (sampleEnd_ < sample_->numSamples && std::abs(chBuf[static_cast<size_t>(sampleEnd_ - 1)]) > 1e-5f) {
                nonZeroEnd = true;
            }
        }
        const int fadeLen = (speedRatio_ <= 1.5 && regionSpan >= 128) ? 48 : 32;
        if (nonZeroStart) trimStartFadeSamples_ = fadeLen;
        if (nonZeroEnd) trimEndFadeSamples_ = fadeLen;
    }

    attackSeconds_ = std::clamp(settings.attackSeconds, 0.0f, 10.0f);
    decaySeconds_ = std::clamp(settings.decaySeconds, 0.0f, 10.0f);
    sustainLevel_ = std::clamp(settings.sustainLevel, 0.0f, 1.0f);
    releaseSeconds_ = std::clamp(settings.releaseSeconds, 0.0f, 20.0f);
    chokeGroup_ = std::clamp(settings.chokeGroup, 0, 4);
    midiNote_ = settings.midiNote;

    const double attackSamples = static_cast<double>(attackSeconds_) * hostSampleRate_;
    const double decaySamples = static_cast<double>(decaySeconds_) * hostSampleRate_;
    const double releaseSamples = static_cast<double>(releaseSeconds_) * hostSampleRate_;
    attackCoeff_ = (attackSamples > 0.5)
        ? std::exp(std::log(kAttackOvershoot / (1.0 + kAttackOvershoot)) / attackSamples)
        : 0.0;
    decayCoeff_ = (decaySamples > 0.5)
        ? std::exp(std::log(kDecayUndershoot / (1.0 + kDecayUndershoot)) / decaySamples)
        : 0.0;
    releaseCoeff_ = (releaseSamples > 0.5)
        ? std::exp(std::log(kReleaseUndershoot / (1.0 + kReleaseUndershoot)) / releaseSamples)
        : 0.0;

    envStage_ = EnvelopeStage::Sustain;
    envLevel_ = static_cast<double>(sustainLevel_);
    envTarget_ = envLevel_;
    if (decayCoeff_ > 0.0 && sustainLevel_ < 0.9999f) {
        envStage_ = EnvelopeStage::Decay;
        envLevel_ = 1.0;
        envTarget_ = static_cast<double>(sustainLevel_) - kDecayUndershoot * (1.0 - static_cast<double>(sustainLevel_));
    }
    if (attackCoeff_ > 0.0) {
        envStage_ = EnvelopeStage::Attack;
        envLevel_ = 0.0;
        envTarget_ = 1.0 + kAttackOvershoot;
    }

    lpfL_ = {};
    lpfR_ = {};
    hpfL_ = {};
    hpfR_ = {};
    lastLpfCutoffHz_ = -1.0f;
    lastHpfCutoffHz_ = -1.0f;
    lastResonance_ = -1.0f;
    lastKeyTrack_ = -1.0f;
    filterSmoothCoeff_ = 1.0f - std::exp(-static_cast<float>(kFilterSubBlock) / static_cast<float>(0.002 * hostSampleRate_));
    updateFilterSettings(settings.filterCutoffHz, settings.highPassCutoffHz, settings.filterResonance, settings.filterKeyTrack);
    lpfLogGCurrent_ = lpfLogGTarget_;
    hpfLogGCurrent_ = hpfLogGTarget_;
    svfKCurrent_ = svfKTarget_;

    drive_ = std::clamp(std::isfinite(settings.drive) ? settings.drive : 0.0f, 0.0f, 1.0f);
    driveGain_ = 1.0f + drive_ * 4.0f;
    driveNorm_ = drive_ > 0.0001f ? (1.0f / std::tanh(driveGain_)) : 1.0f;
    upBandL_ = {};
    upBandR_ = {};
    downBandL_ = {};
    downBandR_ = {};
    adaaPrevL_ = 0.0;
    adaaPrevR_ = 0.0;
    adaaPrimed_ = false;

    const float clampedPan = std::clamp(pan, -1.0f, 1.0f);
    const float panAngle = (clampedPan + 1.0f) * 0.25f * kPi;
    const float totalAmp = std::clamp(velocity, 0.0f, 1.0f) * std::max(0.0f, volumeGain);
    gainLeft_ = totalAmp * std::cos(panAngle);
    gainRight_ = totalAmp * std::sin(panAngle);
    isPlaying_ = true;
}

void SampleVoice::updateFilterSettings(float lowPassCutoffHz,
                                       float highPassCutoffHz,
                                       float resonance,
                                       float keyTrack) noexcept {
    const float safeLpf = std::isfinite(lowPassCutoffHz) ? lowPassCutoffHz : 20000.0f;
    const float safeHpf = std::isfinite(highPassCutoffHz) ? highPassCutoffHz : 20.0f;
    const float safeRes = std::clamp(std::isfinite(resonance) ? resonance : 0.0f, 0.0f, 1.0f);
    const float safeTrack = std::clamp(std::isfinite(keyTrack) ? keyTrack : 0.0f, 0.0f, 1.0f);

    if (safeLpf == lastLpfCutoffHz_ && safeHpf == lastHpfCutoffHz_
        && safeRes == lastResonance_ && safeTrack == lastKeyTrack_) {
        return;
    }
    lastLpfCutoffHz_ = safeLpf;
    lastHpfCutoffHz_ = safeHpf;
    lastResonance_ = safeRes;
    lastKeyTrack_ = safeTrack;

    const float qVal = 0.70710678f * std::pow(12.0f, safeRes);
    svfKTarget_ = 1.0f / qVal;

    const float pitchScale = static_cast<float>(std::pow(basePitchFactor_, static_cast<double>(safeTrack)));
    const float maxCutoff = static_cast<float>(hostSampleRate_ * 0.485);

    lpfTargetActive_ = (safeLpf < 19990.0f) || (safeRes > 0.001f);
    const float clampedLpf = std::clamp(safeLpf * pitchScale, 20.0f, maxCutoff);
    const float lpfG = lpfTargetActive_
        ? std::tan(kPi * clampedLpf / static_cast<float>(hostSampleRate_))
        : std::tan(kPi * 0.485f);
    lpfLogGTarget_ = std::log(std::max(1e-5f, lpfG));

    hpfTargetActive_ = (safeHpf > 20.5f);
    const float clampedHpf = std::clamp(safeHpf * pitchScale, 20.0f, maxCutoff);
    const float hpfG = std::tan(kPi * clampedHpf / static_cast<float>(hostSampleRate_));
    hpfLogGTarget_ = std::log(std::max(1e-5f, hpfG));
}

void SampleVoice::stopPrimaryVoice() noexcept {
    isPlaying_ = false;
    sample_.reset();
}

void SampleVoice::stop() {
    stopPrimaryVoice();
    tailPos_ = 0;
    tailLength_ = 0;
}

void SampleVoice::advanceEnvelope() noexcept {
    if (envStage_ == EnvelopeStage::Attack) {
        envLevel_ = envTarget_ + attackCoeff_ * (envLevel_ - envTarget_);
        if (envLevel_ >= 1.0) {
            envLevel_ = 1.0;
            if (decayCoeff_ > 0.0 && sustainLevel_ < 0.9999f) {
                envStage_ = EnvelopeStage::Decay;
                envTarget_ = static_cast<double>(sustainLevel_) - kDecayUndershoot * (1.0 - static_cast<double>(sustainLevel_));
                return;
            }
            envStage_ = EnvelopeStage::Sustain;
            envLevel_ = static_cast<double>(sustainLevel_);
            envTarget_ = envLevel_;
        }
        return;
    }

    if (envStage_ == EnvelopeStage::Decay) {
        envLevel_ = envTarget_ + decayCoeff_ * (envLevel_ - envTarget_);
        if (envLevel_ <= static_cast<double>(sustainLevel_)) {
            envLevel_ = static_cast<double>(sustainLevel_);
            envStage_ = EnvelopeStage::Sustain;
        }
        return;
    }

    if (envStage_ == EnvelopeStage::Release) {
        if (releaseCoeff_ <= 0.0) {
            envLevel_ = 0.0;
            return;
        }
        envLevel_ = envTarget_ + releaseCoeff_ * (envLevel_ - envTarget_);
        if (envLevel_ <= 0.0) {
            envLevel_ = 0.0;
        }
    }
}

void SampleVoice::noteOff() noexcept {
    if (!isPlaying_ || envStage_ == EnvelopeStage::Release) return;
    if (releaseSeconds_ <= 0.0f || envLevel_ <= 1e-6) {
        stopPrimaryVoice();
        return;
    }
    envStage_ = EnvelopeStage::Release;
    envTarget_ = -kReleaseUndershoot * envLevel_;
}

void SampleVoice::choke() noexcept {
    if (!isPlaying_ || !sample_ || sample_->numSamples <= 0) {
        stopPrimaryVoice();
        return;
    }

    const int fadeFrames = std::clamp(static_cast<int>(std::round(0.003 * hostSampleRate_)), 16, kMaxShadowTailFrames);
    const int remainingOld = std::max(0, tailLength_ - tailPos_);
    const int newTailLen = std::max(remainingOld, fadeFrames);

    for (int i = 0; i < remainingOld; ++i) {
        tailLeft_[static_cast<size_t>(i)] = tailLeft_[static_cast<size_t>(tailPos_ + i)];
        tailRight_[static_cast<size_t>(i)] = tailRight_[static_cast<size_t>(tailPos_ + i)];
    }
    for (int i = remainingOld; i < newTailLen; ++i) {
        tailLeft_[static_cast<size_t>(i)] = 0.0f;
        tailRight_[static_cast<size_t>(i)] = 0.0f;
    }

    renderPrimaryFrames(tailLeft_.data(), tailRight_.data(), fadeFrames, true);
    stopPrimaryVoice();
    tailPos_ = 0;
    tailLength_ = newTailLen;
}

void SampleVoice::renderPrimaryFrames(float* leftOut,
                                      float* rightOut,
                                      int numFrames,
                                      bool applyWindowFade) noexcept {
    if (!isPlaying_ || !sample_ || sample_->numSamples <= 0 || numFrames <= 0) {
        return;
    }

    const bool useMip = activeMipIndex_ >= 0 && activeMipIndex_ < static_cast<int>(sample_->mipLevels.size());
    const auto& activeChannels = useMip ? sample_->mipLevels[static_cast<size_t>(activeMipIndex_)].channels : sample_->channels;
    const int64_t totalLength = useMip ? sample_->mipLevels[static_cast<size_t>(activeMipIndex_)].numSamples : sample_->numSamples;
    const int numChannels = sample_->numChannels;
    const float* leftBuf = activeChannels[0].data();
    const float* rightBuf = (numChannels > 1) ? activeChannels[1].data() : leftBuf;
    const double invDecim = 1.0 / static_cast<double>(mipDecimation_);
    const double mipSpeedRatio = speedRatio_ * invDecim;
    const double cutoffScale = std::min(1.0, 1.0 / std::max(1e-6, mipSpeedRatio));
    const int64_t mipRegionStart = sampleStart_ / mipDecimation_;
    const int64_t mipRegionEnd = std::clamp<int64_t>((sampleEnd_ + mipDecimation_ - 1) / mipDecimation_, mipRegionStart + 1, totalLength);
    const int mipStartFade = (trimStartFadeSamples_ > 0) ? std::max(2, trimStartFadeSamples_ / (mipDecimation_ * 2)) : 0;
    const int mipEndFade = (trimEndFadeSamples_ > 0) ? std::max(2, trimEndFadeSamples_ / (mipDecimation_ * 2)) : 0;

    int frame = 0;
    while (frame < numFrames && isPlaying_) {
        const int blockEnd = std::min(numFrames, frame + kFilterSubBlock);

        const float lpfDiff = lpfLogGTarget_ - lpfLogGCurrent_;
        lpfLogGCurrent_ = (std::abs(lpfDiff) > 1e-5f) ? (lpfLogGCurrent_ + filterSmoothCoeff_ * lpfDiff) : lpfLogGTarget_;
        const float hpfDiff = hpfLogGTarget_ - hpfLogGCurrent_;
        hpfLogGCurrent_ = (std::abs(hpfDiff) > 1e-5f) ? (hpfLogGCurrent_ + filterSmoothCoeff_ * hpfDiff) : hpfLogGTarget_;
        svfKCurrent_ += filterSmoothCoeff_ * (svfKTarget_ - svfKCurrent_);

        const bool runLpf = lpfTargetActive_ || (std::abs(lpfLogGTarget_ - lpfLogGCurrent_) > 1e-4f);
        const bool runHpf = hpfTargetActive_ || (std::abs(hpfLogGTarget_ - hpfLogGCurrent_) > 1e-4f);

        const float lpfG = runLpf ? std::exp(lpfLogGCurrent_) : 1.0f;
        const float lpfA1 = 1.0f / (1.0f + lpfG * (lpfG + svfKCurrent_));
        const float lpfA2 = lpfG * lpfA1;
        const float lpfA3 = lpfG * lpfA2;

        const float hpfG = runHpf ? std::exp(hpfLogGCurrent_) : 0.0f;
        const float hpfK = 1.41421356f;
        const float hpfA1 = 1.0f / (1.0f + hpfG * (hpfG + hpfK));
        const float hpfA2 = hpfG * hpfA1;
        const float hpfA3 = hpfG * hpfA2;

        for (; frame < blockEnd; ++frame) {
            if ((direction_ > 0 && readPosition_ >= static_cast<double>(sampleEnd_))
                || (direction_ < 0 && readPosition_ < static_cast<double>(sampleStart_))) {
                stopPrimaryVoice();
                break;
            }
            if (envStage_ == EnvelopeStage::Release && envLevel_ <= 0.0) {
                stopPrimaryVoice();
                break;
            }

            const double mipPos = readPosition_ * invDecim;
            float sampL = sampleSinc(leftBuf, totalLength, mipPos, cutoffScale, mipRegionStart, mipRegionEnd, mipStartFade, mipEndFade);
            float sampR = (rightBuf == leftBuf)
                ? sampL
                : sampleSinc(rightBuf, totalLength, mipPos, cutoffScale, mipRegionStart, mipRegionEnd, mipStartFade, mipEndFade);

            if (trimStartFadeSamples_ > 0) {
                const double distStart = readPosition_ - static_cast<double>(sampleStart_);
                if (distStart < static_cast<double>(trimStartFadeSamples_)) {
                    const float phase = static_cast<float>(std::clamp(distStart / static_cast<double>(trimStartFadeSamples_), 0.0, 1.0));
                    const float trimGain = 0.5f * (1.0f - std::cos(kPi * phase));
                    sampL *= trimGain;
                    sampR *= trimGain;
                }
            }
            if (trimEndFadeSamples_ > 0) {
                const double distEnd = static_cast<double>(sampleEnd_ - 1) - readPosition_;
                if (distEnd < static_cast<double>(trimEndFadeSamples_)) {
                    const float phase = static_cast<float>(std::clamp(distEnd / static_cast<double>(trimEndFadeSamples_), 0.0, 1.0));
                    const float trimGain = 0.5f * (1.0f - std::cos(kPi * phase));
                    sampL *= trimGain;
                    sampR *= trimGain;
                }
            }

            if (runHpf) {
                const float v3L = sampL - hpfL_.ic2;
                const float v1L = hpfA1 * hpfL_.ic1 + hpfA2 * v3L;
                const float v2L = hpfL_.ic2 + hpfA2 * hpfL_.ic1 + hpfA3 * v3L;
                hpfL_.ic1 = 2.0f * v1L - hpfL_.ic1;
                hpfL_.ic2 = 2.0f * v2L - hpfL_.ic2;
                sampL = sampL - hpfK * v1L - v2L;

                const float v3R = sampR - hpfR_.ic2;
                const float v1R = hpfA1 * hpfR_.ic1 + hpfA2 * v3R;
                const float v2R = hpfR_.ic2 + hpfA2 * hpfR_.ic1 + hpfA3 * v3R;
                hpfR_.ic1 = 2.0f * v1R - hpfR_.ic1;
                hpfR_.ic2 = 2.0f * v2R - hpfR_.ic2;
                sampR = sampR - hpfK * v1R - v2R;
            }

            if (runLpf) {
                const float v3L = sampL - lpfL_.ic2;
                const float v1L = lpfA1 * lpfL_.ic1 + lpfA2 * v3L;
                const float v2L = lpfL_.ic2 + lpfA2 * lpfL_.ic1 + lpfA3 * v3L;
                lpfL_.ic1 = 2.0f * v1L - lpfL_.ic1;
                lpfL_.ic2 = 2.0f * v2L - lpfL_.ic2;
                sampL = v2L;

                const float v3R = sampR - lpfR_.ic2;
                const float v1R = lpfA1 * lpfR_.ic1 + lpfA2 * v3R;
                const float v2R = lpfR_.ic2 + lpfA2 * lpfR_.ic1 + lpfA3 * v3R;
                lpfR_.ic1 = 2.0f * v1R - lpfR_.ic1;
                lpfR_.ic2 = 2.0f * v2R - lpfR_.ic2;
                sampR = v2R;
            }
            if (!runLpf) {
                lpfL_.ic1 = 0.0f;
                lpfL_.ic2 = sampL;
                lpfR_.ic1 = 0.0f;
                lpfR_.ic2 = sampR;
            }

            if (drive_ > 0.0001f) {
                const auto upL = upBandL_.upsample(sampL);
                const auto upR = upBandR_.upsample(sampR);

                const double xL0 = static_cast<double>(upL[0] * driveGain_);
                const double xL1 = static_cast<double>(upL[1] * driveGain_);
                const double xR0 = static_cast<double>(upR[0] * driveGain_);
                const double xR1 = static_cast<double>(upR[1] * driveGain_);
                if (!adaaPrimed_) {
                    adaaPrevL_ = xL0;
                    adaaPrevR_ = xR0;
                    adaaPrimed_ = true;
                }

                const float satL0 = adaaTanhSample(xL0, adaaPrevL_) * driveNorm_;
                const float satL1 = adaaTanhSample(xL1, adaaPrevL_) * driveNorm_;
                const float satR0 = adaaTanhSample(xR0, adaaPrevR_) * driveNorm_;
                const float satR1 = adaaTanhSample(xR1, adaaPrevR_) * driveNorm_;

                sampL = downBandL_.downsample(satL0, satL1);
                sampR = downBandR_.downsample(satR0, satR1);
            }

            float ampEnvelope = envelopeLevel();
            if (applyWindowFade) {
                const float fadePhase = static_cast<float>(frame + 1) / static_cast<float>(numFrames);
                ampEnvelope *= 0.5f * (1.0f + std::cos(kPi * fadePhase));
            }

            leftOut[frame] += sampL * gainLeft_ * ampEnvelope;
            rightOut[frame] += sampR * gainRight_ * ampEnvelope;

            readPosition_ += speedRatio_ * static_cast<double>(direction_);
            advanceEnvelope();
            if ((direction_ > 0 && readPosition_ >= static_cast<double>(sampleEnd_))
                || (direction_ < 0 && readPosition_ < static_cast<double>(sampleStart_))
                || (envStage_ == EnvelopeStage::Release && envLevel_ <= 0.0)) {
                stopPrimaryVoice();
                break;
            }
        }
    }
}

void SampleVoice::renderNextBlock(float* leftOut, float* rightOut, int numFrames) {
    if (numFrames <= 0) return;

    if (tailPos_ < tailLength_) {
        const int tailCount = std::min(numFrames, tailLength_ - tailPos_);
        for (int i = 0; i < tailCount; ++i) {
            leftOut[i] += tailLeft_[static_cast<size_t>(tailPos_ + i)];
            rightOut[i] += tailRight_[static_cast<size_t>(tailPos_ + i)];
        }
        tailPos_ += tailCount;
        if (tailPos_ >= tailLength_) {
            tailPos_ = 0;
            tailLength_ = 0;
        }
    }

    renderPrimaryFrames(leftOut, rightOut, numFrames, false);
}

} // namespace openfruit::core
