#include "Parameters.h"
#include <catch2/catch_test_macros.hpp>
#include <juce_audio_processors/juce_audio_processors.h>
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
