#pragma once

#include <juce_graphics/juce_graphics.h>
#include "SequencerTypes.h"

namespace openfruit::theme {

struct Palette {
    juce::Colour background;
    juce::Colour panel;
    juce::Colour panelEdge;
    juce::Colour text;
    juce::Colour textDim;
    juce::Colour accentPrimary;
    juce::Colour accentSecondary;
    juce::Colour padInactiveBeatA;
    juce::Colour padInactiveBeatB;
    juce::Colour padActive;
    juce::Colour padGlow;
    juce::Colour playheadGlow;
    juce::Colour midiChannelBadge;
};

inline Palette getPalette(openfruit::core::ColorTheme theme) noexcept {
    switch (theme) {
        case openfruit::core::ColorTheme::CyberpunkNeon:
            return Palette{
                .background = juce::Colour(0xff090c12),
                .panel = juce::Colour(0xff101622),
                .panelEdge = juce::Colour(0xff1f2c42),
                .text = juce::Colour(0xffe6f1ff),
                .textDim = juce::Colour(0xff7b91af),
                .accentPrimary = juce::Colour(0xffff007f),    // Cyber Magenta
                .accentSecondary = juce::Colour(0xff00f0ff),  // Neon Cyan
                .padInactiveBeatA = juce::Colour(0xff1b2333),
                .padInactiveBeatB = juce::Colour(0xff131a27),
                .padActive = juce::Colour(0xffff007f),
                .padGlow = juce::Colour(0x66ff007f),
                .playheadGlow = juce::Colour(0xff00f0ff),
                .midiChannelBadge = juce::Colour(0xffff9900)  // Cyber Orange
            };

        case openfruit::core::ColorTheme::FruityClassic:
            return Palette{
                .background = juce::Colour(0xff18181a),
                .panel = juce::Colour(0xff222326),
                .panelEdge = juce::Colour(0xff35373d),
                .text = juce::Colour(0xffededed),
                .textDim = juce::Colour(0xff8a8d94),
                .accentPrimary = juce::Colour(0xffff7700),    // Fruity Orange
                .accentSecondary = juce::Colour(0xff55cc22),  // Fruity Green
                .padInactiveBeatA = juce::Colour(0xff4a3a2a), // warm beat 1
                .padInactiveBeatB = juce::Colour(0xff2e3a2e), // cool beat 2
                .padActive = juce::Colour(0xffff8800),
                .padGlow = juce::Colour(0x66ff8800),
                .playheadGlow = juce::Colour(0xffffffff),
                .midiChannelBadge = juce::Colour(0xff55cc22)
            };

        case openfruit::core::ColorTheme::SynthwaveOutrun:
            return Palette{
                .background = juce::Colour(0xff120824),
                .panel = juce::Colour(0xff1d0f36),
                .panelEdge = juce::Colour(0xff3c1b68),
                .text = juce::Colour(0xffffe6ff),
                .textDim = juce::Colour(0xffb07ecb),
                .accentPrimary = juce::Colour(0xffff2a85),    // Hot Pink
                .accentSecondary = juce::Colour(0xff05d9e8),  // Electric Teal
                .padInactiveBeatA = juce::Colour(0xff2b144d),
                .padInactiveBeatB = juce::Colour(0xff1a0b33),
                .padActive = juce::Colour(0xffff2a85),
                .padGlow = juce::Colour(0x66ff2a85),
                .playheadGlow = juce::Colour(0xffffd319),      // Sunset Yellow
                .midiChannelBadge = juce::Colour(0xff05d9e8)
            };

        case openfruit::core::ColorTheme::AcidMatrix:
            return Palette{
                .background = juce::Colour(0xff060c07),
                .panel = juce::Colour(0xff0a160d),
                .panelEdge = juce::Colour(0xff14331d),
                .text = juce::Colour(0xffd4ffd8),
                .textDim = juce::Colour(0xff539260),
                .accentPrimary = juce::Colour(0xff00ff66),    // Terminal Phosphor Green
                .accentSecondary = juce::Colour(0xffffbb00),  // Amber
                .padInactiveBeatA = juce::Colour(0xff102917),
                .padInactiveBeatB = juce::Colour(0xff0a1c10),
                .padActive = juce::Colour(0xff00ff66),
                .padGlow = juce::Colour(0x6600ff66),
                .playheadGlow = juce::Colour(0xffffbb00),
                .midiChannelBadge = juce::Colour(0xffffbb00)
            };
    }
    return getPalette(openfruit::core::ColorTheme::CyberpunkNeon);
}

} // namespace openfruit::theme
