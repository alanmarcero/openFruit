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

static std::vector<std::shared_ptr<openfruit::core::AudioSampleBuffer>> createLegacyKit(double sampleRate) {
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
        kit.push_back(createBuffer("808 kick", data, sampleRate));
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
        kit.push_back(createBuffer("Snare", data, sampleRate));
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
        kit.push_back(createBuffer("Clap", data, sampleRate));
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
        kit.push_back(createBuffer("Closed hat", data, sampleRate));
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
        kit.push_back(createBuffer("Open hat", data, sampleRate));
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
        kit.push_back(createBuffer("Low tom", data, sampleRate));
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
        kit.push_back(createBuffer("High tom", data, sampleRate));
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
        kit.push_back(createBuffer("Shaker", data, sampleRate));
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
        kit.push_back(createBuffer("Sub bass F1", data, sampleRate));
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
        kit.push_back(createBuffer("Cowbell", data, sampleRate));
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
        kit.push_back(createBuffer("Stab A3", data, sampleRate));
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
        kit.push_back(createBuffer("Perc E4", data, sampleRate));
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
        kit.push_back(createBuffer("Metal hit", data, sampleRate));
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
        kit.push_back(createBuffer("Zap", data, sampleRate));
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
        kit.push_back(createBuffer("Crash", data, sampleRate));
    }

    return kit;
}

std::vector<std::shared_ptr<core::AudioSampleBuffer>> createDefaultKit(double sampleRate, bool legacy) {
    if (legacy) return createLegacyKit(sampleRate);
    enum class Voice { drum, snare, clap, metal, wood, shaker, bass, chord, zap };
    struct Sound { const char* name; Voice voice; double hz, sweep, length, air, level; };
    constexpr Sound sounds[] = {
        {"Navel kick", Voice::drum, 47, 152, .39, .12, .91},
        {"Rind snare", Voice::snare, 192, 80, .23, .68, .75},
        {"Palm clap", Voice::clap, 940, 0, .21, 1, .71},
        {"Pip hat", Voice::metal, 2730, 0, .095, .62, .54},
        {"Peel hat", Voice::metal, 2060, 0, .29, .83, .62},
        {"Bamboo rim", Voice::wood, 718, 88, .11, .13, .66},
        {"Clay tom", Voice::drum, 99, 69, .28, .06, .74},
        {"Glass tom", Voice::drum, 157, 124, .21, .08, .70},
        {"Seed shaker", Voice::shaker, 3640, 0, .14, 1, .52},
        {"Pulp sub", Voice::bass, 43.654, 0, .53, 0, .84},
        {"Brass bell", Voice::wood, 554, 14, .30, .04, .60},
        {"Citrus stab", Voice::chord, 220, 0, .34, .02, .64},
        {"Pith pluck", Voice::wood, 329.63, 50, .21, .06, .65},
        {"Steel seed", Voice::metal, 967, 0, .31, .21, .57},
        {"Ribbon zap", Voice::zap, 93, 1580, .22, .04, .59},
        {"Orchard ride", Voice::metal, 1279, 0, .65, .48, .62}
    };
    std::mt19937 random(18723);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);
    std::vector<std::shared_ptr<core::AudioSampleBuffer>> kit;
    for (const auto& sound : sounds) {
        std::vector<float> pcm(static_cast<size_t>(sampleRate * sound.length));
        double slow = 0, fast = 0, peak = 0;
        const double slowPole = 1.0 - std::exp(-kTwoPi * 650.0 / sampleRate);
        const double fastPole = 1.0 - std::exp(-kTwoPi * 5100.0 / sampleRate);
        for (size_t i = 0; i < pcm.size(); ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            const double raw = noise(random);
            slow += slowPole * (raw - slow);
            fast += fastPole * (raw - fast);
            const double high = raw - fast, band = fast - slow;
            const double phase = kTwoPi * sound.hz * t;
            const double body = std::exp(-6.0 * t / sound.length);
            const double strike = std::exp(-t / .0045);
            double value = 0;
            switch (sound.voice) {
                case Voice::drum:
                case Voice::zap: {
                    const double bend = sound.voice == Voice::zap ? .044 : .018;
                    const double swept = phase + kTwoPi * sound.sweep * bend * -std::expm1(-t / bend);
                    value = std::tanh(1.35 * std::sin(swept) + .24 * std::sin(2 * swept) * std::exp(-38*t)) * body
                          + sound.air * high * strike;
                    break;
                }
                case Voice::snare:
                    value = (.40 * std::sin(phase + .65 * -std::expm1(-75*t)) + .16 * std::sin(1.58 * phase))
                              * std::exp(-56*t) + sound.air * (band + .35*high) * body;
                    break;
                case Voice::clap: {
                    double burst = 0;
                    for (const double onset : {0.0, .009, .021, .035})
                        if (t >= onset) burst += std::exp(-(t-onset) / (onset > .03 ? .034 : .0028));
                    value = (band + .15*high) * burst;
                    break;
                }
                case Voice::metal:
                    value = ((.44*std::sin(phase) + .32*std::sin(1.51*phase) + .24*std::sin(2.037*phase))
                              * (1.0 - sound.air) + sound.air*(high + .17*band)) * body;
                    break;
                case Voice::wood:
                    value = (std::sin(phase) + .54*std::sin(1.48*phase) + .32*std::sin(2.73*phase)*std::exp(-45*t))
                              * body + sound.air*high*strike;
                    break;
                case Voice::shaker:
                    value = high * (1.0 - std::exp(-170*t)) * body * (.78 + .22*std::sin(715*t));
                    break;
                case Voice::bass:
                    value = (std::sin(phase) + .19*std::sin(2*phase) + .09*std::sin(3*phase)*std::exp(-16*t)) * body;
                    break;
                case Voice::chord:
                    for (const double ratio : {1.0, 1.189207, 1.498307})
                        value += (std::sin(ratio*phase) + .33*std::sin(2*ratio*phase)*std::exp(-24*t)) * body;
                    break;
            }
            value *= std::min(1.0, t/.0007) * std::min(1.0, (sound.length-t)/.006);
            pcm[i] = static_cast<float>(value);
            peak = std::max(peak, std::abs(value));
        }
        for (auto& sample : pcm) sample *= static_cast<float>(sound.level / std::max(.001,peak));
        kit.push_back(createBuffer(sound.name, pcm, sampleRate));
    }
    return kit;
}

} // namespace openfruit::kit
