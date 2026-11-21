#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "PluginProcessor.h"
#include "PatternState.h"
#include "SampleState.h"
#include "StepEditAction.h"
#include "WavReader.h"
#include "suite/PresetLibrary.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace openfruit;

namespace {

juce::File writeTone(const juce::File& file, float amplitude) {
    file.deleteFile();
    juce::AudioBuffer<float> buffer(1, 2048);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample(0, i, amplitude * std::sin(0.07f * static_cast<float>(i)));
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    juce::WavAudioFormat wav;
    juce::AudioFormat& format = wav;
    const auto options = juce::AudioFormatWriterOptions{}
        .withSampleRate(44100.0)
        .withNumChannels(1)
        .withBitsPerSample(24);
    auto writer = format.createWriterFor(stream, options);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()));
    writer.reset();
    return file;
}

float sampleAt(const core::AudioSampleBuffer& buffer, int index) {
    return buffer.channels.at(0).at(static_cast<size_t>(index));
}

} // namespace

TEST_CASE("Embedded samples round-trip without the original file", "[plugin][state]") {
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("openfruit-embedded-session");
    directory.createDirectory();
    const auto file = writeTone(directory.getChildFile("rim.wav"), 0.55f);

    OpenFruitAudioProcessor source;
    auto loaded = core::WavReader::loadSample(file.getFullPathName().toStdString());
    REQUIRE(loaded != nullptr);
    const float expected = sampleAt(*loaded, 40);
    source.getEngine().loadSampleForTrack(3, loaded);
    source.getEngine().loadSampleForTrack(5, loaded);

    juce::MemoryBlock session;
    source.getStateInformation(session);
    auto xml = juce::AudioProcessor::getXmlFromBinary(session.getData(), static_cast<int>(session.getSize()));
    REQUIRE(xml != nullptr);
    auto* files = xml->getChildByName("SampleFiles");
    REQUIRE(files != nullptr);
    int blobs = 0;
    for (auto* child = files->getFirstChildElement(); child != nullptr; child = child->getNextElement())
        if (child->hasTagName("Blob"))
            ++blobs;
    REQUIRE(blobs == 1);
    REQUIRE(files->getStringAttribute("track3") == file.getFullPathName());

    file.deleteFile();
    OpenFruitAudioProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    const auto sample = restored.getEngine().loadedSample(3);
    REQUIRE(sample != nullptr);
    REQUIRE(sample->isLoaded);
    REQUIRE(sample->numSamples == loaded->numSamples);
    REQUIRE_THAT(sampleAt(*sample, 40), Catch::Matchers::WithinAbs(expected, 1.0e-4));
    REQUIRE(sample->filePath == file.getFullPathName().toStdString());
    REQUIRE(restored.getEngine().loadedSample(5)->numSamples == loaded->numSamples);
    directory.deleteRecursively();
}

TEST_CASE("Legacy path sessions load and are embedded on the next save", "[plugin][state]") {
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("openfruit-legacy-session");
    directory.createDirectory();
    const auto file = writeTone(directory.getChildFile("hat.wav"), 0.4f);
    auto original = core::WavReader::loadSample(file.getFullPathName().toStdString());
    REQUIRE(original != nullptr);

    OpenFruitAudioProcessor donor;
    auto state = donor.getAPVTS().copyState();
    state.setProperty("factoryKit", 2, nullptr);
    juce::ValueTree files { "SampleFiles" };
    files.setProperty("track0", file.getFullPathName(), nullptr);
    state.addChild(files, -1, nullptr);
    state.addChild(plugin::makePatternState(donor.getEngine()), -1, nullptr);
    juce::MemoryBlock legacy;
    donor.copyXmlToBinary(*state.createXml(), legacy);

    OpenFruitAudioProcessor fromPath;
    fromPath.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
    auto loaded = fromPath.getEngine().loadedSample(0);
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->isLoaded);
    REQUIRE_THAT(sampleAt(*loaded, 10), Catch::Matchers::WithinAbs(sampleAt(*original, 10), 1.0e-4));

    file.deleteFile();
    juce::MemoryBlock embedded;
    fromPath.getStateInformation(embedded);
    OpenFruitAudioProcessor fromSession;
    fromSession.setStateInformation(embedded.getData(), static_cast<int>(embedded.getSize()));
    auto kept = fromSession.getEngine().loadedSample(0);
    REQUIRE(kept != nullptr);
    REQUIRE(kept->isLoaded);
    REQUIRE(kept->numSamples == original->numSamples);
    REQUIRE_THAT(sampleAt(*kept, 10), Catch::Matchers::WithinAbs(sampleAt(*original, 10), 1.0e-4));
    REQUIRE(kept->filePath == file.getFullPathName().toStdString());
    directory.deleteRecursively();
}

