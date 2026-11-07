#include "SonicAnalyzer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace openfruit::core {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr double kMinPitchHz = 24.0;
constexpr double kMaxPitchHz = 4200.0;
constexpr double kDefaultFallbackHz = 130.81278265; // C3

constexpr std::array<const char*, 12> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

struct ParabolicPeak {
    double position = 0.0;
    double value = 0.0;
};

struct FrameEstimate {
    double frequencyHz = 0.0;
    double clarity = 0.0;
    double spectralFreqHz = 0.0;
    double spectralCrest = 0.0;
    double centroidHz = 0.0;
    double frameRms = 0.0;
    bool isBodyFrame = false;
};

ParabolicPeak refineParabolicPeak(double y0, double y1, double y2, double xCenter) noexcept {
    const double denom = 2.0 * (2.0 * y1 - y0 - y2);
    if (std::abs(denom) < 1e-12) {
        return { xCenter, y1 };
    }
    const double delta = std::clamp((y2 - y0) / denom, -0.5, 0.5);
    const double refinedValue = y1 + 0.25 * (y2 - y0) * delta;
    return { xCenter + delta, refinedValue };
}

size_t nextPowerOfTwo(size_t n) noexcept {
    size_t p = 1;
    while (p < n) {
        p <<= 1;
    }
    return p;
}

void fftInPlace(std::vector<double>& re, std::vector<double>& im, bool inverse) noexcept {
    const size_t n = re.size();
    if (n <= 1) {
        return;
    }

    size_t j = 0;
    for (size_t i = 1; i < n; ++i) {
        size_t bit = n >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    const double sign = inverse ? 1.0 : -1.0;
    for (size_t len = 2; len <= n; len <<= 1) {
        const size_t half = len >> 1;
        const double angle = sign * kTwoPi / static_cast<double>(len);
        const double wStepRe = std::cos(angle);
        const double wStepIm = std::sin(angle);

        for (size_t i = 0; i < n; i += len) {
            double wRe = 1.0;
            double wIm = 0.0;
            for (size_t k = 0; k < half; ++k) {
                const size_t e = i + k;
                const size_t o = e + half;
                const double tRe = wRe * re[o] - wIm * im[o];
                const double tIm = wRe * im[o] + wIm * re[o];

                re[o] = re[e] - tRe;
                im[o] = im[e] - tIm;
                re[e] += tRe;
                im[e] += tIm;

                const double nextWRe = wRe * wStepRe - wIm * wStepIm;
                wIm = wRe * wStepIm + wIm * wStepRe;
                wRe = nextWRe;
            }
        }
    }

    if (!inverse) {
        return;
    }

    const double invN = 1.0 / static_cast<double>(n);
    for (size_t i = 0; i < n; ++i) {
        re[i] *= invN;
        im[i] *= invN;
    }
}

void removeDcAndNormalizeEnvelope(std::vector<double>& frame, double sampleRate) {
    const size_t n = frame.size();
    if (n == 0) {
        return;
    }

    double sum = 0.0;
    for (double v : frame) {
        sum += v;
    }
    const double mean = sum / static_cast<double>(n);

    double peak = 0.0;
    for (double& v : frame) {
        v -= mean;
        peak = std::max(peak, std::abs(v));
    }

    if (peak < 1e-12) {
        return;
    }

    const double invPeak = 1.0 / peak;
    for (double& v : frame) {
        v *= invPeak;
    }

    // Check if the frame has a steep exponential decay (e.g. percussive hit/chirp).
    // Leveling slow amplitude decay prevents the earliest transient cycles from dominating correlation.
    const size_t quarter = std::max<size_t>(1, n / 4);
    double earlyEnergy = 0.0;
    double lateEnergy = 0.0;
    for (size_t i = 0; i < quarter; ++i) {
        earlyEnergy += frame[i] * frame[i];
        lateEnergy += frame[n - 1 - i] * frame[n - 1 - i];
    }

    if (earlyEnergy <= 4.0 * lateEnergy || lateEnergy < 1e-6 * static_cast<double>(quarter)) {
        return;
    }

    const int smoothRadius = std::clamp(static_cast<int>(sampleRate * 0.006), 16, static_cast<int>(n / 4));
    std::vector<double> prefixAbs(n + 1, 0.0);
    for (size_t i = 0; i < n; ++i) {
        prefixAbs[i + 1] = prefixAbs[i] + std::abs(frame[i]);
    }

    double leveledPeak = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const int left = std::max(0, static_cast<int>(i) - smoothRadius);
        const int right = std::min(static_cast<int>(n), static_cast<int>(i) + smoothRadius + 1);
        const double localEnv = (prefixAbs[static_cast<size_t>(right)] - prefixAbs[static_cast<size_t>(left)]) /
                                static_cast<double>(right - left);
        const double gain = 1.0 / std::max(0.08, localEnv);
        frame[i] *= gain;
        leveledPeak = std::max(leveledPeak, std::abs(frame[i]));
    }

    if (leveledPeak > 1e-9) {
        const double scale = 1.0 / leveledPeak;
        for (double& v : frame) {
            v *= scale;
        }
    }
}

