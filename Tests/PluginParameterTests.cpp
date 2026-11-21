#include "Parameters.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

namespace {

class ParameterTestProcessor final : public juce::AudioProcessor {
public:
    ParameterTestProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "ParameterTest"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
};

} // namespace

TEST_CASE("OpenFruit output dB parameter defaults to unity and is automatable", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    auto* output = state.getParameter("output_db");
    REQUIRE(output != nullptr);
    CHECK(output->isAutomatable());
    CHECK(std::abs(state.getRawParameterValue("output_db")->load()) < 0.001f);
}

TEST_CASE("OpenFruit output dB session state keeps legacy master volume", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    auto* output = state.getParameter("output_db");
    REQUIRE(output != nullptr);
    REQUIRE(state.getParameter("master_vol") != nullptr);

    output->setValueNotifyingHost(output->convertTo0to1(-6.0f));
    const auto saved = state.copyState();
    ParameterTestProcessor restoredProcessor;
    juce::AudioProcessorValueTreeState restored(
        restoredProcessor, nullptr, "Parameters", openfruit::params::createParameterLayout());
    restored.replaceState(saved);

    CHECK(std::abs(restored.getRawParameterValue("output_db")->load() + 6.0f) < 0.01f);
    CHECK(restored.getRawParameterValue("master_vol")->load() == 1.0f);
}

TEST_CASE("OpenFruit exposes per-track filter and envelope controls", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    REQUIRE(state.getParameter(openfruit::params::trackFilterId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::trackAttackId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::trackDecayId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::trackSustainId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::trackReleaseId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::trackFilterId(15)) != nullptr);
}

TEST_CASE("OpenFruit exposes song mode sections and swing automation", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    REQUIRE(state.getParameter(openfruit::params::kSongMode) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::kSwing) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::songSectionPageId(0)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::songSectionBarsId(3)) != nullptr);
    REQUIRE(state.getParameter(openfruit::params::songSectionFillId(2)) != nullptr);
}

TEST_CASE("OpenFruit exposes automatable track groove and accent controls", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    const auto nudgeId = openfruit::params::trackNudgeId(7);
    const auto grooveId = openfruit::params::trackGrooveId(7);
    const auto nudge = state.getParameter(nudgeId);
    const auto groove = state.getParameter(grooveId);
    const auto amount = state.getParameter(openfruit::params::kAccentAmount);
    const auto interval = state.getParameter(openfruit::params::kAccentEvery);
    REQUIRE(nudge != nullptr);
    REQUIRE(groove != nullptr);
    REQUIRE(amount != nullptr);
    REQUIRE(interval != nullptr);
    CHECK(nudge->isAutomatable());
    CHECK(groove->isAutomatable());
    CHECK(amount->isAutomatable());
    CHECK(interval->isAutomatable());

    nudge->setValueNotifyingHost(nudge->convertTo0to1(0.2f));
    groove->setValueNotifyingHost(groove->convertTo0to1(0.35f));
    amount->setValueNotifyingHost(amount->convertTo0to1(0.6f));
    interval->setValueNotifyingHost(interval->convertTo0to1(3.0f));
    const auto saved = state.copyState();
    ParameterTestProcessor restoredProcessor;
    juce::AudioProcessorValueTreeState restored(
        restoredProcessor, nullptr, "Parameters", openfruit::params::createParameterLayout());
    restored.replaceState(saved);

    CHECK(std::abs(restored.getRawParameterValue(nudgeId)->load() - 0.2f) < 0.01f);
    CHECK(std::abs(restored.getRawParameterValue(grooveId)->load() - 0.35f) < 0.01f);
    CHECK(std::abs(restored.getRawParameterValue(openfruit::params::kAccentAmount)->load() - 0.6f) < 0.01f);
    CHECK(restored.getRawParameterValue(openfruit::params::kAccentEvery)->load() == 3.0f);
}

TEST_CASE("OpenFruit exposes per-track high-pass, drive, and global humanize controls", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    auto* hpf = state.getParameter(openfruit::params::trackHpfId(0));
    auto* drive = state.getParameter(openfruit::params::trackDriveId(0));
    auto* humanize = state.getParameter(openfruit::params::kHumanize);
    REQUIRE(hpf != nullptr);
    REQUIRE(drive != nullptr);
    REQUIRE(humanize != nullptr);
    CHECK(hpf->isAutomatable());
    CHECK(drive->isAutomatable());
    CHECK(humanize->isAutomatable());
    CHECK(std::abs(state.getRawParameterValue(openfruit::params::trackHpfId(0))->load() - 20.0f) < 0.01f);
    CHECK(std::abs(state.getRawParameterValue(openfruit::params::trackDriveId(0))->load()) < 0.01f);
    CHECK(std::abs(state.getRawParameterValue(openfruit::params::kHumanize)->load()) < 0.01f);
}

