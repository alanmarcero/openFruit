#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SonicAnalyzer.h"
#include "WavReader.h"
#include <cmath>
#include <cstring>
#include <vector>

using namespace openfruit::core;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
}

TEST_CASE("SonicAnalyzer detects pure sine tones accurately", "[SonicAnalyzer]") {
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 44100; // 1 second

    SECTION("Detects A4 (440 Hz)") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = static_cast<float>(
                std::sin(kTwoPi * 440.0 * static_cast<double>(i) / sampleRate));
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(440.0, 0.5));
        REQUIRE(res.midiNote == 69);
        REQUIRE(res.detuneCents == 0);
        REQUIRE(res.noteName == "A4");
        REQUIRE_THAT(res.durationSeconds, Catch::Matchers::WithinAbs(1.0, 0.001));
    }

    SECTION("Detects C3 (130.81 Hz)") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = static_cast<float>(
                std::sin(kTwoPi * 130.8128 * static_cast<double>(i) / sampleRate));
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(130.8128, 0.5));
        REQUIRE(res.midiNote == 48);
        REQUIRE(res.noteName == "C3");
    }

    SECTION("Detects low A1 (55.0 Hz) and sub-bass C1 (32.70 Hz) including at 96 kHz") {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[static_cast<size_t>(i)] = static_cast<float>(
                std::sin(kTwoPi * 55.0 * static_cast<double>(i) / sampleRate));
        }

        const auto res = SonicAnalyzer::analyze(buffer.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(55.0, 0.5));
        REQUIRE(res.midiNote == 33);
        REQUIRE(res.noteName == "A1");

        // Sub-bass C1 (32.703 Hz) at 96 kHz sample rate
        constexpr double highSr = 96000.0;
        constexpr int highLen = 48000;
        std::vector<float> subBass(highLen);
        for (int i = 0; i < highLen; ++i) {
            subBass[static_cast<size_t>(i)] = static_cast<float>(
                std::sin(kTwoPi * 32.703 * static_cast<double>(i) / highSr));
        }
        const auto subRes = SonicAnalyzer::analyze(subBass.data(), highLen, highSr);
        REQUIRE(subRes.hasClearTone);
        REQUIRE_THAT(subRes.dominantFrequencyHz, Catch::Matchers::WithinAbs(32.703, 0.5));
        REQUIRE(subRes.midiNote == 24);
        REQUIRE(subRes.noteName == "C1");
    }
}

TEST_CASE("SonicAnalyzer accurately tracks settled body pitch of pitch-swept drums", "[SonicAnalyzer]") {
    constexpr double sampleRate = 44100.0;

    SECTION("808 kick with steep initial pitch envelope (208 Hz -> 48 Hz G1)") {
        const int len = static_cast<int>(sampleRate * 0.45);
        std::vector<float> kick(static_cast<size_t>(len));
        double phase = 0.0;
        for (int i = 0; i < len; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double env = std::exp(-7.0 * t);
            const double freq = 48.999 + 160.0 * std::exp(-35.0 * t); // Settles to G1 (49.0 Hz)
            phase += kTwoPi * freq / sampleRate;
            kick[static_cast<size_t>(i)] = static_cast<float>(std::sin(phase) * env);
        }

        const auto res = SonicAnalyzer::analyze(kick.data(), len, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(49.0, 2.0));
        REQUIRE(res.midiNote == 31);
        REQUIRE(res.noteName == "G1");
    }

    SECTION("Electronic tom with pitch drop (200 Hz -> 110 Hz A2)") {
        const int len = static_cast<int>(sampleRate * 0.35);
        std::vector<float> tom(static_cast<size_t>(len));
        double phase = 0.0;
        for (int i = 0; i < len; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double env = std::exp(-10.0 * t);
            const double freq = 110.0 + 90.0 * std::exp(-25.0 * t);
            phase += kTwoPi * freq / sampleRate;
            tom[static_cast<size_t>(i)] = static_cast<float>(std::sin(phase) * env);
        }

        const auto res = SonicAnalyzer::analyze(tom.data(), len, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(110.0, 2.5));
        REQUIRE(res.midiNote == 45);
        REQUIRE(res.noteName == "A2");
    }

    SECTION("Short perc blip with fast transient chirp (630 Hz -> 329.6 Hz E4)") {
        const int len = static_cast<int>(sampleRate * 0.12);
        std::vector<float> perc(static_cast<size_t>(len));
        double phase = 0.0;
        for (int i = 0; i < len; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double env = std::exp(-35.0 * t);
            const double freq = 329.63 + 300.0 * std::exp(-60.0 * t);
            phase += kTwoPi * freq / sampleRate;
            perc[static_cast<size_t>(i)] = static_cast<float>(std::sin(phase) * env);
        }

        const auto res = SonicAnalyzer::analyze(perc.data(), len, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(329.63, 6.0));
        REQUIRE(res.midiNote == 64);
        REQUIRE(res.noteName == "E4");
    }
}