struct LobeCandidate {
    int lag = 0;
    double refinedLag = 0.0;
    double nsdfPeak = 0.0;
    double cmndfVal = 1.0;
};

std::vector<LobeCandidate> extractLobeCandidates(const std::vector<double>& nsdf,
                                                 const std::vector<double>& cmndf,
                                                 int minLag,
                                                 int maxLag) {
    std::vector<LobeCandidate> candidates;
    if (maxLag <= minLag + 2) {
        return candidates;
    }

    // Advance past the initial lag-0 self-correlation descent
    int startLag = minLag;
    while (startLag < maxLag - 1 && nsdf[static_cast<size_t>(startLag)] > 0.0 &&
           nsdf[static_cast<size_t>(startLag + 1)] <= nsdf[static_cast<size_t>(startLag)]) {
        ++startLag;
    }

    bool inPositiveLobe = false;
    int bestLagInLobe = -1;
    double bestValInLobe = -1.0;

    for (int lag = std::max(minLag + 1, startLag); lag < maxLag; ++lag) {
        const double val = nsdf[static_cast<size_t>(lag)];
        if (val > 0.0) {
            if (!inPositiveLobe) {
                inPositiveLobe = true;
                bestLagInLobe = -1;
                bestValInLobe = -1.0;
            }
            const double prev = nsdf[static_cast<size_t>(lag - 1)];
            const double next = nsdf[static_cast<size_t>(lag + 1)];
            if (val >= prev && val >= next && val > bestValInLobe) {
                bestValInLobe = val;
                bestLagInLobe = lag;
            }
            continue;
        }

        if (inPositiveLobe && bestLagInLobe > minLag) {
            const auto refined = refineParabolicPeak(
                nsdf[static_cast<size_t>(bestLagInLobe - 1)],
                nsdf[static_cast<size_t>(bestLagInLobe)],
                nsdf[static_cast<size_t>(bestLagInLobe + 1)],
                static_cast<double>(bestLagInLobe)
            );
            candidates.push_back({
                bestLagInLobe,
                refined.position,
                std::min(1.0, refined.value),
                cmndf[static_cast<size_t>(bestLagInLobe)]
            });
        }
        inPositiveLobe = false;
    }

    if (inPositiveLobe && bestLagInLobe > minLag && bestLagInLobe < maxLag) {
        const auto refined = refineParabolicPeak(
            nsdf[static_cast<size_t>(bestLagInLobe - 1)],
            nsdf[static_cast<size_t>(bestLagInLobe)],
            nsdf[static_cast<size_t>(bestLagInLobe + 1)],
            static_cast<double>(bestLagInLobe)
        );
        candidates.push_back({
            bestLagInLobe,
            refined.position,
            std::min(1.0, refined.value),
            cmndf[static_cast<size_t>(bestLagInLobe)]
        });
    }

    return candidates;
}

