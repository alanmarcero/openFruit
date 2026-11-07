#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "FruitEngine.h"

using namespace openfruit::core;

TEST_CASE("FruitEngine track counts and routing", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    REQUIRE(engine.getActiveTrackCount() == 16);

    engine.setTrackCountMode(TrackCountMode::Tracks4);
    REQUIRE(engine.getActiveTrackCount() == 4);

    engine.setTrackCountMode(TrackCountMode::Tracks8);
    REQUIRE(engine.getActiveTrackCount() == 8);

    engine.setTrackCountMode(TrackCountMode::Tracks12);
    REQUIRE(engine.getActiveTrackCount() == 12);

    engine.setTrackCountMode(TrackCountMode::Tracks16);
    REQUIRE(engine.getActiveTrackCount() == 16);

    // Verify MIDI channel correspondence: Track index 0..15 maps to MIDI Channel 1..16
    for (int i = 0; i < 16; ++i) {
        REQUIRE(engine.getTrack(i).midiChannel == i + 1);
    }
}

TEST_CASE("FruitEngine pages and step editing", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    REQUIRE(engine.getActivePage() == PageIndex::PageA);
    engine.setActivePage(PageIndex::PageB);
    REQUIRE(engine.getActivePage() == PageIndex::PageB);

    // Edit step on Track 0, Page B, Step 4
    engine.setStepActive(0, PageIndex::PageB, 4, true);
    engine.setStepVelocity(0, PageIndex::PageB, 4, 0.9f);
    engine.setStepTune(0, PageIndex::PageB, 4, 7);

    auto stepB4 = engine.getStep(0, PageIndex::PageB, 4);
    REQUIRE(stepB4.active);
    REQUIRE_THAT(stepB4.velocity, Catch::Matchers::WithinAbs(0.9f, 0.01f));
    REQUIRE(stepB4.semitoneOffset == 7);

    // Page A should remain unedited
    auto stepA4 = engine.getStep(0, PageIndex::PageA, 4);
    REQUIRE_FALSE(stepA4.active);
}

TEST_CASE("FruitEngine live auditioning behavior", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    auto mainSample = std::make_shared<AudioSampleBuffer>();
    mainSample->sampleRate = 44100.0;
    mainSample->numChannels = 1;
    mainSample->numSamples = 1000;
    mainSample->channels.resize(1);
    mainSample->channels[0].assign(1000, 0.2f);
    mainSample->isLoaded = true;

    auto auditionSample = std::make_shared<AudioSampleBuffer>();
    auditionSample->sampleRate = 44100.0;
    auditionSample->numChannels = 1;
    auditionSample->numSamples = 1000;
    auditionSample->channels.resize(1);
    auditionSample->channels[0].assign(1000, 0.8f);
    auditionSample->isLoaded = true;

    engine.loadSampleForTrack(0, mainSample);
    REQUIRE(engine.getTrack(0).sample == mainSample);
    REQUIRE(engine.getTrack(0).auditionSample == nullptr);

    // Start live browsing audition
    engine.setAuditionSampleForTrack(0, auditionSample);
    REQUIRE(engine.getTrack(0).auditionSample == auditionSample);

    // Triggering track should play audition sample
    engine.triggerTrackManual(0, 1.0f, 0);

    // Commit audition sample
    engine.commitAuditionSample(0);
    REQUIRE(engine.getTrack(0).sample == auditionSample);
    REQUIRE(engine.getTrack(0).auditionSample == nullptr);
}

TEST_CASE("FruitEngine MIDI vs Sequencer modes", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    REQUIRE(engine.getInputControlMode() == InputControlMode::SequencerPads);
    engine.setInputControlMode(InputControlMode::MidiMode);
    REQUIRE(engine.getInputControlMode() == InputControlMode::MidiMode);

    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 1000;
    sample->channels.resize(1);
    sample->channels[0].assign(1000, 0.5f);
    sample->isLoaded = true;
    engine.loadSampleForTrack(0, sample);

    // Trigger via MIDI channel 1 (maps to track 0), note 64 (+4 semitones)
    engine.handleMidiNoteOn(1, 64, 0.75f);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, false);

    // Voice rendered sound
    REQUIRE(left[0] > 0.0f);
    REQUIRE(right[0] > 0.0f);
}
