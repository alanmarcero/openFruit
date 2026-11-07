#include "SonicAnalyzer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace openfruit::core {

namespace {

constexpr std::array<const char*, 12> kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Parabolic interpolation for sub-sample peak refinement
double parabolicInterpolation(double y0, double y1, double y2, double xPeak) noexcept {
    const double denom = 2.0 * (2.0 * y1 - y0 - y2);
    if (std::abs(denom) < 1e-9) {
        return xPeak;
    }
    const double delta = (y2 - y0) / denom;
    return xPeak + std::clamp(delta, -1.0, 1.0);
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
    if (dominantFrequencyHz <= 10.0 || !std::isfinite(dominantFrequencyHz)) {
        return "~C1 (32.7 Hz)";
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
    if (numSamples <= 0 || sampleRate <= 1000.0 || numChannels <= 0 || channelData == nullptr) {
        return res;
    }

    res.durationSamples = numSamples;
    res.durationSeconds = static_cast<double>(numSamples) / sampleRate;

    // 1. Calculate Peak and RMS level across channels
    double sumSquares = 0.0;
    float peak = 0.0f;
    for (int64_t i = 0; i < numSamples; ++i) {
        float mixSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                const float s = channelData[ch][i];
                mixSample += s;
                peak = std::max(peak, std::abs(s));
            }
        }
        mixSample /= static_cast<float>(numChannels);
        sumSquares += static_cast<double>(mixSample * mixSample);
    }
    res.peakAmplitude = peak;
    res.rmsAmplitude = static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numSamples)));

    if (peak < 1e-4f) {
        // Complete silence
        return res;
    }

    // 2. Select an analysis window for pitch detection (up to 4096 samples around body)
    constexpr int kMaxWindow = 4096;
    const int windowSize = static_cast<int>(std::min<int64_t>(numSamples, kMaxWindow));

    std::vector<float> mono(static_cast<size_t>(windowSize));
    // Find the offset of the transient / energy start
    int64_t startOffset = 0;
    for (int64_t i = 0; i < std::min<int64_t>(numSamples, 2048); ++i) {
        float maxVal = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                maxVal = std::max(maxVal, std::abs(channelData[ch][i]));
            }
        }
        if (maxVal > 0.05f * peak) {
            startOffset = i;
            break;
        }
    }

    if (startOffset + windowSize > numSamples) {
        startOffset = std::max<int64_t>(0, numSamples - windowSize);
    }

    for (int i = 0; i < windowSize; ++i) {
        float s = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch) {
            if (channelData[ch] != nullptr) {
                s += channelData[ch][startOffset + i];
            }
        }
        mono[static_cast<size_t>(i)] = s / static_cast<float>(numChannels);
    }

    // 3. Normalized Square Difference Function (NSDF)
    // Search frequency range 25 Hz to 2400 Hz
    const int minLag = std::max(2, static_cast<int>(std::floor(sampleRate / 2400.0)));
    const int maxLag = std::min(windowSize / 2, static_cast<int>(std::ceil(sampleRate / 25.0)));

    if (maxLag <= minLag) {
        return res;
    }

    std::vector<double> nsdf(static_cast<size_t>(maxLag + 1), 0.0);
    const int correlationLength = windowSize - maxLag;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        double m = 0.0;
        double r = 0.0;
        for (int j = 0; j < correlationLength; ++j) {
            const double s1 = mono[static_cast<size_t>(j)];
            const double s2 = mono[static_cast<size_t>(j + lag)];
            r += s1 * s2;
            m += s1 * s1 + s2 * s2;
        }
        if (m > 1e-12) {
            nsdf[static_cast<size_t>(lag)] = (2.0 * r) / m;
        }
    }

    // 4. Find all local maxima of NSDF
    struct Peak {
        int lag = 0;
        double value = 0.0;
    };
    std::vector<Peak> peaks;
    double maxPeakVal = 0.0;

    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        const double curr = nsdf[static_cast<size_t>(lag)];
        const double prev = nsdf[static_cast<size_t>(lag - 1)];
        const double next = nsdf[static_cast<size_t>(lag + 1)];

        if (curr > prev && curr > next && curr > 0.0) {
            peaks.push_back({ lag, curr });
            maxPeakVal = std::max(maxPeakVal, curr);
        }
    }

    int bestLag = -1;
    if (maxPeakVal >= 0.40) {
        // Choose the first peak that exceeds 80% of maxPeakVal (MPM threshold)
        const double threshold = std::max(0.40, 0.80 * maxPeakVal);
        for (const auto& p : peaks) {
            if (p.value >= threshold) {
                bestLag = p.lag;
                break;
            }
        }
    } else if (!peaks.empty()) {
        // For inharmonic or noisy percussive sounds, find the strongest body resonance
        double bestVal = 0.0;
        for (const auto& p : peaks) {
            if (p.value > bestVal && p.value > 0.10) {
                bestVal = p.value;
                bestLag = p.lag;
            }
        }
    }

    if (bestLag > 0) {
        const double y0 = nsdf[static_cast<size_t>(bestLag - 1)];
        const double y1 = nsdf[static_cast<size_t>(bestLag)];
        const double y2 = nsdf[static_cast<size_t>(bestLag + 1)];
        const double refinedLag = parabolicInterpolation(y0, y1, y2, static_cast<double>(bestLag));

        if (refinedLag > 0.0) {
            res.dominantFrequencyHz = sampleRate / refinedLag;
            res.hasClearTone = (maxPeakVal >= 0.40);
            frequencyToNote(res.dominantFrequencyHz, res.midiNote, res.detuneCents, res.noteName);
            return res;
        }
    }

    // Fallback: estimate dominant zero-crossing or spectral centroid frequency
    int zeroCrossings = 0;
    for (int i = 1; i < windowSize; ++i) {
        if ((mono[static_cast<size_t>(i - 1)] <= 0.0f && mono[static_cast<size_t>(i)] > 0.0f) ||
            (mono[static_cast<size_t>(i - 1)] >= 0.0f && mono[static_cast<size_t>(i)] < 0.0f)) {
            zeroCrossings++;
        }
    }
    double zcrFreq = (static_cast<double>(zeroCrossings) * sampleRate) / (2.0 * static_cast<double>(windowSize));
    if (zcrFreq <= 10.0 || !std::isfinite(zcrFreq)) {
        zcrFreq = 130.81; // default to C3
    }
    res.dominantFrequencyHz = zcrFreq;
    res.hasClearTone = false;
    frequencyToNote(zcrFreq, res.midiNote, res.detuneCents, res.noteName);

    return res;
}

} // namespace openfruit::core
