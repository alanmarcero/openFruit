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

TEST_CASE("OpenFruit legacy state without pattern data clears reused pages", "[plugin][state]") {
    FruitEngine engine;
    StepData oldPattern;
    oldPattern.active = true;
    engine.setStepData(0, PageIndex::PageA, 0, oldPattern);

    const juce::ValueTree legacyState {"Parameters"};
    const auto patterns = legacyState.getChildWithName("StepPatterns");
    openfruit::plugin::restorePatternStateIfPresent(patterns, engine);

    REQUIRE(engine.getStep(0, PageIndex::PageA, 0) == StepData{});
}

TEST_CASE("OpenFruit invalid pattern data leaves existing pages intact", "[plugin][state]") {
    FruitEngine engine;
    StepData existing;
    existing.active = true;
    engine.setStepData(0, PageIndex::PageB, 4, existing);

    openfruit::plugin::restorePatternState({}, engine);

    REQUIRE(engine.getStep(0, PageIndex::PageB, 4) == existing);
}

TEST_CASE("Row commands shift, clear and paste as undoable change sets", "[plugin][state]") {
    using namespace openfruit::plugin;
    FruitEngine engine;
    StepData hit;
    hit.active = true;
    engine.setStepData(2, PageIndex::PageB, 15, hit);
    engine.setStepData(3, PageIndex::PageB, 0, hit);

    StepEditAction shift(engine, rowsCommandChanges(engine, PageIndex::PageB, 2, 1, RowsCommand::ShiftRight, {}));
    shift.perform();
    REQUIRE(engine.getStep(2, PageIndex::PageB, 0).active);
    REQUIRE_FALSE(engine.getStep(2, PageIndex::PageB, 15).active);

    const auto copied = copyRows(engine, PageIndex::PageB, 0, kMaxTracks);
    StepEditAction clear(engine, rowsCommandChanges(engine, PageIndex::PageB, 0, kMaxTracks, RowsCommand::Clear, {}));
    clear.perform();
    REQUIRE_FALSE(engine.getStep(3, PageIndex::PageB, 0).active);
    StepEditAction paste(engine, rowsCommandChanges(engine, PageIndex::PageC, 0, kMaxTracks, RowsCommand::Paste, copied));
    paste.perform();
    REQUIRE(engine.getStep(2, PageIndex::PageC, 0).active);
    REQUIRE(engine.getStep(3, PageIndex::PageC, 0).active);

    clear.undo();
    REQUIRE(engine.getStep(3, PageIndex::PageB, 0).active);
}

TEST_CASE("Song position names the section, bar and page that play", "[plugin][song]") {
    const std::array<SongSection, kNumPages> sections {{
        { PageIndex::PageA, 2, false }, { PageIndex::PageB, 3, true },
        { PageIndex::PageC, 1, false }, { PageIndex::PageD, 1, false } }};

    const auto start = songPositionAt(sections, 0.5);
    CHECK(start.section == 0);
    CHECK(start.bar == 0);
    CHECK(start.page == PageIndex::PageA);
    const auto middle = songPositionAt(sections, 4.0 * 3.0 + 1.0);
    CHECK(middle.section == 1);
    CHECK(middle.bar == 1);
    CHECK(middle.page == PageIndex::PageB);
    CHECK(songPositionAt(sections, 4.0 * 4.0).page == PageIndex::PageD);
    CHECK(songPositionAt(sections, 4.0 * 7.0).section == 0);
}
