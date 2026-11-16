#include <catch2/catch_test_macros.hpp>
#include "PatternState.h"

using namespace openfruit::core;

TEST_CASE("OpenFruit step patterns preserve sequencer performance state", "[plugin][state]") {
    FruitEngine source;
    StepData active;
    active.active = true;
    active.velocity = 0.72f;
    active.semitoneOffset = -7;
    active.probability = 0.4f;
    active.ratchets = 5;
    active.timingOffset = 0.25f;
    active.sampleStart = 0.15f;
    active.sampleEnd = 0.82f;
    active.reverse = true;
    active.chokeGroup = 2;
    source.setStepData(4, PageIndex::PageC, 11, active);

    StepData inactive = active;
    inactive.active = false;
    inactive.probability = 0.9f;
    source.setStepData(7, PageIndex::PageB, 3, inactive);
    const auto saved = openfruit::plugin::makePatternState(source);

    FruitEngine restored;
    StepData stale;
    stale.active = true;
    stale.ratchets = 3;
    restored.setStepData(0, PageIndex::PageD, 15, stale);
    openfruit::plugin::restorePatternState(saved, restored);

    REQUIRE(restored.getStep(4, PageIndex::PageC, 11) == active);
    REQUIRE(restored.getStep(7, PageIndex::PageB, 3) == inactive);
    REQUIRE(restored.getStep(0, PageIndex::PageD, 15) == StepData{});
    REQUIRE_FALSE(restored.getStep(4, PageIndex::PageA, 11).active);
}

TEST_CASE("OpenFruit empty pattern state clears existing sequencer pages", "[plugin][state]") {
    FruitEngine engine;
    StepData existing;
    existing.active = true;
    existing.reverse = true;
    engine.setStepData(2, PageIndex::PageC, 7, existing);

    const juce::ValueTree emptyState {"StepPatterns"};
    openfruit::plugin::restorePatternState(emptyState, engine);

    REQUIRE(engine.getStep(2, PageIndex::PageC, 7) == StepData{});
}
