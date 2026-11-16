#include "MidiEventProcessor.h"
#include <algorithm>
#include <cmath>

namespace openfruit::plugin {

void processMidiBlock(core::FruitEngine& engine,
                      const juce::MidiBuffer& midiMessages,
                      float* left,
                      float* right,
                      int numSamples,
                      double bpm,
                      double ppqPosition,
                      bool isHostPlaying,
                      double sampleRate) {
    const double effectiveBpm = std::isfinite(bpm) && bpm > 10.0 ? bpm : 120.0;
    const double effectiveSampleRate = std::max(8000.0, sampleRate);
    const double quarterNotesPerSample = effectiveBpm / (effectiveSampleRate * 60.0);
    const auto renderSegment = [&](int start, int end) {
        if (end <= start) return;
        const double segmentPpq = ppqPosition >= 0.0
            ? ppqPosition + static_cast<double>(start) * quarterNotesPerSample
            : ppqPosition;
        engine.process(left + start, right + start, end - start, bpm, segmentPpq, isHostPlaying);
    };

    int renderedThrough = 0;
    for (const auto metadata : midiMessages) {
        const int eventOffset = std::clamp(metadata.samplePosition, renderedThrough, numSamples);
        renderSegment(renderedThrough, eventOffset);

        const auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            engine.handleMidiNoteOn(message.getChannel(), message.getNoteNumber(), message.getFloatVelocity());
        } else if (message.isNoteOff()) {
            engine.handleMidiNoteOff(message.getChannel(), message.getNoteNumber());
        }

        renderedThrough = eventOffset;
    }

    renderSegment(renderedThrough, numSamples);
}

} // namespace openfruit::plugin
