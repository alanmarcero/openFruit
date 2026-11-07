#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <string>

namespace openfruit::params {

inline constexpr const char* kPage = "page";
inline constexpr const char* kTrackCount = "track_count";
inline constexpr const char* kInputMode = "input_mode";
inline constexpr const char* kTheme = "theme";
inline constexpr const char* kMasterVol = "master_vol";

// Helpers for per-track parameter IDs
inline std::string trackVolId(int trackIdx) { return "track_vol_" + std::to_string(trackIdx); }
inline std::string trackPanId(int trackIdx) { return "track_pan_" + std::to_string(trackIdx); }
inline std::string trackTuneId(int trackIdx) { return "track_tune_" + std::to_string(trackIdx); }
inline std::string trackTimingId(int trackIdx) { return "track_timing_" + std::to_string(trackIdx); }
inline std::string trackMuteId(int trackIdx) { return "track_mute_" + std::to_string(trackIdx); }
inline std::string trackSoloId(int trackIdx) { return "track_solo_" + std::to_string(trackIdx); }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openfruit::params
