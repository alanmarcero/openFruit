#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include "FruitEngine.h"
#include "WavReader.h"

using namespace openfruit::core;

TEST_CASE("FruitEngine default 8 tracks and configurable MIDI channel routing", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    // Default track count is 8 when the app starts
    REQUIRE(engine.getActiveTrackCount() == 8);
    REQUIRE(engine.getTrackCountMode() == TrackCountMode::Tracks8);

    engine.setTrackCountMode(TrackCountMode::Tracks4);
    REQUIRE(engine.getActiveTrackCount() == 4);

    engine.setTrackCountMode(TrackCountMode::Tracks8);
    REQUIRE(engine.getActiveTrackCount() == 8);

    engine.setTrackCountMode(TrackCountMode::Tracks12);
    REQUIRE(engine.getActiveTrackCount() == 12);

    engine.setTrackCountMode(TrackCountMode::Tracks16);
    REQUIRE(engine.getActiveTrackCount() == 16);

    // Verify default MIDI channel correspondence: Track index 0..15 maps to MIDI Channel 1..16
    for (int i = 0; i < 16; ++i) {
        REQUIRE(engine.getTrack(i).midiChannel == i + 1);
        REQUIRE(engine.getTrackMidiChannel(i) == i + 1);
    }

    // Verify per-row MIDI channel is configurable
    engine.setTrackMidiChannel(0, 10);
    REQUIRE(engine.getTrackMidiChannel(0) == 10);
    REQUIRE(engine.getTrack(0).midiChannel == 10);
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

TEST_CASE("FruitEngine MIDI triggering is default and respects per-row MIDI channel", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 1000;
    sample->channels.resize(1);
    sample->channels[0].assign(1000, 0.5f);
    sample->isLoaded = true;
    engine.loadSampleForTrack(0, sample);

    // Trigger via default MIDI channel 1 (maps to track 0), note 64 (+4 semitones)
    engine.handleMidiNoteOn(1, 64, 0.75f);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, false);

    REQUIRE(left[0] > 0.0f);
    REQUIRE(right[0] > 0.0f);

    // Reconfigure track 1 to MIDI channel 5 and verify MIDI channel 5 triggers track 1
    engine.reset();
    engine.loadSampleForTrack(1, sample);
    engine.setTrackMidiChannel(1, 5);

    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    engine.handleMidiNoteOn(5, 60, 0.85f);
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, false);
    REQUIRE(left[0] > 0.0f);
}

TEST_CASE("FruitEngine play/pause button and host transport sync behavior", "[FruitEngine]") {
    FruitEngine engine;
    engine.prepare(44100.0, 512);

    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 1000;
    sample->channels.resize(1);
    sample->channels[0].assign(1000, 0.6f);
    sample->isLoaded = true;
    engine.loadSampleForTrack(0, sample);
    engine.setStepActive(0, PageIndex::PageA, 0, true);

    std::vector<float> left(256, 0.0f);
    std::vector<float> right(256, 0.0f);

    // 1. Play button is off by default: host playing does NOT auto-play pattern
    REQUIRE_FALSE(engine.isPlaying());
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, true);
    REQUIRE(left[0] == 0.0f);

    // 2. Pressing Play starts pattern playback even when host is stopped
    engine.setPlaying(true);
    REQUIRE(engine.isPlaying());
    std::fill(left.begin(), left.end(), 0.0f);
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, false);
    REQUIRE(left[0] > 0.0f);

    // 3. When host starts and then stops, plugin pauses
    engine.process(left.data(), right.data(), 256, 120.0, 0.1, true);
    REQUIRE(engine.isPlaying());
    engine.process(left.data(), right.data(), 256, 120.0, 0.2, false);
    REQUIRE_FALSE(engine.isPlaying());

    // 4. When host starts again while play was armed, it syncs and plays; hitting pause pauses it
    engine.process(left.data(), right.data(), 256, 120.0, 0.0, true);
    REQUIRE(engine.isPlaying());
    engine.setPlaying(false);
    REQUIRE_FALSE(engine.isPlaying());
}

TEST_CASE("WavReader supports all common audio file extensions", "[WavReader]") {
    REQUIRE(WavReader::isSupportedAudioFile("kick.wav"));
    REQUIRE(WavReader::isSupportedAudioFile("snare.WAV"));
    REQUIRE(WavReader::isSupportedAudioFile("loop.aiff"));
    REQUIRE(WavReader::isSupportedAudioFile("lead.aif"));
    REQUIRE(WavReader::isSupportedAudioFile("bass.mp3"));
    REQUIRE(WavReader::isSupportedAudioFile("chord.flac"));
    REQUIRE(WavReader::isSupportedAudioFile("pad.ogg"));
    REQUIRE(WavReader::isSupportedAudioFile("vocal.m4a"));
    REQUIRE(WavReader::isSupportedAudioFile("perc.aac"));
    REQUIRE(WavReader::isSupportedAudioFile("apple_loop.caf"));
    REQUIRE(WavReader::isSupportedAudioFile("synth.wma"));
    REQUIRE(WavReader::isSupportedAudioFile("fx.opus"));
    REQUIRE_FALSE(WavReader::isSupportedAudioFile("readme.txt"));

    // Write and decode FLAC, OGG, AIFF, and WAV files via WavReader
    const juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("openfruit_format_tests");
    tempDir.createDirectory();

    constexpr int sampleRate = 44100;
    constexpr int numSamples = 4096;
    juce::AudioBuffer<float> srcBuf(1, numSamples);
    for (int i = 0; i < numSamples; ++i) {
        srcBuf.setSample(0, i, 0.5f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / static_cast<float>(sampleRate)));
    }

    auto writeWithFormat = [&](juce::AudioFormat& fmt, const juce::String& fileName) {
        const juce::File f = tempDir.getChildFile(fileName);
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> outStream(f.createOutputStream());
        REQUIRE(outStream != nullptr);
        const auto options = juce::AudioFormatWriterOptions{}
            .withSampleRate(sampleRate)
            .withNumChannels(1)
            .withBitsPerSample(16);
        std::unique_ptr<juce::AudioFormatWriter> writer(
            fmt.createWriterFor(outStream, options)
        );
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(srcBuf, 0, numSamples));
        writer.reset();
        return f.getFullPathName().toStdString();
    };

    juce::FlacAudioFormat flacFmt;
    juce::OggVorbisAudioFormat oggFmt;
    juce::AiffAudioFormat aiffFmt;
    juce::WavAudioFormat wavFmt;

    const std::string flacPath = writeWithFormat(flacFmt, "test_sample.flac");
    const std::string oggPath = writeWithFormat(oggFmt, "test_sample.ogg");
    const std::string aiffPath = writeWithFormat(aiffFmt, "test_sample.aiff");
    const std::string wavPath = writeWithFormat(wavFmt, "test_sample.wav");

    for (const auto& path : { flacPath, oggPath, aiffPath, wavPath }) {
        const auto decoded = WavReader::readWavFile(path);
        REQUIRE(decoded.isValid);
        REQUIRE(decoded.numChannels == 1);
        REQUIRE(decoded.sampleRate == sampleRate);
        REQUIRE(decoded.numSamples > 1000);
        REQUIRE_FALSE(decoded.channels.empty());

        const auto analysisDecoded = WavReader::readWavFileForAnalysis(path, 2048);
        REQUIRE(analysisDecoded.isValid);
        REQUIRE_FALSE(analysisDecoded.monoChannel.empty());
    }

    tempDir.deleteRecursively();
}
