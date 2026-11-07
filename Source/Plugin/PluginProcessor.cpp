#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DefaultKit.h"

OpenFruitAudioProcessor::OpenFruitAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", openfruit::params::createParameterLayout()) {

    analysisQueue_ = std::make_unique<openfruit::core::AnalysisQueue>(toneCache_, 5);
    toneCache_.loadFromFile(openfruit::core::ToneCache::getDefaultCachePath());

    // Load factory default drum kit
    const auto kit = openfruit::kit::createDefaultKit(44100.0);
    for (size_t i = 0; i < kit.size() && i < static_cast<size_t>(openfruit::core::kMaxTracks); ++i) {
        engine_.loadSampleForTrack(static_cast<int>(i), kit[i]);
    }

    // Set a classic default 4-on-the-floor groove on Track 0 (Kick) and Track 1 (Snare)
    engine_.setStepActive(0, openfruit::core::PageIndex::PageA, 0, true);
    engine_.setStepActive(0, openfruit::core::PageIndex::PageA, 4, true);
    engine_.setStepActive(0, openfruit::core::PageIndex::PageA, 8, true);
    engine_.setStepActive(0, openfruit::core::PageIndex::PageA, 12, true);

    engine_.setStepActive(1, openfruit::core::PageIndex::PageA, 4, true);
    engine_.setStepActive(1, openfruit::core::PageIndex::PageA, 12, true);

    // Hi-hats on 8ths
    engine_.setStepActive(3, openfruit::core::PageIndex::PageA, 2, true);
    engine_.setStepActive(3, openfruit::core::PageIndex::PageA, 6, true);
    engine_.setStepActive(3, openfruit::core::PageIndex::PageA, 10, true);
    engine_.setStepActive(3, openfruit::core::PageIndex::PageA, 14, true);
}

OpenFruitAudioProcessor::~OpenFruitAudioProcessor() {
    toneCache_.saveToFile(openfruit::core::ToneCache::getDefaultCachePath());
    if (analysisQueue_) {
        analysisQueue_->stop();
    }
}

void OpenFruitAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    engine_.prepare(sampleRate, samplesPerBlock);
}

void OpenFruitAudioProcessor::releaseResources() {
    engine_.reset();
}

bool OpenFruitAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }
    return true;
}

void OpenFruitAudioProcessor::syncParametersToEngine() {
    // 1. Page
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kPage)) {
        engine_.setActivePage(static_cast<openfruit::core::PageIndex>(static_cast<int>(*p)));
    }

    // 2. Control Mode
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kInputMode)) {
        engine_.setInputControlMode(static_cast<openfruit::core::InputControlMode>(static_cast<int>(*p)));
    }

    // 3. Track Count
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kTrackCount)) {
        const int idx = static_cast<int>(*p);
        if (idx == 0) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks4);
        else if (idx == 1) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks8);
        else if (idx == 2) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks12);
        else engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks16);
    }

    // 4. Master Volume
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kMasterVol)) {
        engine_.setMasterVolume(*p);
    }

    // 5. Per-track parameters
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        auto& trk = engine_.getTrack(i);
        if (const auto* v = apvts_.getRawParameterValue(openfruit::params::trackVolId(i))) {
            trk.volume = *v;
        }
        if (const auto* pan = apvts_.getRawParameterValue(openfruit::params::trackPanId(i))) {
            trk.pan = *pan;
        }
        if (const auto* tune = apvts_.getRawParameterValue(openfruit::params::trackTuneId(i))) {
            trk.baseTune = static_cast<int>(*tune);
        }
        if (const auto* tm = apvts_.getRawParameterValue(openfruit::params::trackTimingId(i))) {
            trk.timing = static_cast<openfruit::core::StepTiming>(static_cast<int>(*tm));
        }
        if (const auto* m = apvts_.getRawParameterValue(openfruit::params::trackMuteId(i))) {
            trk.mute = (*m > 0.5f);
        }
        if (const auto* s = apvts_.getRawParameterValue(openfruit::params::trackSoloId(i))) {
            trk.solo = (*s > 0.5f);
        }
    }
}

void OpenFruitAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    syncParametersToEngine();

    // 1. Process MIDI Events
    for (const auto metadata : midiMessages) {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            engine_.handleMidiNoteOn(msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity());
        } else if (msg.isNoteOff()) {
            engine_.handleMidiNoteOff(msg.getChannel(), msg.getNoteNumber());
        }
    }

    // 2. Query PlayHead
    double bpm = 120.0;
    double ppqPos = 0.0;
    bool isPlaying = false;

    if (auto* playHead = getPlayHead()) {
        if (auto posOpt = playHead->getPosition()) {
            if (posOpt->getBpm().hasValue()) {
                bpm = *posOpt->getBpm();
            }
            if (posOpt->getPpqPosition().hasValue()) {
                ppqPos = *posOpt->getPpqPosition();
            }
            isPlaying = posOpt->getIsPlaying();
        }
    }

    // Clear output buffer
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    float* left = buffer.getWritePointer(0);
    float* right = (buffer.getNumChannels() > 1) ? buffer.getWritePointer(1) : left;

    // 3. Render Engine
    engine_.process(left, right, numSamples, bpm, ppqPos, isPlaying);
}

juce::AudioProcessorEditor* OpenFruitAudioProcessor::createEditor() {
    return new OpenFruitAudioProcessorEditor(*this);
}

void OpenFruitAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OpenFruitAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType())) {
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        syncParametersToEngine();
    }
}

// Juce plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenFruitAudioProcessor();
}
