#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FruitEngine.h"
#include "ToneCache.h"
#include "AnalysisQueue.h"
#include "Parameters.h"

class OpenFruitAudioProcessor : public juce::AudioProcessor {
public:
    OpenFruitAudioProcessor();
    ~OpenFruitAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "openFruit"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    openfruit::core::FruitEngine& getEngine() noexcept { return engine_; }
    openfruit::core::ToneCache& getToneCache() noexcept { return toneCache_; }
    openfruit::core::AnalysisQueue& getAnalysisQueue() noexcept { return *analysisQueue_; }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }

private:
    openfruit::core::FruitEngine engine_;
    openfruit::core::ToneCache toneCache_;
    std::unique_ptr<openfruit::core::AnalysisQueue> analysisQueue_;
    juce::AudioProcessorValueTreeState apvts_;

    void syncParametersToEngine();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFruitAudioProcessor)
};