TEST_CASE("SonicAnalyzer resolves complex harmonics, missing fundamental, leading silence, and stereo phase", "[SonicAnalyzer]") {
    constexpr double sampleRate = 44100.0;
    constexpr int numSamples = 22050;

    SECTION("Missing fundamental harmonic series (220 + 330 + 440 + 550 Hz -> 110 Hz A2)") {
        std::vector<float> wave(numSamples);
        const double f0 = 110.0;
        for (int i = 0; i < numSamples; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            wave[static_cast<size_t>(i)] = static_cast<float>(
                0.50 * std::sin(kTwoPi * (2.0 * f0) * t) +
                0.35 * std::sin(kTwoPi * (3.0 * f0) * t) +
                0.25 * std::sin(kTwoPi * (4.0 * f0) * t) +
                0.15 * std::sin(kTwoPi * (5.0 * f0) * t)
            );
        }

        const auto res = SonicAnalyzer::analyze(wave.data(), numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(110.0, 1.0));
        REQUIRE(res.midiNote == 45);
        REQUIRE(res.noteName == "A2");
    }

    SECTION("Sample with 100ms leading silence, DC offset, and inverted stereo channels") {
        const int silenceSamples = 4410; // 100 ms (> old 2048 limit)
        std::vector<float> left(numSamples, 0.25f);  // DC offset +0.25
        std::vector<float> right(numSamples, -0.25f);
        for (int i = silenceSamples; i < numSamples; ++i) {
            const float s = static_cast<float>(0.6 * std::sin(kTwoPi * 261.6256 * static_cast<double>(i) / sampleRate));
            left[static_cast<size_t>(i)] += s;
            right[static_cast<size_t>(i)] -= s; // Inverted polarity on right channel
        }

        const float* channels[2] = { left.data(), right.data() };
        const auto res = SonicAnalyzer::analyze(channels, 2, numSamples, sampleRate);
        REQUIRE(res.hasClearTone);
        REQUIRE_THAT(res.dominantFrequencyHz, Catch::Matchers::WithinAbs(261.6256, 1.0));
        REQUIRE(res.midiNote == 60);
        REQUIRE(res.noteName == "C4");
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
        REQUIRE(res.dominantFrequencyHz > 100.0);
        REQUIRE(res.dominantFrequencyHz < 5500.0);
        REQUIRE(res.midiNote >= 45);
        REQUIRE(res.midiNote <= 112);
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
        REQUIRE(res.dominantFrequencyHz > 1500.0);
        REQUIRE(res.dominantFrequencyHz < 5500.0);
        REQUIRE(res.midiNote >= 90);
        REQUIRE_FALSE(res.noteName.empty());

        const std::string formatted = res.formattedTone();
        REQUIRE_FALSE(formatted.empty());
        REQUIRE(formatted.find("Unpitched") == std::string::npos);
    }

    SECTION("Silent or empty sample still returns valid fallback estimated pitch") {
        std::vector<float> silence(4096, 0.0f);
        const auto res = SonicAnalyzer::analyze(silence.data(), 4096, sampleRate);
        REQUIRE(res.dominantFrequencyHz > 10.0);
        REQUIRE(res.noteName != "---");
        REQUIRE_FALSE(res.hasClearTone);
    }
}

TEST_CASE("WavReader supports WAVE_FORMAT_EXTENSIBLE and AIFF samples for pitch analysis", "[WavReader]") {
    constexpr int sr = 44100;
    constexpr int numFrames = 8192;

    // Build a 16-bit WAVE_FORMAT_EXTENSIBLE (0xFFFE) buffer with a 440 Hz sine wave
    const uint32_t dataBytes = numFrames * 2;
    const uint32_t fmtBytes = 40;
    std::vector<uint8_t> wavExt(12 + 8 + fmtBytes + 8 + dataBytes, 0);

    std::memcpy(wavExt.data(), "RIFF", 4);
    const uint32_t riffSize = static_cast<uint32_t>(wavExt.size() - 8);
    std::memcpy(wavExt.data() + 4, &riffSize, 4);
    std::memcpy(wavExt.data() + 8, "WAVEfmt ", 8);
    std::memcpy(wavExt.data() + 16, &fmtBytes, 4);

    const uint16_t formatExt = 0xFFFE;
    const uint16_t channels = 1;
    const uint32_t sampleRate = sr;
    const uint32_t byteRate = sr * 2;
    const uint16_t blockAlign = 2;
    const uint16_t bitsPerSample = 16;
    const uint16_t cbSize = 22;
    const uint16_t validBits = 16;
    const uint16_t subFormatPcm = 1;

    std::memcpy(wavExt.data() + 20, &formatExt, 2);
    std::memcpy(wavExt.data() + 22, &channels, 2);
    std::memcpy(wavExt.data() + 24, &sampleRate, 4);
    std::memcpy(wavExt.data() + 28, &byteRate, 4);
    std::memcpy(wavExt.data() + 32, &blockAlign, 2);
    std::memcpy(wavExt.data() + 34, &bitsPerSample, 2);
    std::memcpy(wavExt.data() + 36, &cbSize, 2);
    std::memcpy(wavExt.data() + 38, &validBits, 2);
    std::memcpy(wavExt.data() + 44, &subFormatPcm, 2);

    std::memcpy(wavExt.data() + 60, "data", 4);
    std::memcpy(wavExt.data() + 64, &dataBytes, 4);

    for (int i = 0; i < numFrames; ++i) {
        const double s = std::sin(kTwoPi * 440.0 * static_cast<double>(i) / sr);
        const int16_t pcm = static_cast<int16_t>(s * 30000.0);
        std::memcpy(wavExt.data() + 68 + i * 2, &pcm, 2);
    }

    const auto decoded = WavReader::readWavFromMemory(wavExt.data(), wavExt.size(), true, 4096);
    REQUIRE(decoded.isValid);
    REQUIRE(decoded.numSamples == numFrames);
    REQUIRE(decoded.monoChannel.size() == 4096);

    const auto tone = SonicAnalyzer::analyze(decoded.monoChannel.data(),
                                             static_cast<int64_t>(decoded.monoChannel.size()),
                                             decoded.sampleRate);
    REQUIRE(tone.hasClearTone);
    REQUIRE(tone.noteName == "A4");
}
