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
    openfruit::plugin::restorePatternState(saved, restored);

    REQUIRE(restored.getStep(4, PageIndex::PageC, 11) == active);
    REQUIRE(restored.getStep(7, PageIndex::PageB, 3) == inactive);
    REQUIRE_FALSE(restored.getStep(4, PageIndex::PageA, 11).active);
}
