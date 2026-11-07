#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SonicAnalyzer.h"
#include <cmath>
#include <vector>

using namespace openfruit::core;

namespace {
constexpr double kPi = 3.14159265358979323846;
}

TEST_CASE("SonicAnalyzer detects pure sine tones accurately", "[SonicAnalyzer]") {
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 44100; // 1 second

    SECTION("Detects A4 (440 Hz)") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / sampleRate);
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(440.0, 2.5));
        REQUIRE(res.midiNote == 69);
        REQUIRE(res.noteName == "A4");
        REQUIRE_THAT(res.durationSeconds, Catch::Matchers::WithinAbs(1.0, 0.001));
    }

    SECTION("Detects C3 (130.81 Hz)") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = std::sin(2.0 * kPi * 130.81 * static_cast<double>(i) / sampleRate);
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(130.81, 1.5));
        REQUIRE(res.midiNote == 48);
        REQUIRE(res.noteName == "C3");
    }

    SECTION("Detects low A1 (55.0 Hz)") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = std::sin(2.0 * kPi * 55.0 * static_cast<double>(i) / sampleRate);
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(55.0, 1.0));
        REQUIRE(res.midiNote == 33);
        REQUIRE(res.noteName == "A1");
    }
}

TEST_CASE("SonicAnalyzer utility conversions", "[SonicAnalyzer]") {
    int note = 0;
    int cents = 0;
    std::string name;

    SonicAnalyzer::frequencyToNote(440.0, note, cents, name);
    REQUIRE(note == 69);
    REQUIRE(cents == 0);
    REQUIRE(name == "A4");

    SonicAnalyzer::frequencyToNote(261.63, note, cents, name);
    REQUIRE(note == 60);
    REQUIRE(name == "C4");

    REQUIRE(SonicAnalyzer::midiNoteToName(60) == "C4");
    REQUIRE(SonicAnalyzer::midiNoteToName(61) == "C#4");
    REQUIRE(SonicAnalyzer::midiNoteToName(62) == "D4");
    REQUIRE(SonicAnalyzer::midiNoteToName(69) == "A4");
}

TEST_CASE("SonicAnalyzer always yields closest pitch for inharmonic and noise sounds", "[SonicAnalyzer]") {
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 22050; // 0.5s

    SECTION("White noise produces estimated closest pitch and formatted tone") {
        std::vector<float> noise(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            noise[static_cast<size_t>(i)] = static_cast<float>((std::rand() % 2000 - 1000) / 1000.0);
        }

        const auto res = SonicAnalyzer::analyze(noise.data(), numSamples, sampleRate);
        REQUIRE(res.dominantFrequencyHz > 10.0);
        REQUIRE(res.midiNote >= 0);
        REQUIRE(res.midiNote <= 127);
        REQUIRE_FALSE(res.noteName.empty());

        const std::string formatted = res.formattedTone();
        REQUIRE_FALSE(formatted.empty());
        REQUIRE(formatted.find("Unpitched") == std::string::npos);
        REQUIRE(formatted.find("Hz") != std::string::npos);
    }

    SECTION("Highpass filtered noise (hi-hat) yields high closest pitch") {
        std::vector<float> hat(numSamples);
        float prev = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-20.0f * t);
            const float white = static_cast<float>((std::rand() % 2000 - 1000) / 1000.0);
            const float hp = white - prev * 0.8f;
            prev = white;
            hat[static_cast<size_t>(i)] = hp * env;
        }

        const auto res = SonicAnalyzer::analyze(hat.data(), numSamples, sampleRate);
        REQUIRE(res.dominantFrequencyHz > 100.0);
        REQUIRE(res.midiNote >= 40);
        REQUIRE_FALSE(res.noteName.empty());

        const std::string formatted = res.formattedTone();
        REQUIRE_FALSE(formatted.empty());
        REQUIRE(formatted.find("Unpitched") == std::string::npos);
    }
}
