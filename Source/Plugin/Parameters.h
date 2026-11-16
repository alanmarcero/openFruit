#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <string>

namespace openfruit::params {

inline constexpr const char* kPage = "page";
inline constexpr const char* kTrackCount = "track_count";
inline constexpr const char* kTheme = "theme";
inline constexpr const char* kMasterVol = "master_vol";
inline constexpr const char* kOutputDb = "output_db";
inline constexpr const char* kSongMode = "song_mode";
inline constexpr const char* kSwing = "swing";
inline constexpr const char* kAccentAmount = "accent_amount";
inline constexpr const char* kAccentEvery = "accent_every";

// Helpers for per-track parameter IDs
inline std::string trackMidiChId(int trackIdx) { return "track_midi_ch_" + std::to_string(trackIdx); }
inline std::string trackVolId(int trackIdx) { return "track_vol_" + std::to_string(trackIdx); }
inline std::string trackPanId(int trackIdx) { return "track_pan_" + std::to_string(trackIdx); }
inline std::string trackTuneId(int trackIdx) { return "track_tune_" + std::to_string(trackIdx); }
inline std::string trackTimingId(int trackIdx) { return "track_timing_" + std::to_string(trackIdx); }
inline std::string trackNudgeId(int trackIdx) { return "track_nudge_" + std::to_string(trackIdx); }
inline std::string trackGrooveId(int trackIdx) { return "track_groove_" + std::to_string(trackIdx); }
inline std::string trackMuteId(int trackIdx) { return "track_mute_" + std::to_string(trackIdx); }
inline std::string trackSoloId(int trackIdx) { return "track_solo_" + std::to_string(trackIdx); }
inline std::string trackFilterId(int trackIdx) { return "track_filter_" + std::to_string(trackIdx); }
inline std::string trackAttackId(int trackIdx) { return "track_attack_" + std::to_string(trackIdx); }
inline std::string trackDecayId(int trackIdx) { return "track_decay_" + std::to_string(trackIdx); }
inline std::string trackSustainId(int trackIdx) { return "track_sustain_" + std::to_string(trackIdx); }
inline std::string trackReleaseId(int trackIdx) { return "track_release_" + std::to_string(trackIdx); }
inline std::string songSectionPageId(int sectionIdx) { return "song_section_page_" + std::to_string(sectionIdx); }
inline std::string songSectionBarsId(int sectionIdx) { return "song_section_bars_" + std::to_string(sectionIdx); }
inline std::string songSectionFillId(int sectionIdx) { return "song_section_fill_" + std::to_string(sectionIdx); }

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace openfruit::params
