#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "ToneCache.h"
#include <filesystem>

using namespace openfruit::core;

TEST_CASE("ToneCache basic insertion and retrieval", "[ToneCache]") {
    ToneCache cache;
    REQUIRE(cache.size() == 0);

    ToneAnalysisResult res;
    res.dominantFrequencyHz = 440.0;
    res.noteName = "A4";
    res.midiNote = 69;
    res.detuneCents = 0;
    res.durationSeconds = 1.25;
    res.durationSamples = 55125;
    res.peakAmplitude = 0.95f;
    res.rmsAmplitude = 0.65f;
    res.hasClearTone = true;

    cache.put("/samples/kick01.wav", 1700000000, 102400, res);
    REQUIRE(cache.size() == 1);

    auto retrieved = cache.get("/samples/kick01.wav", 1700000000, 102400);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->noteName == "A4");
    REQUIRE(retrieved->midiNote == 69);
    REQUIRE_THAT(retrieved->durationSeconds, Catch::Matchers::WithinAbs(1.25, 0.001));

    // Modified timestamp should miss cache
    auto miss = cache.get("/samples/kick01.wav", 1700000050, 102400);
    REQUIRE_FALSE(miss.has_value());

    // Clear cache
    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.get("/samples/kick01.wav", 1700000000, 102400).has_value());
}

TEST_CASE("ToneCache disk serialization and reload", "[ToneCache]") {
    const std::string tempFile = (std::filesystem::temp_directory_path() / "test_openfruit_cache.tsv").string();

    ToneCache cache1;
    ToneAnalysisResult res1;
    res1.dominantFrequencyHz = 130.8;
    res1.noteName = "C3";
    res1.midiNote = 48;
    res1.durationSeconds = 0.5;
    res1.hasClearTone = true;
    cache1.put("/samples/bass.wav", 1600000000, 44100, res1);

    REQUIRE(cache1.saveToFile(tempFile));

    ToneCache cache2;
    REQUIRE(cache2.loadFromFile(tempFile));
    REQUIRE(cache2.size() == 1);

    auto loaded = cache2.get("/samples/bass.wav", 1600000000, 44100);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->noteName == "C3");
    REQUIRE(loaded->midiNote == 48);

    std::filesystem::remove(tempFile);
}
