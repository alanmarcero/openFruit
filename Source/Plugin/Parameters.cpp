#include "Parameters.h"
#include "SequencerTypes.h"

namespace openfruit::params {

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Automatable Page selector: Page A, B, C, D
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(kPage, 1),
        "Active Page",
        juce::StringArray{ "Page A", "Page B", "Page C", "Page D" },
        0
    ));

    // Track Count: 4, 8, 12, 16
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(kTrackCount, 1),
        "Track Count",
        juce::StringArray{ "4 Tracks", "8 Tracks", "12 Tracks", "16 Tracks" },
        1 // default 8
    ));

    // Theme selector
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(kTheme, 1),
        "Color Theme",
        juce::StringArray{ "Cyberpunk Neon", "Fruity Classic", "Synthwave Outrun", "Acid Matrix" },
        0
    ));

    // Master Volume
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kMasterVol, 1),
        "Master Volume",
        juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f),
        1.0f
    ));

    // Post-saturation output gain in decibels
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kOutputDb, 1),
        "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(kSongMode, 1), "Song Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kSwing, 1), "Swing",
        juce::NormalisableRange<float>(0.0f, 0.5f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("step")));

    for (int section = 0; section < openfruit::core::kNumPages; ++section) {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(songSectionPageId(section), 1),
            "Song Section " + juce::String(section + 1) + " Page",
            juce::StringArray {"Page A", "Page B", "Page C", "Page D"}, section));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(songSectionBarsId(section), 1),
            "Song Section " + juce::String(section + 1) + " Bars", 1, 32, 1));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(songSectionFillId(section), 1),
            "Song Section " + juce::String(section + 1) + " Fill", false));
    }

    // 16 Tracks Parameters
    for (int i = 0; i < openfruit::core::kMaxTracks; ++i) {
        const std::string prefix = "Track " + std::to_string(i + 1) + " ";

        // MIDI Channel (1 .. 16, defaults 1 .. 16)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(trackMidiChId(i), 1),
            prefix + "MIDI Channel",
            1,
            16,
            i + 1
        ));

        // Volume
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackVolId(i), 1),
            prefix + "Volume",
            juce::NormalisableRange<float>(0.0f, 1.5f, 0.01f),
            1.0f
        ));

        // Pan
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackPanId(i), 1),
            prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
            0.0f
        ));

        // Tune (-24 .. +24)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(trackTuneId(i), 1),
            prefix + "Tune",
            -24,
            24,
            0
        ));

        // Timing (1/16, 1/32, 1/64)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(trackTimingId(i), 1),
            prefix + "Timing",
            juce::StringArray{ "1/16", "1/32", "1/64" },
            0
        ));

        // Mute
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(trackMuteId(i), 1),
            prefix + "Mute",
            false
        ));

        // Solo
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(trackSoloId(i), 1),
            prefix + "Solo",
            false
        ));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackFilterId(i), 1), prefix + "Filter",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.35f), 20000.0f,
            juce::AudioParameterFloatAttributes().withLabel("Hz")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackAttackId(i), 1), prefix + "Attack",
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("s")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackDecayId(i), 1), prefix + "Decay",
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("s")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackSustainId(i), 1), prefix + "Sustain",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel("%")));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackReleaseId(i), 1), prefix + "Release",
            juce::NormalisableRange<float>(0.0f, 20.0f, 0.001f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel("s")));
    }

    return { params.begin(), params.end() };
}

} // namespace openfruit::params