TEST_CASE("Parameter gestures and step edits undo as one step", "[plugin][undo]") {
    OpenFruitAudioProcessor processor;
    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    const float start = swing->getValue();
    swing->beginChangeGesture();
    swing->setValueNotifyingHost(swing->convertTo0to1(0.2f));
    swing->setValueNotifyingHost(swing->convertTo0to1(0.45f));
    swing->endChangeGesture();
    REQUIRE(processor.getUndo().canUndo());
    processor.getUndo().undo();
    REQUIRE_THAT(swing->getValue(), Catch::Matchers::WithinAbs(start, 1.0e-5));
    processor.getUndo().redo();
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.45f, 1.0e-3));

    auto& engine = processor.getEngine();
    const auto before = engine.getStep(1, core::PageIndex::PageA, 4);
    core::StepData after = before;
    after.active = true;
    after.velocity = 0.33f;
    after.ratchets = 3;
    engine.setStepData(1, core::PageIndex::PageA, 4, after);
    processor.getUndo().beginNewTransaction();
    processor.getUndo().perform(new plugin::StepEditAction(engine, { { 1, core::PageIndex::PageA, 4, before, after } }));
    processor.getUndo().undo();
    REQUIRE(engine.getStep(1, core::PageIndex::PageA, 4) == before);
    processor.getUndo().redo();
    REQUIRE(engine.getStep(1, core::PageIndex::PageA, 4) == after);
}

TEST_CASE("A/B slots keep two full sessions", "[plugin][ab]") {
    OpenFruitAudioProcessor processor;
    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    swing->beginChangeGesture();
    swing->setValueNotifyingHost(swing->convertTo0to1(0.3f));
    swing->endChangeGesture();

    processor.selectAbSlot(1);
    REQUIRE(processor.getAb().active() == 1);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.0f, 1.0e-3));
    swing->beginChangeGesture();
    swing->setValueNotifyingHost(swing->convertTo0to1(0.1f));
    swing->endChangeGesture();

    processor.selectAbSlot(0);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.3f, 1.0e-3));
    processor.selectAbSlot(1);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.1f, 1.0e-3));

    processor.copyAbSlot(0, 1);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.3f, 1.0e-3));
    processor.selectAbSlot(0);
    processor.copyAbSlot(1, 0);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.3f, 1.0e-3));
}

TEST_CASE("Each A/B slot keeps its own undo history", "[plugin][ab][undo]") {
    OpenFruitAudioProcessor processor;
    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    const auto gesture = [swing](float value) {
        swing->beginChangeGesture();
        swing->setValueNotifyingHost(swing->convertTo0to1(value));
        swing->endChangeGesture();
    };
    gesture(0.3f);

    processor.selectAbSlot(1);
    REQUIRE_FALSE(processor.getUndo().canUndo());
    gesture(0.1f);
    processor.selectAbSlot(0);
    processor.getUndo().undo();
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.0f, 1.0e-3));

    processor.selectAbSlot(1);
    REQUIRE(processor.getUndo().canUndo());
    processor.copyAbSlot(0, 1);
    REQUIRE_FALSE(processor.getUndo().canUndo());
}

TEST_CASE("A kit load is one undo step that keeps earlier edits", "[plugin][presets][undo]") {
    OpenFruitAudioProcessor processor;
    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    swing->beginChangeGesture();
    swing->setValueNotifyingHost(swing->convertTo0to1(0.3f));
    swing->endChangeGesture();
    const auto kits = processor.getPresets().list();
    const auto blank = std::find_if(kits.begin(), kits.end(), [](const suite::PresetEntry& kit) { return kit.name == "Blank"; });
    REQUIRE(blank != kits.end());
    juce::MemoryBlock block;
    juce::String error;
    REQUIRE(processor.getPresets().load(*blank, block, error));

    processor.loadPresetBlock(block, blank->name);
    REQUIRE_FALSE(processor.getEngine().getStep(0, core::PageIndex::PageA, 0).active);
    REQUIRE(processor.getPresetName() == "Blank");
    REQUIRE_FALSE(processor.isPresetEdited());

    processor.getUndo().undo();
    REQUIRE(processor.getEngine().getStep(0, core::PageIndex::PageA, 0).active);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.3f, 1.0e-3));
    REQUIRE(processor.getPresetName() == "Citrus");
    REQUIRE(processor.isPresetEdited());
    processor.getUndo().undo();
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.0f, 1.0e-3));
}