TEST_CASE("Every float parameter parses its own display text", "[plugin][parameters]") {
    const float normalised = GENERATE(0.0f, 0.001f, 0.013f, 0.1f, 0.25f, 0.5f, 0.618f, 0.75f, 0.9f, 0.999f, 1.0f);
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());

    juce::StringArray unstable;
    const auto& parameters = processor.getParameters();
    std::for_each(parameters.begin(), parameters.end(), [&](juce::AudioProcessorParameter* parameter) {
        if (dynamic_cast<juce::AudioParameterFloat*>(parameter) == nullptr)
            return;
        const auto text = parameter->getText(normalised, 0);
        const auto reread = parameter->getText(parameter->getValueForText(text), 0);
        if (reread != text)
            unstable.add(parameter->getName(64) + ": " + text + " -> " + reread);
    });

    CHECK(unstable.joinIntoString(", ").toStdString() == "");
}

TEST_CASE("Typed values accept the units the display shows", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());
    const auto typed = [&state](const std::string& id, const juce::String& text) {
        auto* parameter = state.getParameter(id);
        return parameter->convertFrom0to1(parameter->getValueForText(text));
    };

    CHECK(typed(openfruit::params::kSwing, "25") == Catch::Approx(0.25f));
    CHECK(typed(openfruit::params::kSwing, "25%") == Catch::Approx(0.25f));
    CHECK(typed(openfruit::params::trackFilterId(0), "6.8k") == Catch::Approx(6800.0f));
    CHECK(typed(openfruit::params::trackFilterId(0), "6.8 kHz") == Catch::Approx(6800.0f));
    CHECK(typed(openfruit::params::trackFilterId(0), "850 Hz") == Catch::Approx(850.0f));
    CHECK(typed(openfruit::params::trackAttackId(0), "160") == Catch::Approx(0.16f));
    CHECK(typed(openfruit::params::trackAttackId(0), "160 ms") == Catch::Approx(0.16f));
    CHECK(typed(openfruit::params::trackAttackId(0), "1.2 s") == Catch::Approx(1.2f));
    CHECK(typed(openfruit::params::kOutputDb, "-6 dB") == Catch::Approx(-6.0f));
    CHECK(typed(openfruit::params::kOutputDb, "+3") == Catch::Approx(3.0f));
    CHECK(typed(openfruit::params::kMasterVol, "-inf dB") == 0.0f);
    CHECK(typed(openfruit::params::kMasterVol, "0 dB") == Catch::Approx(1.0f));
    CHECK(typed(openfruit::params::trackPanId(0), "C") == Catch::Approx(0.0f).margin(1.0e-6));
    CHECK(typed(openfruit::params::trackPanId(0), "40 L") == Catch::Approx(-0.4f));
    CHECK(typed(openfruit::params::trackPanId(0), "40 R") == Catch::Approx(0.4f));
    CHECK(typed(openfruit::params::trackNudgeId(0), "-20%") == Catch::Approx(-0.2f));
}

TEST_CASE("Parameter text uses the suite display formats", "[plugin][parameters]") {
    ParameterTestProcessor processor;
    juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", openfruit::params::createParameterLayout());
    const auto shown = [&state](const std::string& id, float value) {
        auto* parameter = state.getParameter(id);
        return parameter->getText(parameter->convertTo0to1(value), 0).toStdString();
    };

    CHECK(shown(openfruit::params::kMasterVol, 1.0f) == "0.0 dB");
    CHECK(shown(openfruit::params::kMasterVol, 1.5f) == "+3.5 dB");
    CHECK(shown(openfruit::params::kMasterVol, 0.0f) == "-inf dB");
    CHECK(shown(openfruit::params::kOutputDb, 3.0f) == "+3.0 dB");
    CHECK(shown(openfruit::params::kOutputDb, -0.04f) == "0.0 dB");
    CHECK(shown(openfruit::params::trackFilterId(0), 850.0f) == "850 Hz");
    CHECK(shown(openfruit::params::trackFilterId(0), 6800.0f) == "6.80 kHz");
    CHECK(shown(openfruit::params::trackAttackId(0), 0.004f) == "4.0 ms");
    CHECK(shown(openfruit::params::trackAttackId(0), 0.16f) == "160 ms");
    CHECK(shown(openfruit::params::trackAttackId(0), 1.2f) == "1.20 s");
    CHECK(shown(openfruit::params::trackPanId(0), 0.0f) == "C");
    CHECK(shown(openfruit::params::trackPanId(0), -0.4f) == "40 L");
    CHECK(shown(openfruit::params::kSwing, 0.25f) == "25%");
    CHECK(shown(openfruit::params::trackNudgeId(0), 0.2f) == "+20%");
    CHECK(shown(openfruit::params::kHumanize, 55.0f) == "55%");
    CHECK(shown(openfruit::params::songSectionBarsId(0), 1.0f) == "1 bar");
    CHECK(shown(openfruit::params::songSectionBarsId(0), 4.0f) == "4 bars");
    CHECK(shown(openfruit::params::trackTuneId(0), 3.0f) == "+3 st");
}
