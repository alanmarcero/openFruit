#include <catch2/catch_test_macros.hpp>
#include "MidiEventProcessor.h"
#include <vector>

TEST_CASE("OpenFruit releases MIDI voices at their event sample offset", "[plugin][midi]") {
    openfruit::core::FruitEngine engine;
    engine.prepare(8000.0, 256);

    auto sample = std::make_shared<openfruit::core::AudioSampleBuffer>();
    sample->sampleRate = 8000.0;
    sample->numChannels = 1;
    sample->numSamples = 2048;
    sample->channels = {std::vector<float>(2048, 1.0f)};
    sample->isLoaded = true;
    engine.loadSampleForTrack(0, sample);
    engine.getTrack(0).amplitudeReleaseSeconds = 0.02f;

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(127)), 20);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 80);

    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    openfruit::plugin::processMidiBlock(engine, midi, left.data(), right.data(), 256,
                                        120.0, 0.0, false, 8000.0);

    REQUIRE(left[19] == 0.0f);
    REQUIRE(left[20] > 0.5f);
    REQUIRE(left[79] > 0.5f);
    REQUIRE(left[239] < 0.01f);
}