void analyzeSpectralFeatures(const std::vector<double>& powerSpectrum,
                             size_t fftSize,
                             double sampleRate,
                             double& outSpectralPeakHz,
                             double& outSpectralCrest,
                             double& outCentroidHz) {
    const double binHz = sampleRate / static_cast<double>(fftSize);
    const size_t halfFft = fftSize / 2;

    const size_t minPeakBin = std::max<size_t>(1, static_cast<size_t>(std::ceil(30.0 / binHz)));
    const size_t maxPeakBin = std::min<size_t>(halfFft - 2, static_cast<size_t>(std::floor(kMaxPitchHz / binHz)));

    std::vector<double> mag(halfFft, 0.0);
    for (size_t k = 1; k < halfFft; ++k) {
        mag[k] = std::sqrt(std::max(0.0, powerSpectrum[k]));
    }

    // Harmonic Product Spectrum (HPS) + direct magnitude for fundamental peak detection
    double maxHpsScore = 0.0;
    size_t bestBin = minPeakBin;
    double magSum = 0.0;
    size_t magCount = 0;

    for (size_t k = minPeakBin; k <= maxPeakBin; ++k) {
        const double m1 = mag[k];
        magSum += m1;
        ++magCount;

        if (m1 <= mag[k - 1] || m1 < mag[k + 1]) {
            continue;
        }

        const double m2 = (2 * k < halfFft) ? mag[2 * k] : 0.0;
        const double m3 = (3 * k < halfFft) ? mag[3 * k] : 0.0;
        const double hpsScore = m1 * (1.0 + 0.45 * (m2 / (m1 + 1e-12)) + 0.25 * (m3 / (m1 + 1e-12)));
        if (hpsScore > maxHpsScore) {
            maxHpsScore = hpsScore;
            bestBin = k;
        }
    }

    const double meanMag = (magCount > 0) ? (magSum / static_cast<double>(magCount)) : 0.0;
    outSpectralCrest = (meanMag > 1e-12) ? (mag[bestBin] / meanMag) : 0.0;

    const double y0 = std::log(std::max(1e-12, mag[bestBin - 1]));
    const double y1 = std::log(std::max(1e-12, mag[bestBin]));
    const double y2 = std::log(std::max(1e-12, mag[bestBin + 1]));
    const auto refinedBin = refineParabolicPeak(y0, y1, y2, static_cast<double>(bestBin));
    outSpectralPeakHz = std::clamp(refinedBin.position * binHz, kMinPitchHz, kMaxPitchHz);

    // Perceptually band-limited log-spectral centroid for unpitched/noisy percussion (50 Hz - 5500 Hz)
    const size_t minCentBin = std::max<size_t>(1, static_cast<size_t>(std::ceil(50.0 / binHz)));
    const size_t maxCentBin = std::min<size_t>(halfFft - 1, static_cast<size_t>(std::floor(5500.0 / binHz)));

    double weightedLogSum = 0.0;
    double weightSum = 0.0;
    for (size_t k = minCentBin; k <= maxCentBin; ++k) {
        const double freq = static_cast<double>(k) * binHz;
        const double w = powerSpectrum[k];
        weightedLogSum += w * std::log2(freq);
        weightSum += w;
    }

    if (weightSum > 1e-12) {
        outCentroidHz = std::clamp(std::exp2(weightedLogSum / weightSum), 50.0, 5500.0);
        return;
    }

    outCentroidHz = kDefaultFallbackHz;
}

