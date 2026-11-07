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
