#include "PatternState.h"

namespace openfruit::plugin {

juce::ValueTree makePatternState(const core::FruitEngine& engine) {
    juce::ValueTree patterns {"StepPatterns"};
    for (int track = 0; track < core::kMaxTracks; ++track) {
        for (int page = 0; page < core::kNumPages; ++page) {
            for (int stepIndex = 0; stepIndex < core::kStepsPerPage; ++stepIndex) {
                const auto step = engine.getStep(track, static_cast<core::PageIndex>(page), stepIndex);
                if (step == core::StepData {}) continue;
                juce::ValueTree child {"Step"};
                child.setProperty("track", track, nullptr);
                child.setProperty("page", page, nullptr);
                child.setProperty("index", stepIndex, nullptr);
                child.setProperty("active", step.active, nullptr);
                child.setProperty("velocity", step.velocity, nullptr);
                child.setProperty("tune", step.semitoneOffset, nullptr);
                child.setProperty("probability", step.probability, nullptr);
                child.setProperty("ratchets", step.ratchets, nullptr);
                child.setProperty("timingOffset", step.timingOffset, nullptr);
                child.setProperty("sampleStart", step.sampleStart, nullptr);
                child.setProperty("sampleEnd", step.sampleEnd, nullptr);
                child.setProperty("reverse", step.reverse, nullptr);
                child.setProperty("chokeGroup", step.chokeGroup, nullptr);
                patterns.addChild(child, -1, nullptr);
            }
        }
    }
    return patterns;
}

void restorePatternState(const juce::ValueTree& patterns, core::FruitEngine& engine) {
    if (!patterns.hasType("StepPatterns")) return;
    engine.clearStepPatterns();
    for (int i = 0; i < patterns.getNumChildren(); ++i) {
        const auto child = patterns.getChild(i);
        if (!child.hasType("Step")) continue;
        core::StepData step;
        step.active = child.getProperty("active", false);
        step.velocity = child.getProperty("velocity", 0.85f);
        step.semitoneOffset = child.getProperty("tune", 0);
        step.probability = child.getProperty("probability", 1.0f);
        step.ratchets = child.getProperty("ratchets", 1);
        step.timingOffset = child.getProperty("timingOffset", 0.0f);
        step.sampleStart = child.getProperty("sampleStart", 0.0f);
        step.sampleEnd = child.getProperty("sampleEnd", 1.0f);
        step.reverse = child.getProperty("reverse", false);
        step.chokeGroup = child.getProperty("chokeGroup", 0);
        engine.setStepData(child.getProperty("track", -1),
            static_cast<core::PageIndex>(static_cast<int>(child.getProperty("page", 0))),
            child.getProperty("index", -1), step);
    }
}

void restorePatternStateIfPresent(const juce::ValueTree& patterns, core::FruitEngine& engine) {
    if (!patterns.isValid()) {
        engine.clearStepPatterns();
        return;
    }
    restorePatternState(patterns, engine);
}

}
