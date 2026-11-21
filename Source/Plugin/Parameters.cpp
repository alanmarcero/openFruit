#include "Parameters.h"
#include "SequencerTypes.h"

namespace openfruit::params {
namespace {

// Shows a stored value as a whole percent. Typed text parses with or without "%".
juce::AudioParameterFloatAttributes percent(float scale, bool showPlus) {
    return juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([scale, showPlus](float value, int) {
            const int whole = juce::roundToInt(value * scale);
            const juce::String sign = showPlus && whole > 0 ? "+" : "";
            return sign + juce::String(whole) + "%";
        })
        .withValueFromStringFunction([scale](const juce::String& text) { return text.getFloatValue() / scale; });
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // The units travel in the text, so the editor, typed entry and automation lanes agree.
    const auto decibels = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int) -> juce::String {
            const float rounded = std::round(value * 10.0f) / 10.0f;
            if (rounded == 0.0f)
                return "0.0 dB";
            const juce::String sign = rounded > 0.0f ? "+" : "";
            return sign + juce::String(rounded, 1) + " dB";
        })
        .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); });
    const auto gain = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([toText = decibels.getStringFromValueFunction()](float value, int length) {
            if (value <= 0.0f)
                return juce::String("-inf dB");
            return toText(juce::Decibels::gainToDecibels(value), length);
        })
        .withValueFromStringFunction([](const juce::String& text) {
            if (text.trim().startsWithIgnoreCase("-inf"))
                return 0.0f;
            return juce::Decibels::decibelsToGain(text.getFloatValue());
        });
    const auto hertz = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int) {
            const int whole = juce::roundToInt(value);
            if (whole < 1000)
                return juce::String(whole) + " Hz";
            return juce::String(value / 1000.0f, 2) + " kHz";
        })
        .withValueFromStringFunction([](const juce::String& text) {
            if (text.containsIgnoreCase("k"))
                return text.getFloatValue() * 1000.0f;
            return text.getFloatValue();
        });
    const auto seconds = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int) {
            const float milliseconds = value * 1000.0f;
            if (milliseconds < 9.95f)
                return juce::String(milliseconds, 1) + " ms";
            if (milliseconds < 999.5f)
                return juce::String(juce::roundToInt(milliseconds)) + " ms";
            return juce::String(value, 2) + " s";
        })
        .withValueFromStringFunction([](const juce::String& text) {
            // A bare number is in milliseconds, the unit the display uses below one second.
            const auto trimmed = text.trim();
            if (trimmed.endsWithIgnoreCase("s") && !trimmed.endsWithIgnoreCase("ms"))
                return trimmed.getFloatValue();
            return trimmed.getFloatValue() / 1000.0f;
        });
    const auto pan = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float value, int) {
            const int amount = juce::roundToInt(std::abs(value) * 100.0f);
            if (amount == 0)
                return juce::String("C");
            return juce::String(amount) + (value < 0.0f ? " L" : " R");
        })
        .withValueFromStringFunction([](const juce::String& text) {
            const float amount = text.retainCharacters("0123456789.").getFloatValue() / 100.0f;
            if (text.containsIgnoreCase("l"))
                return -amount;
            if (text.containsIgnoreCase("r"))
                return amount;
            return text.getFloatValue() / 100.0f;
        });

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
        1.0f,
        gain
    ));

    // Post-saturation output gain in decibels
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kOutputDb, 1),
        "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        decibels
    ));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(kSongMode, 1), "Song Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kSwing, 1), "Swing",
        juce::NormalisableRange<float>(0.0f, 0.5f, 0.001f), 0.0f,
        percent(100.0f, false)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kAccentAmount, 1), "Accent Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f, percent(100.0f, false)));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID(kAccentEvery, 1), "Accent Every", 2, 16, 4));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(kHumanize, 1), "Humanize",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
        percent(1.0f, false)));

    for (int section = 0; section < openfruit::core::kNumPages; ++section) {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(songSectionPageId(section), 1),
            "Song Section " + juce::String(section + 1) + " Page",
            juce::StringArray {"Page A", "Page B", "Page C", "Page D"}, section));
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(songSectionBarsId(section), 1),
            "Song Section " + juce::String(section + 1) + " Bars", 1, 32, 1,
            juce::AudioParameterIntAttributes().withStringFromValueFunction([](int bars, int) {
                return juce::String(bars) + (bars == 1 ? " bar" : " bars");
            })));
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
            1.0f,
            gain
        ));

        // Pan
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackPanId(i), 1),
            prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
            0.0f,
            pan
        ));

        // Tune (-24 .. +24)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID(trackTuneId(i), 1),
            prefix + "Tune",
            -24,
            24,
            0,
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction([](int semitones, int) {
                    return (semitones > 0 ? "+" : "") + juce::String(semitones) + " st";
                })
                .withValueFromStringFunction([](const juce::String& text) {
                    return text.retainCharacters("-0123456789").getIntValue();
                })
        ));

        // Timing (1/16, 1/32, 1/64)
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(trackTimingId(i), 1),
            prefix + "Timing",
            juce::StringArray{ "1/16", "1/32", "1/64" },
            0
        ));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackNudgeId(i), 1), prefix + "Nudge",
            juce::NormalisableRange<float>(-0.5f, 0.5f, 0.001f), 0.0f,
            percent(100.0f, true)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackGrooveId(i), 1), prefix + "Groove",
            juce::NormalisableRange<float>(0.0f, 0.5f, 0.001f), 0.0f,
            percent(100.0f, false)));

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
            hertz));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackHpfId(i), 1), prefix + "High-Pass",
            juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.35f), 20.0f,
            hertz));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackDriveId(i), 1), prefix + "Drive",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
            percent(1.0f, false)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackAttackId(i), 1), prefix + "Attack",
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f), 0.0f,
            seconds));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackDecayId(i), 1), prefix + "Decay",
            juce::NormalisableRange<float>(0.0f, 10.0f, 0.001f), 0.0f,
            seconds));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackSustainId(i), 1), prefix + "Sustain",
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f,
            percent(1.0f, false)));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackReleaseId(i), 1), prefix + "Release",
            juce::NormalisableRange<float>(0.0f, 20.0f, 0.001f), 0.0f,
            seconds));
    }

    return { params.begin(), params.end() };
}

} // namespace openfruit::params