FrameEstimate analyzeFrame(const float* monoSignal,
                           int64_t frameStart,
                           int windowSize,
                           double sampleRate,
                           bool isBodyFrame) {
    FrameEstimate est{};
    est.isBodyFrame = isBodyFrame;
    if (windowSize < 16) {
        return est;
    }

    std::vector<double> frame(static_cast<size_t>(windowSize));
    double sumSq = 0.0;
    for (int i = 0; i < windowSize; ++i) {
        const double s = static_cast<double>(monoSignal[frameStart + i]);
        frame[static_cast<size_t>(i)] = s;
        sumSq += s * s;
    }
    est.frameRms = std::sqrt(sumSq / static_cast<double>(windowSize));
    if (est.frameRms < 1e-7) {
        return est;
    }

    removeDcAndNormalizeEnvelope(frame, sampleRate);

    const size_t fftSize = nextPowerOfTwo(static_cast<size_t>(windowSize) * 2);
    std::vector<double> fftRe(fftSize, 0.0);
    std::vector<double> fftIm(fftSize, 0.0);
    for (int i = 0; i < windowSize; ++i) {
        fftRe[static_cast<size_t>(i)] = frame[static_cast<size_t>(i)];
    }

    // 1. Forward FFT -> Power Spectrum
    fftInPlace(fftRe, fftIm, false);
    std::vector<double> powerSpectrum(fftSize, 0.0);
    for (size_t k = 0; k < fftSize; ++k) {
        powerSpectrum[k] = fftRe[k] * fftRe[k] + fftIm[k] * fftIm[k];
        fftRe[k] = powerSpectrum[k];
        fftIm[k] = 0.0;
    }

    analyzeSpectralFeatures(powerSpectrum, fftSize, sampleRate,
                            est.spectralFreqHz, est.spectralCrest, est.centroidHz);

    // 2. Inverse FFT of Power Spectrum -> Exact Linear Autocorrelation r(lag)
    fftInPlace(fftRe, fftIm, true);

    const int minLag = std::max(2, static_cast<int>(std::floor(sampleRate / kMaxPitchHz)));
    const int maxLag = std::min((windowSize * 3) / 4, static_cast<int>(std::ceil(sampleRate / kMinPitchHz)));
    if (maxLag <= minLag + 2) {
        est.frequencyHz = est.spectralFreqHz;
        est.clarity = (est.spectralCrest > 6.0) ? 0.55 : 0.0;
        return est;
    }

    // 3. O(1) per-lag NSDF & YIN CMNDF via prefix sums of squares
    std::vector<double> prefixSq(static_cast<size_t>(windowSize + 1), 0.0);
    for (int i = 0; i < windowSize; ++i) {
        const double v = frame[static_cast<size_t>(i)];
        prefixSq[static_cast<size_t>(i + 1)] = prefixSq[static_cast<size_t>(i)] + v * v;
    }

    const double totalSq = prefixSq[static_cast<size_t>(windowSize)];
    std::vector<double> nsdf(static_cast<size_t>(maxLag + 1), 0.0);
    std::vector<double> cmndf(static_cast<size_t>(maxLag + 1), 1.0);

    double runningDiffSum = 0.0;
    for (int lag = 1; lag <= maxLag; ++lag) {
        const double r = fftRe[static_cast<size_t>(lag)];
        const double m = prefixSq[static_cast<size_t>(windowSize - lag)] +
                         (totalSq - prefixSq[static_cast<size_t>(lag)]);
        const double diff = std::max(0.0, m - 2.0 * r);
        runningDiffSum += diff;

        if (runningDiffSum > 1e-12) {
            cmndf[static_cast<size_t>(lag)] = (diff * static_cast<double>(lag)) / runningDiffSum;
        }
        if (m > 1e-12) {
            // Gentle window-overlap reliability weight for lags > W/2
            const double overlapRatio = static_cast<double>(windowSize - lag) / static_cast<double>(windowSize);
            const double taper = (overlapRatio < 0.45) ? (0.85 + 0.33 * overlapRatio) : 1.0;
            nsdf[static_cast<size_t>(lag)] = ((2.0 * r) / m) * taper;
        }
    }

    // 4. Extract MPM Lobe Maxima and select Fundamental Period
    const auto candidates = extractLobeCandidates(nsdf, cmndf, minLag, maxLag);
    double maxPeakVal = 0.0;
    for (const auto& c : candidates) {
        maxPeakVal = std::max(maxPeakVal, c.nsdfPeak);
    }

    if (maxPeakVal >= 0.35 && !candidates.empty()) {
        const double threshold = std::max(0.35, 0.85 * maxPeakVal);
        const LobeCandidate* chosen = nullptr;

        for (size_t i = 0; i < candidates.size(); ++i) {
            if (candidates[i].nsdfPeak < threshold) {
                continue;
            }

            // Guard against 2nd/3rd harmonic lock when the true fundamental at 2x or 3x lag is significantly stronger
            bool isHarmonicOfStrongerFundamental = false;
            for (size_t j = i + 1; j < candidates.size(); ++j) {
                const double ratio = candidates[j].refinedLag / candidates[i].refinedLag;
                const bool nearIntMultiple = (std::abs(ratio - 2.0) < 0.08) || (std::abs(ratio - 3.0) < 0.08);
                if (nearIntMultiple && candidates[j].nsdfPeak > candidates[i].nsdfPeak + 0.05 &&
                    candidates[j].cmndfVal + 0.02 < candidates[i].cmndfVal) {
                    isHarmonicOfStrongerFundamental = true;
                    break;
                }
            }

            if (!isHarmonicOfStrongerFundamental) {
                chosen = &candidates[i];
                break;
            }
        }

        if (chosen == nullptr) {
            for (const auto& c : candidates) {
                if (c.nsdfPeak >= threshold) {
                    chosen = &c;
                    break;
                }
            }
        }

        if (chosen != nullptr && chosen->refinedLag > 0.0) {
            est.frequencyHz = sampleRate / chosen->refinedLag;
            est.clarity = chosen->nsdfPeak;
            return est;
        }
    }

    // 5. Fallback for metallic/two-tone/noisy percussion: use narrowband spectral peak if prominent
    if (est.spectralCrest >= 4.5 && est.spectralFreqHz >= kMinPitchHz) {
        est.frequencyHz = est.spectralFreqHz;
        est.clarity = std::clamp((est.spectralCrest - 3.5) / 10.0, 0.25, 0.55);
        return est;
    }

    // 6. Broadband / unpitched noise percussion: use perceptual band-limited spectral centroid
    est.frequencyHz = est.centroidHz;
    est.clarity = 0.0;
    return est;
}

} // namespace

