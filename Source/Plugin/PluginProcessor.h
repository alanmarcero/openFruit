#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FruitEngine.h"
#include "ToneCache.h"
#include "AnalysisQueue.h"
#include "Parameters.h"
#include "suite/AbCompare.h"
#include "suite/PresetLibrary.h"
#include "suite/UndoHistory.h"

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
    openfruit::suite::UndoHistory& getUndo() noexcept { return undo_; }
    openfruit::suite::AbCompare& getAb() noexcept { return ab_; }
    openfruit::suite::PresetLibrary& getPresets() noexcept { return presets_; }

    void selectAbSlot(int index);
    void copyAbSlot(int from, int to);
    void loadPresetBlock(const juce::MemoryBlock& block);
    [[nodiscard]] juce::MemoryBlock captureInnerState();

private:
    openfruit::core::FruitEngine engine_;
    openfruit::core::ToneCache toneCache_;
    std::unique_ptr<openfruit::core::AnalysisQueue> analysisQueue_;
    juce::AudioProcessorValueTreeState apvts_;
    openfruit::suite::UndoHistory undo_;
    openfruit::suite::AbCompare ab_;
    openfruit::suite::PresetLibrary presets_ { "openFruit" };

    bool legacyKit_ = false;
    int stateRestoreDepth_ = 0;
    bool loadingSlot_ = false;
    void loadFactoryKit(bool legacy);
    void syncParametersToEngine();
    void buildFactoryPresets();
    [[nodiscard]] juce::ValueTree makeStateTree();
    [[nodiscard]] juce::MemoryBlock treeToBlock(const juce::ValueTree& tree);
    void restoreBlock(const juce::MemoryBlock& block);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenFruitAudioProcessor)
};
