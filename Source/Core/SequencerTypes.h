#pragma once

#include <string>
#include <array>
#include <vector>
#include <cstdint>

namespace openfruit::core {

inline constexpr int kMaxTracks = 16;
inline constexpr int kStepsPerPage = 16;
inline constexpr int kNumPages = 4; // A, B, C, D
inline constexpr int kMaxSemitoneTune = 24;
inline constexpr int kMinSemitoneTune = -24;

enum class PageIndex : int {
    PageA = 0,
    PageB = 1,
    PageC = 2,
    PageD = 3
};

inline const char* pageName(PageIndex page) noexcept {
    switch (page) {
        case PageIndex::PageA: return "A";
        case PageIndex::PageB: return "B";
        case PageIndex::PageC: return "C";
        case PageIndex::PageD: return "D";
    }
    return "A";
}

enum class StepTiming : int {
    Timing1_16 = 0, // standard 1/16th notes (4 steps per quarter note)
    Timing1_32 = 1, // fast 1/32nd notes (8 steps per quarter note)
    Timing1_64 = 2  // rapid 1/64th notes (16 steps per quarter note)
};

inline const char* timingName(StepTiming timing) noexcept {
    switch (timing) {
        case StepTiming::Timing1_16: return "1/16";
        case StepTiming::Timing1_32: return "1/32";
        case StepTiming::Timing1_64: return "1/64";
    }
    return "1/16";
}

inline double stepsPerQuarter(StepTiming timing) noexcept {
    switch (timing) {
        case StepTiming::Timing1_16: return 4.0;
        case StepTiming::Timing1_32: return 8.0;
        case StepTiming::Timing1_64: return 16.0;
    }
    return 4.0;
}

enum class TrackCountMode : int {
    Tracks4 = 4,
    Tracks8 = 8,
    Tracks12 = 12,
    Tracks16 = 16
};

enum class InputControlMode : int {
    SequencerPads = 0, // Fruity Loops style step sequencer pads drive playback
    MidiMode = 1       // Pads disabled; incoming MIDI notes dictate pitch (+/- 24) and velocity
};

struct StepData {
    bool active = false;
    float velocity = 0.85f;    // 0.0 .. 1.0
    int semitoneOffset = 0;    // -24 .. +24 semitones from base sample

    bool operator==(const StepData& other) const noexcept {
        return active == other.active &&
               semitoneOffset == other.semitoneOffset &&
               std::abs(velocity - other.velocity) < 1e-4f;
    }
};

using PagePattern = std::array<StepData, kStepsPerPage>;

struct TrackPatternPages {
    std::array<PagePattern, kNumPages> pages;

    TrackPatternPages() {
        for (auto& page : pages) {
            for (auto& step : page) {
                step = StepData{};
            }
        }
    }
};

enum class ColorTheme : int {
    CyberpunkNeon = 0, // Default: Neon Magenta, Cyan, Electric Orange on Gunmetal
    FruityClassic = 1, // Retro FL style: Charcoal, 4-beat orange and green grouping
    SynthwaveOutrun = 2, // Deep purple, hot pink, sunset yellow
    AcidMatrix = 3     // Phosphor terminal green, amber accents on deep obsidian
};

inline const char* themeName(ColorTheme theme) noexcept {
    switch (theme) {
        case ColorTheme::CyberpunkNeon: return "Cyberpunk Neon";
        case ColorTheme::FruityClassic: return "Fruity Classic";
        case ColorTheme::SynthwaveOutrun: return "Synthwave Outrun";
        case ColorTheme::AcidMatrix: return "Acid Matrix";
    }
    return "Cyberpunk Neon";
}

} // namespace openfruit::core