std::string SonicAnalyzer::midiNoteToName(int midiNote) {
    if (midiNote < 0 || midiNote > 127) {
        return "---";
    }
    const int noteIndex = midiNote % 12;
    const int octave = (midiNote / 12) - 1;
    return std::string(kNoteNames[static_cast<size_t>(noteIndex)]) + std::to_string(octave);
}

void SonicAnalyzer::frequencyToNote(double freqHz,
                                    int& outMidiNote,
                                    int& outCents,
                                    std::string& outNoteName) {
    if (freqHz <= 10.0 || !std::isfinite(freqHz)) {
        outMidiNote = 60;
        outCents = 0;
        outNoteName = "---";
        return;
    }

    const double exactMidi = 69.0 + 12.0 * std::log2(freqHz / 440.0);
    const int nearestMidi = std::clamp(static_cast<int>(std::round(exactMidi)), 0, 127);
    const int cents = static_cast<int>(std::round((exactMidi - nearestMidi) * 100.0));

    outMidiNote = nearestMidi;
    outCents = std::clamp(cents, -50, 50);
    outNoteName = midiNoteToName(nearestMidi);
}

std::string ToneAnalysisResult::formattedTone() const {
    if (dominantFrequencyHz <= 10.0 || !std::isfinite(dominantFrequencyHz) || noteName.empty() || noteName == "---") {
        return "~C3 (130.8 Hz)";
    }

    std::ostringstream ss;
    if (!hasClearTone) {
        ss << "~" << noteName;
        if (detuneCents > 0) ss << " +" << detuneCents << "c";
        else if (detuneCents < 0) ss << " " << detuneCents << "c";
        ss << " (~" << static_cast<int>(std::round(dominantFrequencyHz)) << " Hz)";
        return ss.str();
    }

    ss << noteName;
    if (detuneCents > 0) ss << " +" << detuneCents << "c";
    else if (detuneCents < 0) ss << " " << detuneCents << "c";
    ss << " (" << std::fixed << std::setprecision(1) << dominantFrequencyHz << " Hz)";
    return ss.str();
}

