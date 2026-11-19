#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Controls.h"
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

        float prev = 0.0f;
        const bool isHat = (sp.name.find("hat_") != std::string::npos);
        for (int i = 0; i < numFrames; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(sr);
            const float env = static_cast<float>(std::exp(-5.0 * t / sp.duration));
            if (sp.isSine) {
                data[static_cast<size_t>(i)] = static_cast<float>(std::sin(2.0 * kPi * sp.freq * t)) * env;
            } else {
                const float noise = static_cast<float>((std::rand() % 2000 - 1000) / 1000.0);
                const float sample = isHat ? (noise - prev * 0.85f) : noise;
                prev = noise;
                data[static_cast<size_t>(i)] = sample * env;
            }
        }

        writeSimpleWav(file, data, sr);

        // Pre-cache analyzed tone
        const auto res = openfruit::core::SonicAnalyzer::analyze(data.data(), numFrames, sr);
        int64_t modTime = 0;
        int64_t fsize = 0;
        if (!openfruit::core::ToneCache::getFileStats(file.getFullPathName().toStdString(), modTime, fsize)) {
            modTime = file.getLastModificationTime().toMilliseconds() / 1000;
            fsize = file.getSize();
        }
        cache.put(file.getFullPathName().toStdString(), modTime, fsize, res);
    }
}

bool saveSnapshot(juce::Component* comp, const juce::File& file) {
    file.getParentDirectory().createDirectory();
    file.deleteFile();

    const juce::Image snapshot = comp->createComponentSnapshot(comp->getLocalBounds(), false, 1.0f);
    juce::FileOutputStream stream(file);
    if (!stream.openedOk()) {
        return false;
    }

    juce::PNGImageFormat png;
    return png.writeImageToStream(snapshot, stream);
}

void check(bool condition, const juce::String& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

template<typename T> T* findControl(juce::Component& parent, const juce::String& name = {}) {
    for (auto* component : parent.getChildren()) {
        if (!component->isVisible()) continue;
        if (auto* control = dynamic_cast<T*>(component)) {
            auto* button = dynamic_cast<juce::Button*>(component);
            if (name.isEmpty() || component->getName() == name || (button && button->getButtonText() == name)) return control;
        }
        if (auto* child = findControl<T>(*component, name)) return child;
    }
    return nullptr;
}

void click(juce::Component& editor, const juce::String& name) {
    auto* button = findControl<juce::Button>(editor, name);
    check(button != nullptr && button->isEnabled(), "Missing button " + name);
    auto pos = button->getLocalBounds().getCentre().toFloat();
    const auto now = juce::Time::getCurrentTime();
    juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys::leftButtonModifier,
                            1,0,0,0,0, button,button,now,pos,now,1,false);
    juce::Component& target = *button;
    target.mouseDown(event);
    target.mouseUp(event);
}

void checkLayout(juce::Component& component) {
    for (auto* child : component.getChildren()) {
        if (!child->isVisible()) continue;
        if (auto* button = dynamic_cast<juce::Button*>(child)) {
            if (dynamic_cast<juce::TextButton*>(button) || dynamic_cast<juce::ToggleButton*>(button)) {
                check(button->getWidth() >= 32 && button->getHeight() >= 28, "Small button " + button->getButtonText());
                check(component.getLocalBounds().contains(button->getBounds()), "Clipped button " + button->getButtonText());
                check(button->getTooltip().isNotEmpty(), "Missing tooltip " + button->getButtonText());
            }
        }
        checkLayout(*child);
    }
}

} // namespace

