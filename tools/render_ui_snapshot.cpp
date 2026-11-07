#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DefaultKit.h"
#include "SonicAnalyzer.h"
#include <iostream>
#include <vector>
#include <cmath>

namespace {

void writeSimpleWav(const juce::File& file, const std::vector<float>& samples, int sampleRate) {
    file.deleteFile();
    juce::FileOutputStream out(file);
    const int numSamples = static_cast<int>(samples.size());
    const int dataSize = numSamples * 2;
    const int fileSize = 36 + dataSize;

    out.write("RIFF", 4);
    out.writeInt(fileSize);
    out.write("WAVEfmt ", 8);
    out.writeInt(16);
    out.writeShort(1);
    out.writeShort(1);
    out.writeInt(sampleRate);
    out.writeInt(sampleRate * 2);
    out.writeShort(2);
    out.writeShort(16);
    out.write("data", 4);
    out.writeInt(dataSize);

    for (float s : samples) {
        const int16_t v = static_cast<int16_t>(std::clamp(s, -1.0f, 1.0f) * 32767.0f);
        out.writeShort(v);
    }
}

void populateSampleFolder(const juce::File& folder, openfruit::core::ToneCache& cache) {
    folder.createDirectory();
    constexpr int sr = 44100;
    constexpr double kPi = 3.14159265358979323846;

    struct SampleSpec {
        std::string name;
        double freq;
        double duration;
        bool isSine;
    };

    const std::vector<SampleSpec> specs = {
        { "kick_deep_sub_A1.wav", 55.0, 0.45, true },
        { "kick_punch_F1.wav", 43.65, 0.32, true },
        { "snare_acoustic_D2.wav", 73.4, 0.22, true },
        { "hat_closed_tight.wav", 0.0, 0.06, false },
        { "hat_open_sizzle.wav", 0.0, 0.35, false },
        { "synth_lead_stab_C3.wav", 130.81, 0.28, true },
        { "tom_electronic_A2.wav", 110.0, 0.30, true },
        { "clap_vintage_808.wav", 0.0, 0.20, false }
    };

    for (const auto& sp : specs) {
        const juce::File file = folder.getChildFile(sp.name);
        const int numFrames = static_cast<int>(sp.duration * sr);
        std::vector<float> data(static_cast<size_t>(numFrames));

        for (int i = 0; i < numFrames; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(sr);
            const float env = static_cast<float>(std::exp(-5.0 * t / sp.duration));
            if (sp.isSine) {
                data[static_cast<size_t>(i)] = static_cast<float>(std::sin(2.0 * kPi * sp.freq * t)) * env;
            } else {
                const float noise = static_cast<float>((std::rand() % 2000 - 1000) / 1000.0);
                data[static_cast<size_t>(i)] = noise * env;
            }
        }

        writeSimpleWav(file, data, sr);

        // Pre-cache analyzed tone
        const auto res = openfruit::core::SonicAnalyzer::analyze(data.data(), numFrames, sr);
        const int64_t modTime = file.getLastModificationTime().toMilliseconds() / 1000;
        const int64_t fsize = file.getSize();
        cache.put(file.getFullPathName().toStdString(), modTime, fsize, res);
    }
}

bool saveSnapshot(juce::Component* comp, const juce::File& file) {
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    const juce::Image snapshot = comp->createComponentSnapshot(comp->getLocalBounds(), false, 2.0f);
    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) {
        return false;
    }

    juce::PNGImageFormat png;
    return png.writeImageToStream(snapshot, stream);
}

} // namespace

int main() {
    juce::ScopedJuceInitialiser_GUI guiInit;

    OpenFruitAudioProcessor processor;
    auto* editor = dynamic_cast<OpenFruitAudioProcessorEditor*>(processor.createEditor());
    if (!editor) {
        std::cerr << "Failed to create editor\n";
        return 1;
    }

    editor->setSize(1120, 720);

    const juce::File baseDir = juce::File::getCurrentWorkingDirectory();
    const juce::File trackerScreenshot = baseDir.getChildFile("assets/openFruit-screenshot.png");
    const juce::File browserScreenshot = baseDir.getChildFile("assets/openFruit-browser-screenshot.png");
    const juce::File sampleDir = baseDir.getChildFile("build/sample_library");

    // 1. Render Tracker Screenshot
    if (saveSnapshot(editor, trackerScreenshot)) {
        std::cout << "Saved tracker screenshot to " << trackerScreenshot.getFullPathName() << "\n";
    }

    // 2. Setup samples and render Browser Screenshot
    populateSampleFolder(sampleDir, processor.getToneCache());
    editor->openSampleBrowser(0, sampleDir);

    // Allow component layout to refresh
    editor->repaint();

    if (saveSnapshot(editor, browserScreenshot)) {
        std::cout << "Saved browser screenshot to " << browserScreenshot.getFullPathName() << "\n";
    }

    delete editor;
    return 0;
}