std::string ToneAnalysisResult::formattedDuration() const {
    if (durationSeconds < 1.0) {
        const int ms = static_cast<int>(std::round(durationSeconds * 1000.0));
        return std::to_string(ms) + " ms";
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << durationSeconds << " s";
    return ss.str();
}

ToneAnalysisResult SonicAnalyzer::analyze(const float* monoData,
                                          int64_t numSamples,
                                          double sampleRate) {
    const float* channels[1] = { monoData };
    return analyze(channels, 1, numSamples, sampleRate);
}

ToneAnalysisResult SonicAnalyzer::analyze(const float* const* channelData,
                                          int numChannels,
                                          int64_t numSamples,
                                          double sampleRate) {
    ToneAnalysisResult res{};
    frequencyToNote(kDefaultFallbackHz, res.midiNote, res.detuneCents, res.noteName);
    res.dominantFrequencyHz = kDefaultFallbackHz;
    res.hasClearTone = false;

    if (numSamples <= 0 || sampleRate <= 1000.0 || numChannels <= 0 || channelData == nullptr || channelData[0] == nullptr) {
        return res;
    }

    res.durationSamples = numSamples;
    res.durationSeconds = static_cast<double>(numSamples) / sampleRate;

    // 1. Downmix active analysis region (up to 1.5 seconds) & compute Peak/RMS.
    // Also guard against out-of-phase stereo cancellation (where L + R == 0).
    const int64_t maxAnalysisSamples = std::min<int64_t>(numSamples, static_cast<int64_t>(sampleRate * 1.5));
    std::vector<float> mono(static_cast<size_t>(maxAnalysisSamples), 0.0f);

    double sumSquaresMix = 0.0;
    double sumSquaresCh0 = 0.0;
    float peak = 0.0f;
    const float invChannels = 1.0f / static_cast<float>(numChannels);

    for (int64_t i = 0; i < maxAnalysisSamples; ++i) {
        float mixSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                const float s = channelData[ch][i];
                mixSample += s;
                peak = std::max(peak, std::abs(s));
            }
        }
        mixSample *= invChannels;
        mono[static_cast<size_t>(i)] = mixSample;
        sumSquaresMix += static_cast<double>(mixSample * mixSample);
        const float ch0 = channelData[0][i];
        sumSquaresCh0 += static_cast<double>(ch0 * ch0);
    }

    // If stereo channels cancel out in mono sum, use channel 0 for pitch analysis
    if (numChannels > 1 && sumSquaresMix < 0.15 * sumSquaresCh0) {
        for (int64_t i = 0; i < maxAnalysisSamples; ++i) {
            mono[static_cast<size_t>(i)] = channelData[0][i];
        }
        sumSquaresMix = sumSquaresCh0;
    }

    res.peakAmplitude = peak;
    res.rmsAmplitude = static_cast<float>(std::sqrt(sumSquaresMix / static_cast<double>(maxAnalysisSamples)));

    if (peak < 1e-7f) {
        return res;
    }

    // 2. Locate onset and active body bounds
    int64_t onsetIdx = 0;
    const float onsetThreshold = 0.05f * peak;
    for (int64_t i = 0; i < maxAnalysisSamples; ++i) {
        if (std::abs(mono[static_cast<size_t>(i)]) >= onsetThreshold) {
            onsetIdx = i;
            break;
        }
    }

    int64_t activeEndIdx = maxAnalysisSamples;
    const float tailThreshold = 0.005f * peak;
    for (int64_t i = maxAnalysisSamples - 1; i > onsetIdx; --i) {
        if (std::abs(mono[static_cast<size_t>(i)]) >= tailThreshold) {
            activeEndIdx = i + 1;
            break;
        }
    }

    const int64_t activeLength = std::max<int64_t>(1, activeEndIdx - onsetIdx);

    // 3. Optional 2x decimation for ultra-high sample rates (> 64 kHz) to keep window sizes compact & fast
    double effectiveSampleRate = sampleRate;
    const float* analysisPtr = mono.data() + onsetIdx;
    int64_t analysisLen = activeLength;
    std::vector<float> decimated;

    if (sampleRate > 64000.0 && activeLength >= 128) {
        effectiveSampleRate = sampleRate * 0.5;
        analysisLen = activeLength / 2;
        decimated.resize(static_cast<size_t>(analysisLen));
        for (int64_t i = 0; i < analysisLen; ++i) {
            decimated[static_cast<size_t>(i)] = 0.5f * (analysisPtr[2 * i] + analysisPtr[2 * i + 1]);
        }
        analysisPtr = decimated.data();
    }

    // 4. Multi-frame body analysis: evaluate onset and settled sustain frames
    const int maxTargetWindow = std::clamp(
        static_cast<int>(nextPowerOfTwo(static_cast<size_t>(effectiveSampleRate * 0.065))),
        1024, 4096
    );
    const int baseWindow = static_cast<int>(std::min<int64_t>(analysisLen, maxTargetWindow));

    std::vector<FrameEstimate> estimates;
    estimates.reserve(4);

    // Frame 0: Immediate onset window (captures ultra-short transients)
    estimates.push_back(analyzeFrame(analysisPtr, 0, baseWindow, effectiveSampleRate, false));

    // Additional body frames positioned past initial attack transient / pitch sweep
    const int bodyWindow = static_cast<int>(std::min<int64_t>(
        std::max<int64_t>(512, analysisLen / 3),
        maxTargetWindow
    ));

    if (analysisLen > bodyWindow + 64) {
        const int64_t maxOffset = analysisLen - bodyWindow;
        const std::array<double, 3> bodyFractions = { 0.30, 0.60, 0.85 };
        for (double frac : bodyFractions) {
            const int64_t offset = std::clamp<int64_t>(
                static_cast<int64_t>(std::round(frac * static_cast<double>(maxOffset))),
                1, maxOffset
            );
            const auto frameEst = analyzeFrame(analysisPtr, offset, bodyWindow, effectiveSampleRate, true);
            if (frameEst.frameRms >= 0.008 * estimates[0].frameRms) {
                estimates.push_back(frameEst);
            }
        }
    }

    // 5. Select winning pitch estimate across frames
    // Favor settled body frames with high periodic clarity and cross-frame pitch agreement
    int bestIdx = 0;
    double bestScore = -1.0;

    for (size_t i = 0; i < estimates.size(); ++i) {
        const auto& e = estimates[i];
        if (e.frequencyHz <= 10.0) {
            continue;
        }

        double agreementBonus = 1.0;
        for (size_t j = 0; j < estimates.size(); ++j) {
            if (i == j || estimates[j].frequencyHz <= 10.0 || estimates[j].clarity < 0.40) {
                continue;
            }
            const double semitoneDiff = 12.0 * std::abs(std::log2(e.frequencyHz / estimates[j].frequencyHz));
            if (semitoneDiff <= 0.85) {
                agreementBonus += 0.30;
            }
        }

        // Progressive sustain bonus: later frames that retain high clarity represent the settled body pitch
        const double progress = static_cast<double>(i) / static_cast<double>( std::max<size_t>(1, estimates.size() - 1) );
        const double bodyBonus = (e.isBodyFrame && e.clarity >= 0.55) ? (1.25 + 0.35 * progress) : 1.0;
        const double score = (e.clarity > 0.0 ? (e.clarity * e.clarity) : 0.05) * bodyBonus * agreementBonus;
        if (score > bestScore) {
            bestScore = score;
            bestIdx = static_cast<int>(i);
        }
    }

    const auto& winner = estimates[static_cast<size_t>(bestIdx)];
    res.dominantFrequencyHz = (winner.frequencyHz > 10.0 && std::isfinite(winner.frequencyHz))
                                  ? winner.frequencyHz
                                  : kDefaultFallbackHz;
    res.hasClearTone = (winner.clarity >= 0.40);
    frequencyToNote(res.dominantFrequencyHz, res.midiNote, res.detuneCents, res.noteName);
    return res;
}

} // namespace openfruit::core
