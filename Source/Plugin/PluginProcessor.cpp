#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DefaultKit.h"
#include "WavReader.h"
#include "MidiEventProcessor.h"
#include "PatternState.h"

OpenFruitAudioProcessor::OpenFruitAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", openfruit::params::createParameterLayout()) {

    analysisQueue_ = std::make_unique<openfruit::core::AnalysisQueue>(toneCache_, 5);
    toneCache_.loadFromFile(openfruit::core::ToneCache::getDefaultCachePath());

    loadFactoryKit(false);
    // Four related phrases: straight floor, syncopated hats, halftime, and a tom fill.
    constexpr uint16_t patterns[4][8] = {
        {0x1111, 0x1010, 0x1000, 0x5555, 0x4444, 0x0080, 0x0000, 0x0000},
        {0x1511, 0x1010, 0x0010, 0x55d5, 0x4044, 0x2080, 0x0800, 0x0000},
        {0x0401, 0x0100, 0x0100, 0x5151, 0x0040, 0x2008, 0x0020, 0x8000},
        {0x1111, 0x9010, 0x1000, 0x1555, 0x0444, 0x0080, 0x5000, 0xa000}
    };
    for (int page = 0; page < 4; ++page)
        for (int track = 0; track < 8; ++track)
            for (int index = 0; index < 16; ++index) {
                if ((patterns[page][track] & (1u << index)) == 0) continue;
                openfruit::core::StepData step;
                step.active = true;
                step.velocity = track == 0 ? .94f : (index % 4 == 0 ? .80f : (index % 2 == 0 ? .72f : .49f));
                if (track == 3 || track == 4) step.chokeGroup = 1;
                if (page == 3 && track == 1 && index == 15) step.ratchets = 2;
                engine_.setStepData(track, static_cast<openfruit::core::PageIndex>(page), index, step);
            }

    syncParametersToEngine();
}

void OpenFruitAudioProcessor::loadFactoryKit(bool legacy) {
    // Factory buffers are immutable and shared between instances.
    static const auto current = openfruit::kit::createDefaultKit();
    const auto& kit = [&]() -> const auto& {
        if (!legacy) return current;
        static const auto previous = openfruit::kit::createDefaultKit(44100.0, true);
        return previous;
    }();
    for (size_t i = 0; i < kit.size(); ++i) engine_.loadSampleForTrack(static_cast<int>(i), kit[i]);
    legacyKit_ = legacy;
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

    // 2. Track Count
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kTrackCount)) {
        const int idx = static_cast<int>(*p);
        if (idx == 0) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks4);
        if (idx == 1) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks8);
        if (idx == 2) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks12);
        if (idx >= 3) engine_.setTrackCountMode(openfruit::core::TrackCountMode::Tracks16);
    }

    // 3. Master Volume
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kMasterVol)) {
        engine_.setMasterVolume(*p);
    }
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kOutputDb)) {
        engine_.setOutputDb(*p);
    }
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kSwing)) engine_.setSwing(*p);
    const float accentAmount = apvts_.getRawParameterValue(openfruit::params::kAccentAmount)->load();
    const int accentEvery = static_cast<int>(apvts_.getRawParameterValue(openfruit::params::kAccentEvery)->load());
    engine_.setAccentuator(accentAmount, accentEvery);
    if (const auto* p = apvts_.getRawParameterValue(openfruit::params::kSongMode)) engine_.setSongMode(*p > 0.5f);
    for (int section = 0; section < openfruit::core::kNumPages; ++section) {
        const auto pageId = openfruit::params::songSectionPageId(section);
        const auto barsId = openfruit::params::songSectionBarsId(section);
        const auto fillId = openfruit::params::songSectionFillId(section);
        const int page = static_cast<int>(apvts_.getRawParameterValue(pageId)->load());
        const int bars = static_cast<int>(apvts_.getRawParameterValue(barsId)->load());
        const bool fill = apvts_.getRawParameterValue(fillId)->load() > 0.5f;
        engine_.setSongSection(section, {static_cast<openfruit::core::PageIndex>(page), bars, fill});
    }

    // 4. Per-track parameters
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        auto& trk = engine_.getTrack(i);
        if (const auto* ch = apvts_.getRawParameterValue(openfruit::params::trackMidiChId(i))) {
            trk.midiChannel = std::clamp(static_cast<int>(*ch), 1, 16);
        }
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
        if (const auto* nudge = apvts_.getRawParameterValue(openfruit::params::trackNudgeId(i))) {
            engine_.setTrackNudge(i, *nudge);
        }
        if (const auto* groove = apvts_.getRawParameterValue(openfruit::params::trackGrooveId(i))) {
            engine_.setTrackGroove(i, *groove);
        }
        if (const auto* m = apvts_.getRawParameterValue(openfruit::params::trackMuteId(i))) {
            trk.mute = (*m > 0.5f);
        }
        if (const auto* s = apvts_.getRawParameterValue(openfruit::params::trackSoloId(i))) {
            trk.solo = (*s > 0.5f);
        }
        if (const auto* filter = apvts_.getRawParameterValue(openfruit::params::trackFilterId(i))) {
            trk.filterCutoffHz = *filter;
        }
        if (const auto* attack = apvts_.getRawParameterValue(openfruit::params::trackAttackId(i))) {
            trk.amplitudeAttackSeconds = *attack;
        }
        if (const auto* decay = apvts_.getRawParameterValue(openfruit::params::trackDecayId(i))) {
            trk.amplitudeDecaySeconds = *decay;
        }
        if (const auto* sustain = apvts_.getRawParameterValue(openfruit::params::trackSustainId(i))) {
            trk.amplitudeSustain = *sustain * 0.01f;
        }
        if (const auto* release = apvts_.getRawParameterValue(openfruit::params::trackReleaseId(i))) {
            trk.amplitudeReleaseSeconds = *release;
        }
    }
}

void OpenFruitAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;

    syncParametersToEngine();

    // 1. Query PlayHead
    double bpm = 120.0;
    double ppqPos = std::numeric_limits<double>::quiet_NaN();
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

    // 3. Render audio alongside MIDI events at their sample offsets.
    openfruit::plugin::processMidiBlock(engine_, midiMessages, left, right, numSamples,
                                        bpm, ppqPos, isPlaying, getSampleRate());
}

juce::AudioProcessorEditor* OpenFruitAudioProcessor::createEditor() {
    return new OpenFruitAudioProcessorEditor(*this);
}

void OpenFruitAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    state.setProperty("factoryKit", legacyKit_ ? 1 : 2, nullptr);
    juce::ValueTree files {"SampleFiles"};
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        if (const auto sample = engine_.loadedSample(i); sample && juce::File::isAbsolutePath(sample->filePath))
            files.setProperty("track" + juce::String(i), juce::String(sample->filePath), nullptr);
    }
    state.addChild(files, -1, nullptr);
    state.addChild(openfruit::plugin::makePatternState(engine_), -1, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void OpenFruitAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType())) {
        auto state = juce::ValueTree::fromXml(*xmlState);
        auto patterns = state.getChildWithName("StepPatterns");
        if (patterns.isValid()) state.removeChild(patterns, nullptr);
        loadFactoryKit(static_cast<int>(state.getProperty("factoryKit", 1)) < 2);
        auto files = state.getChildWithName("SampleFiles");
        if (files.isValid()) {
            state.removeChild(files, nullptr);
            for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
                const auto path = files.getProperty("track" + juce::String(i)).toString();
                if (!juce::File::isAbsolutePath(path)) continue;
                auto sample = openfruit::core::WavReader::loadSample(path.toStdString());
                if (!sample) {
                    sample = std::make_shared<openfruit::core::AudioSampleBuffer>();
                    sample->filePath = path.toStdString();
                    sample->sampleName = "Missing: " + juce::File(path).getFileNameWithoutExtension().toStdString();
                }
                engine_.loadSampleForTrack(i, std::move(sample));
            }
        }
        apvts_.replaceState(state);
        syncParametersToEngine();
        openfruit::plugin::restorePatternStateIfPresent(patterns, engine_);
    }
}

// Juce plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OpenFruitAudioProcessor();
}
