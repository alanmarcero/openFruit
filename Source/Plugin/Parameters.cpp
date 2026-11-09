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
    }

    return { params.begin(), params.end() };
}

} // namespace openfruit::params
