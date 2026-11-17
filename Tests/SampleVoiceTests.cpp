#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "SampleVoice.h"
#include <vector>

using namespace openfruit::core;

TEST_CASE("SampleVoice playback and pitch tuning", "[SampleVoice]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 1000;
    sample->channels.resize(1);
    sample->channels[0].assign(1000, 0.5f); // DC offset 0.5
    sample->isLoaded = true;

    SampleVoice voice;
    REQUIRE_FALSE(voice.isPlaying());

    // Start with 0 semitone offset
    voice.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f);
    REQUIRE(voice.isPlaying());

    std::vector<float> left(500, 0.0f);
    std::vector<float> right(500, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 500);

    REQUIRE(voice.isPlaying());
    // Equal power center pan has left == right > 0
    REQUIRE(left[0] > 0.0f);
    REQUIRE(right[0] > 0.0f);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(right[0], 0.01f));

    // Render remaining 500 samples
    voice.renderNextBlock(left.data(), right.data(), 500);
    // At end of sample, isPlaying becomes false
    voice.renderNextBlock(left.data(), right.data(), 10);
    REQUIRE_FALSE(voice.isPlaying());
}

TEST_CASE("SampleVoice plays a trimmed region in either direction", "[SampleVoice][regions]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 8;
    sample->channels = {{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f}};
    sample->isLoaded = true;

    VoiceSettings settings;
    settings.sampleStart = 0.25f;
    settings.sampleEnd = 0.75f;
    SampleVoice forward;
    forward.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f, settings);
    std::vector<float> left(4, 0.0f), right(4, 0.0f);
    forward.renderNextBlock(left.data(), right.data(), 4);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(0.2f * 0.7071068f, 0.001f));
    REQUIRE_THAT(left[3], Catch::Matchers::WithinAbs(0.5f * 0.7071068f, 0.001f));
    REQUIRE_FALSE(forward.isPlaying());

    settings.reverse = true;
    SampleVoice reversed;
    reversed.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f, settings);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    reversed.renderNextBlock(left.data(), right.data(), 4);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(0.5f * 0.7071068f, 0.001f));
    REQUIRE_THAT(left[3], Catch::Matchers::WithinAbs(0.2f * 0.7071068f, 0.001f));
    REQUIRE_FALSE(reversed.isPlaying());
}

TEST_CASE("SampleVoice trim boundaries do not interpolate adjacent audio", "[SampleVoice][regions]") {
    auto makeSample = [](float outside) {
        auto sample = std::make_shared<AudioSampleBuffer>();
        sample->sampleRate = 8000.0;
        sample->numChannels = 1;
        sample->numSamples = 10;
        sample->channels = {{outside, outside, outside, outside, 0.4f, 0.4f, 0.4f, 0.4f, outside, outside}};
        sample->isLoaded = true;
        return sample;
    };

    VoiceSettings settings;
    settings.sampleStart = 0.4f;
    settings.sampleEnd = 0.8f;
    SampleVoice quietOutside, loudOutside;
    quietOutside.start(makeSample(0.0f), 1.0f, 0, 0, 16000.0, 1.0f, 0.0f, settings);
    loudOutside.start(makeSample(0.9f), 1.0f, 0, 0, 16000.0, 1.0f, 0.0f, settings);
    std::vector<float> quietLeft(2, 0.0f), quietRight(2, 0.0f);
    std::vector<float> loudLeft(2, 0.0f), loudRight(2, 0.0f);
    quietOutside.renderNextBlock(quietLeft.data(), quietRight.data(), 2);
    loudOutside.renderNextBlock(loudLeft.data(), loudRight.data(), 2);

    REQUIRE_THAT(quietLeft[1], Catch::Matchers::WithinAbs(loudLeft[1], 1.0e-5f));
}

TEST_CASE("SampleVoice applies track amplitude ADSR and low-pass settings", "[SampleVoice][envelope]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 8000.0;
    sample->numChannels = 1;
    sample->numSamples = 256;
    sample->channels = {std::vector<float>(256, 1.0f)};
    sample->isLoaded = true;

    VoiceSettings settings;
    settings.attackSeconds = 0.004f;
    settings.decaySeconds = 0.004f;
    settings.sustainLevel = 0.5f;
    settings.releaseSeconds = 0.01f;
    settings.filterCutoffHz = 2000.0f;
    SampleVoice voice;
    voice.start(sample, 1.0f, 0, 0, 8000.0, 1.0f, 0.0f, settings);
    std::vector<float> left(8, 0.0f), right(8, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 8);
    REQUIRE(left[0] < left[7]);
    REQUIRE(left[7] < 0.5f);

    voice.noteOff();
    std::vector<float> release(16, 0.0f), releaseRight(16, 0.0f);
    voice.renderNextBlock(release.data(), releaseRight.data(), 16);
    REQUIRE(release[0] > release[15]);
    REQUIRE(release[15] < left[7]);
}

TEST_CASE("Fast forward and reverse sampling reject folded source harmonics", "[SampleVoice][resampling]") {
    for (double rate : {32000.0, 44100.0, 48000.0, 96000.0}) {
        for (const bool reverse : {false, true}) {
            auto sample = std::make_shared<AudioSampleBuffer>();
            sample->sampleRate = rate;
            sample->numChannels = 1;
            sample->numSamples = 8192;
            sample->channels = {std::vector<float>(8192)};
            sample->isLoaded = true;
            for (int i = 0; i < 8192; ++i)
                sample->channels.front()[static_cast<size_t>(i)] = .35f + (i % 2 == 0 ? .6f : -.6f);
            VoiceSettings settings;
            settings.reverse = reverse;
            settings.sampleStart = .25f;
            settings.sampleEnd = .75f;
            SampleVoice voice;
            voice.start(sample, 1, 24, 0, rate, 1, -1, settings);
            std::array<float,256> left{}, right{};
            voice.renderNextBlock(left.data(), right.data(), 256);
            for (int i = 16; i < 250; ++i) REQUIRE_THAT(left[static_cast<size_t>(i)], Catch::Matchers::WithinAbs(.35, .001));
        }
    }
}
