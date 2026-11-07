#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

namespace openfruit::core {

struct ToneAnalysisResult {
    double dominantFrequencyHz = 0.0;
    std::string noteName = "---";
    int midiNote = 60;
    int detuneCents = 0;
    double durationSeconds = 0.0;
    int64_t durationSamples = 0;
    float peakAmplitude = 0.0f;
    float rmsAmplitude = 0.0f;
    bool hasClearTone = false;

    [[nodiscard]] std::string formattedTone() const;
    [[nodiscard]] std::string formattedDuration() const;
};

class SonicAnalyzer {
public:
    SonicAnalyzer() = default;

    // Fast analysis of a monophonic or stereo float buffer.
    static ToneAnalysisResult analyze(const float* const* channelData,
                                      int numChannels,
                                      int64_t numSamples,
                                      double sampleRate);

    // Convenience overload for mono buffer
    static ToneAnalysisResult analyze(const float* monoData,
                                      int64_t numSamples,
                                      double sampleRate);

    // Convert frequency in Hz to closest MIDI note and cents offset
    static void frequencyToNote(double freqHz,
                                int& outMidiNote,
                                int& outCents,
                                std::string& outNoteName);

    // Convert MIDI note number to note string (e.g. 60 -> "C4", 69 -> "A4")
    static std::string midiNoteToName(int midiNote);
};

} // namespace openfruit::core