int main() try {
    juce::ScopedJuceInitialiser_GUI guiInit;
    OpenFruitAudioProcessor processor;
    processor.setRateAndBufferSizeDetails(48000,480);
    processor.prepareToPlay(48000,480);
    std::unique_ptr<OpenFruitAudioProcessorEditor> editor(static_cast<OpenFruitAudioProcessorEditor*>(processor.createEditor()));
    const auto base = juce::File::getCurrentWorkingDirectory();
    auto& engine = processor.getEngine();
    auto& state = processor.getAPVTS();
    const auto capture = [&](const juce::String& view) {
        editor->refresh();
        checkLayout(*editor);
        check(saveSnapshot(editor.get(), base.getChildFile("assets/openFruit-" + view + ".png")), view);
    };
    capture("screenshot");
    std::array<float, 480> frameL{}, frameR{};
    engine.process(frameL.data(), frameR.data(), 480, 120, .05, true);
    const auto illustration = editor->createComponentSnapshot({16,12,64,64});
    engine.process(frameL.data(), frameR.data(), 480, 120, 1.1, true);
    const auto turned = editor->createComponentSnapshot({16,12,64,64});
    int paintedChanges = 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (illustration.getPixelAt(x,y) != turned.getPixelAt(x,y)) ++paintedChanges;
    check(paintedChanges > 100, "Host beat rotates the SVG");
    engine.process(frameL.data(), frameR.data(), 480, 120, 0, false);
    for (const auto* count : {"4","12","16","8"}) {
        click(*editor, count);
        check(engine.getActiveTrackCount() == juce::String(count).getIntValue(), "Track count");
        checkLayout(*editor);
    }
    click(*editor, "Mute");
    check(engine.getTrack(0).mute, "Mute");
    click(*editor, "Mute");
    click(*editor, "Solo");
    check(engine.getTrack(0).solo, "Solo");
    click(*editor, "Solo");
    click(*editor, "Navel kick");
    click(*editor, "Edit");
    check(findControl<openfruit::ui::TrackVoiceSettingsPanel>(*editor) != nullptr, "Native track inspector");
    capture("track-settings-screenshot");
    click(*editor, "Done");
    click(*editor, "Song mode");
    click(*editor, "B");
    auto* bars = state.getParameter(openfruit::params::songSectionBarsId(1));
    bars->setValueNotifyingHost(bars->convertTo0to1(4));
    click(*editor, "Fill");
    capture("song-mode-screenshot");
    click(*editor, "Song mode");
    click(*editor, "A");

    auto* theme = findControl<juce::ComboBox>(*editor);
    check(theme != nullptr, "Theme selector");
    for (int t : {2,3,4,1}) {
        theme->setSelectedId(t, juce::sendNotificationSync);
        check(static_cast<int>(engine.getColorTheme()) == t-1, "Theme");
        checkLayout(*editor);
    }
    click(*editor, "Browse");
    auto* browser = findControl<openfruit::ui::SampleBrowserModal>(*editor);
    check(browser != nullptr, "Native browser");
    const auto library = base.getChildFile("build/sample_library");
    populateSampleFolder(library, processor.getToneCache());
    browser->setFolder(library);
    auto* table = findControl<juce::TableListBox>(*browser);
    check(table && table->getNumRows() == 8, "Library files");
    table->selectRow(2);
    auto selected = engine.getTrack(0).auditionSample;
    check(selected != nullptr, "Audition selection");
    auto* sort = findControl<juce::ComboBox>(*browser);
    sort->setSelectedId(3, juce::sendNotificationSync);
    click(*editor, "Preview");
    check(engine.getTrack(0).auditionSample->filePath == selected->filePath, "Sort keeps the selected file");
    capture("browser-screenshot");
    click(*editor, "Load sample");
    check(engine.loadedSample(0)->filePath == selected->filePath, "Loaded sample");
    juce::MemoryBlock session;
    processor.getStateInformation(session);
    OpenFruitAudioProcessor recalled;
    recalled.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(recalled.getEngine().loadedSample(0)->filePath == selected->filePath, "External sample restored");
    check(recalled.getEngine().getStep(1,openfruit::core::PageIndex::PageD,15).ratchets == 2, "Fill pattern restored");
    auto old = juce::AudioProcessor::getXmlFromBinary(session.getData(), static_cast<int>(session.getSize()));
    old->removeAttribute("factoryKit");
    old->deleteAllChildElementsWithTagName("SampleFiles");
    old->deleteAllChildElementsWithTagName("StepPatterns");
    juce::AudioProcessor::copyXmlToBinary(*old, session);
    recalled.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    check(recalled.getEngine().loadedSample(0)->sampleName == "808 kick", "Legacy kit restored");
    check(!recalled.getEngine().getStep(0,openfruit::core::PageIndex::PageA,0).active, "Legacy empty pattern");

    OpenFruitAudioProcessor factory;
    const auto audioDir = base.getChildFile("build/citrus-audio");
    audioDir.createDirectory();
    for (int i = 0; i < 16; ++i) {
        const auto sample = factory.getEngine().loadedSample(i);
        const auto& pcm = sample->channels.front();
        const float peak = *std::max_element(pcm.begin(),pcm.end(), [](float x,float y){ return std::abs(x)<std::abs(y); });
        check(std::isfinite(peak) && std::abs(peak) >= .5f && std::abs(peak) < 1, "Factory peak");
        writeSimpleWav(audioDir.getChildFile(sample->sampleName + ".wav"), pcm, static_cast<int>(sample->sampleRate));
    }
    for (const int rate : {32000,48000,96000}) {
        auto& synth = factory.getEngine();
        synth.prepare(rate,480);
        synth.setPlaying(true);
        std::vector<float> song;
        const int blockSize = 480;
        std::array<float,480> left{}, right{};
        for (int page = 0; page < 4; ++page) {
            synth.setActivePage(static_cast<openfruit::core::PageIndex>(page));
            double energy = 0;
            for (int offset = 0; offset < rate*2; offset += blockSize) {
                const int frames = std::min(blockSize,rate*2-offset);
                left.fill(0); right.fill(0);
                synth.process(left.data(),right.data(),frames,120,page*4.0+2.0*offset/rate,true);
                for (int j = 0; j < frames; ++j) {
                    const float sample = .5f*(left[static_cast<size_t>(j)]+right[static_cast<size_t>(j)]);
                    check(std::isfinite(sample) && std::abs(sample) <= 1.01f, "Factory mix headroom");
                    song.push_back(sample);
                    energy += sample*sample;
                }
            }
            check(std::sqrt(energy/(rate*2)) > .025, "Starter page rendered");
        }
        writeSimpleWav(audioDir.getChildFile("four-pages-" + juce::String(rate) + ".wav"), song, rate);
    }
    std::cout << "Native layout, controls, file restore, legacy kit and audio renders passed.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