TEST_CASE("The kit name and edit mark travel with each slot and the session", "[plugin][presets][ab]") {
    OpenFruitAudioProcessor processor;
    REQUIRE(processor.getPresetName() == "Citrus");
    REQUIRE_FALSE(processor.isPresetEdited());
    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    swing->beginChangeGesture();
    swing->setValueNotifyingHost(swing->convertTo0to1(0.2f));
    swing->endChangeGesture();
    REQUIRE(processor.isPresetEdited());
    processor.presetSaved("Room Kit");
    REQUIRE(processor.getPresetName() == "Room Kit");
    REQUIRE_FALSE(processor.isPresetEdited());
    processor.markEdited();

    processor.selectAbSlot(1);
    REQUIRE(processor.getPresetName() == "Citrus");
    REQUIRE_FALSE(processor.isPresetEdited());
    processor.selectAbSlot(0);
    REQUIRE(processor.getPresetName() == "Room Kit");
    REQUIRE(processor.isPresetEdited());

    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenFruitAudioProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    REQUIRE(restored.getPresetName() == "Room Kit");
    REQUIRE(restored.isPresetEdited());
}

TEST_CASE("An armed sequencer is still armed when the session reopens", "[plugin][transport]") {
    OpenFruitAudioProcessor processor;
    auto& engine = processor.getEngine();
    engine.prepare(48000.0, 480);
    std::vector<float> left(480), right(480);
    engine.setPlaying(true);
    engine.process(left.data(), right.data(), 480, 120.0, 0.0, true);
    engine.process(left.data(), right.data(), 480, 120.0, 0.1, false);
    REQUIRE_FALSE(engine.isPlaying());
    REQUIRE(engine.isHostSyncArmed());

    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenFruitAudioProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    auto& reopened = restored.getEngine();
    reopened.prepare(48000.0, 480);
    REQUIRE(reopened.isHostSyncArmed());
    reopened.process(left.data(), right.data(), 480, 120.0, 0.0, true);
    REQUIRE(reopened.isPlaying());
}

TEST_CASE("User presets save and factory presets stay read-only", "[plugin][presets]") {
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("openfruit-presets");
    directory.deleteRecursively();
    directory.createDirectory();

    OpenFruitAudioProcessor processor;
    processor.getPresets().setDirectory(directory);
    const auto factory = processor.getPresets().list();
    REQUIRE(factory.size() >= 4);
    REQUIRE(factory.front().factory);

    auto* swing = processor.getAPVTS().getParameter(params::kSwing);
    swing->setValueNotifyingHost(swing->convertTo0to1(0.22f));
    juce::String error;
    REQUIRE(processor.getPresets().save("Room Kit", processor.captureInnerState(), error));
    REQUIRE_FALSE(processor.getPresets().save("Citrus", processor.captureInnerState(), error));

    suite::PresetEntry saved;
    bool found = false;
    for (const auto& entry : processor.getPresets().list("room")) {
        if (!entry.factory && entry.name == "Room Kit") {
            saved = entry;
            found = true;
        }
    }
    REQUIRE(found);
    REQUIRE(processor.getPresets().setFavorite(saved, true));
    REQUIRE(processor.getPresets().list().front().favorite);

    juce::MemoryBlock block;
    REQUIRE(processor.getPresets().load(saved, block, error));
    swing->setValueNotifyingHost(swing->convertTo0to1(0.0f));
    processor.loadPresetBlock(block, saved.name);
    REQUIRE_THAT(swing->convertFrom0to1(swing->getValue()), Catch::Matchers::WithinAbs(0.22f, 1.0e-3));

    REQUIRE(processor.getPresets().rename(saved, "Closer Kit", error));
    REQUIRE_FALSE(processor.getPresets().remove(factory.front(), error));
    suite::PresetEntry renamed;
    renamed.name = "Closer Kit";
    renamed.file = directory.getChildFile("Closer Kit.preset");
    REQUIRE(processor.getPresets().remove(renamed, error));
    REQUIRE(processor.getPresets().list("Closer").empty());
    directory.deleteRecursively();
}
