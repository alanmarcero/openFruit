#include "DefaultKit.h"
#include <cmath>
#include <random>

namespace openfruit::kit {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;

std::shared_ptr<openfruit::core::AudioSampleBuffer> createBuffer(const std::string& name,
                                                                 const std::vector<float>& mono,
                                                                 double sampleRate) {
    auto buf = std::make_shared<openfruit::core::AudioSampleBuffer>();
    buf->sampleName = name;
    buf->filePath = "[Factory] " + name;
    buf->sampleRate = sampleRate;
    buf->numChannels = 1;
    buf->numSamples = static_cast<int64_t>(mono.size());
    buf->channels.resize(1);
    buf->channels[0] = mono;
    buf->isLoaded = true;

    buf->toneResult = openfruit::core::SonicAnalyzer::analyze(buf->channels[0].data(),
                                                              buf->numSamples,
                                                              sampleRate);
    return buf;
}

} // namespace

std::vector<std::shared_ptr<openfruit::core::AudioSampleBuffer>> createDefaultKit(double sampleRate) {
    std::vector<std::shared_ptr<openfruit::core::AudioSampleBuffer>> kit;
    kit.reserve(16);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);

    // 1. Kick 808
    {
        const int len = static_cast<int>(sampleRate * 0.45);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-7.0f * t);
            const float pitchFreq = 48.0f + 160.0f * std::exp(-35.0f * t);
            phase += kTwoPi * pitchFreq / static_cast<float>(sampleRate);
            if (phase >= kTwoPi) phase -= kTwoPi;
            data[static_cast<size_t>(i)] = std::sin(phase) * env;
        }
        kit.push_back(createBuffer("808 Deep Kick", data, sampleRate));
    }

    // 2. Snare Punch
    {
        const int len = static_cast<int>(sampleRate * 0.25);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float envTone = std::exp(-22.0f * t);
            const float envNoise = std::exp(-15.0f * t);
            const float freq = 180.0f + 120.0f * std::exp(-40.0f * t);
            phase += kTwoPi * freq / static_cast<float>(sampleRate);
            if (phase >= kTwoPi) phase -= kTwoPi;
            const float tone = std::sin(phase) * envTone * 0.5f;
            const float noise = noiseDist(rng) * envNoise * 0.5f;
            data[static_cast<size_t>(i)] = tone + noise;
        }
        kit.push_back(createBuffer("Snare Punch", data, sampleRate));
    }

    // 3. Clap Cyber
    {
        const int len = static_cast<int>(sampleRate * 0.3);
        std::vector<float> data(static_cast<size_t>(len), 0.0f);
        for (int burst = 0; burst < 3; ++burst) {
            const int offset = burst * static_cast<int>(sampleRate * 0.012);
            for (int i = 0; i < len - offset; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
                const float env = std::exp(-30.0f * t);
                data[static_cast<size_t>(offset + i)] += noiseDist(rng) * env * 0.35f;
            }
        }
        kit.push_back(createBuffer("Clap Cyber", data, sampleRate));
    }

    // 4. Closed Hi-Hat
    {
        const int len = static_cast<int>(sampleRate * 0.06);
        std::vector<float> data(static_cast<size_t>(len));
        float prev = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-60.0f * t);
            const float white = noiseDist(rng);
            const float hp = white - prev; // simple highpass
            prev = white;
            data[static_cast<size_t>(i)] = hp * env * 0.7f;
        }
        kit.push_back(createBuffer("Hat Closed", data, sampleRate));
    }

    // 5. Open Hi-Hat
    {
        const int len = static_cast<int>(sampleRate * 0.35);
        std::vector<float> data(static_cast<size_t>(len));
        float prev = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-9.0f * t);
            const float white = noiseDist(rng);
            const float hp = white - prev;
            prev = white;
            data[static_cast<size_t>(i)] = hp * env * 0.6f;
        }
        kit.push_back(createBuffer("Hat Open", data, sampleRate));
    }

    // 6. Rimshot
    {
        const int len = static_cast<int>(sampleRate * 0.08);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-50.0f * t);
            phase += kTwoPi * 420.0f / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = (std::sin(phase) + 0.3f * noiseDist(rng)) * env * 0.8f;
        }
        kit.push_back(createBuffer("Rimshot", data, sampleRate));
    }

    // 7. Low Tom (A1 ~ 55 Hz)
    {
        const int len = static_cast<int>(sampleRate * 0.4);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-8.0f * t);
            const float freq = 65.0f + 70.0f * std::exp(-25.0f * t);
            phase += kTwoPi * freq / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = std::sin(phase) * env * 0.85f;
        }
        kit.push_back(createBuffer("Tom Low", data, sampleRate));
    }

    // 8. High Tom (D2 ~ 73.4 Hz)
    {
        const int len = static_cast<int>(sampleRate * 0.35);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-10.0f * t);
            const float freq = 110.0f + 90.0f * std::exp(-25.0f * t);
            phase += kTwoPi * freq / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = std::sin(phase) * env * 0.85f;
        }
        kit.push_back(createBuffer("Tom High", data, sampleRate));
    }

    // 9. Shaker Pulse
    {
        const int len = static_cast<int>(sampleRate * 0.1);
        std::vector<float> data(static_cast<size_t>(len));
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-35.0f * t);
            data[static_cast<size_t>(i)] = noiseDist(rng) * env * 0.5f;
        }
        kit.push_back(createBuffer("Shaker Pulse", data, sampleRate));
    }

    // 10. Synth Bass Note (F1 ~ 43.6 Hz)
    {
        const int len = static_cast<int>(sampleRate * 0.6);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-3.5f * t);
            phase += kTwoPi * 43.65f / static_cast<float>(sampleRate);
            if (phase >= kTwoPi) phase -= kTwoPi;
            data[static_cast<size_t>(i)] = std::sin(phase) * env * 0.9f;
        }
        kit.push_back(createBuffer("Sub Bass F1", data, sampleRate));
    }

    // 11. Cowbell Vintage
    {
        const int len = static_cast<int>(sampleRate * 0.2);
        std::vector<float> data(static_cast<size_t>(len));
        float p1 = 0.0f, p2 = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-15.0f * t);
            p1 += kTwoPi * 540.0f / static_cast<float>(sampleRate);
            p2 += kTwoPi * 800.0f / static_cast<float>(sampleRate);
            const float square1 = (std::sin(p1) > 0.0f ? 1.0f : -1.0f);
            const float square2 = (std::sin(p2) > 0.0f ? 1.0f : -1.0f);
            data[static_cast<size_t>(i)] = (square1 + square2) * 0.35f * env;
        }
        kit.push_back(createBuffer("Cowbell Vintage", data, sampleRate));
    }

    // 12. Cyber Stab (A3 ~ 220 Hz)
    {
        const int len = static_cast<int>(sampleRate * 0.3);
        std::vector<float> data(static_cast<size_t>(len));
        float p1 = 0.0f, p2 = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-11.0f * t);
            p1 += kTwoPi * 220.0f / static_cast<float>(sampleRate);
            p2 += kTwoPi * 220.7f / static_cast<float>(sampleRate); // slight detune
            data[static_cast<size_t>(i)] = (std::sin(p1) + 0.6f * std::sin(p2)) * env * 0.5f;
        }
        kit.push_back(createBuffer("Cyber Stab A3", data, sampleRate));
    }

    // 13. Perc Blip (E4 ~ 329.6 Hz)
    {
        const int len = static_cast<int>(sampleRate * 0.12);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-35.0f * t);
            const float freq = 329.6f + 300.0f * std::exp(-60.0f * t);
            phase += kTwoPi * freq / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = std::sin(phase) * env * 0.8f;
        }
        kit.push_back(createBuffer("Perc Blip E4", data, sampleRate));
    }

    // 14. Metallic Hit
    {
        const int len = static_cast<int>(sampleRate * 0.25);
        std::vector<float> data(static_cast<size_t>(len));
        float p1 = 0.0f, p2 = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-18.0f * t);
            p1 += kTwoPi * 1150.0f / static_cast<float>(sampleRate);
            p2 += kTwoPi * 1740.0f / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = (std::sin(p1) * std::sin(p2) + 0.2f * noiseDist(rng)) * env * 0.6f;
        }
        kit.push_back(createBuffer("Metallic Hit", data, sampleRate));
    }

    // 15. Glitch Zap
    {
        const int len = static_cast<int>(sampleRate * 0.15);
        std::vector<float> data(static_cast<size_t>(len));
        float phase = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-25.0f * t);
            const float freq = 120.0f + 1400.0f * std::exp(-40.0f * t);
            phase += kTwoPi * freq / static_cast<float>(sampleRate);
            data[static_cast<size_t>(i)] = std::sin(phase) * env * 0.85f;
        }
        kit.push_back(createBuffer("Glitch Zap", data, sampleRate));
    }

    // 16. Cyber Crash
    {
        const int len = static_cast<int>(sampleRate * 0.7);
        std::vector<float> data(static_cast<size_t>(len));
        float prev = 0.0f;
        for (int i = 0; i < len; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float env = std::exp(-4.5f * t);
            const float white = noiseDist(rng);
            const float hp = white - prev * 0.8f;
            prev = white;
            data[static_cast<size_t>(i)] = hp * env * 0.45f;
        }
        kit.push_back(createBuffer("Cyber Crash", data, sampleRate));
    }

    return kit;
}

} // namespace openfruit::kit
