#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace {

std::unique_ptr<OpenFruitAudioProcessorEditor> openEditor(OpenFruitAudioProcessor& processor) {
    auto editor = std::unique_ptr<OpenFruitAudioProcessorEditor>(
        static_cast<OpenFruitAudioProcessorEditor*>(processor.createEditor()));
    editor->refresh();
    return editor;
}

juce::Button* findButton(juce::Component& parent, const juce::String& text) {
    for (auto* child : parent.getChildren()) {
        if (!child->isVisible())
            continue;
        if (auto* button = dynamic_cast<juce::Button*>(child); button != nullptr && button->getButtonText() == text)
            return button;
        if (auto* found = findButton(*child, text))
            return found;
    }
    return nullptr;
}

template <typename Type>
Type* findVisible(juce::Component& parent, const juce::String& name = {}) {
    for (auto* child : parent.getChildren()) {
        if (!child->isVisible())
            continue;
        if (auto* match = dynamic_cast<Type*>(child); match != nullptr && (name.isEmpty() || child->getName() == name))
            return match;
        if (auto* found = findVisible<Type>(*child, name))
            return found;
    }
    return nullptr;
}

juce::MouseEvent mouseAt(juce::Component& target, juce::ModifierKeys modifiers, int clicks = 1) {
    const auto position = target.getLocalBounds().getCentre().toFloat();
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), position, modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             &target, &target, now, position, now, clicks, false };
}

void click(juce::Component& target, int clicks = 1) {
    const auto event = mouseAt(target, juce::ModifierKeys::leftButtonModifier, clicks);
    target.mouseDown(event);
    target.mouseUp(event);
    if (clicks == 2)
        target.mouseDoubleClick(event);
}

juce::String describe(juce::Component& component) {
    if (auto* button = dynamic_cast<juce::Button*>(&component))
        return "button '" + button->getButtonText() + "'";
    if (component.getTitle().isNotEmpty())
        return component.getTitle();
    return component.getName().isNotEmpty() ? component.getName() : juce::String(typeid(component).name());
}

// Overlays and the resize corner may cover their siblings by design.
bool floatsAbove(juce::Component& component) {
    return dynamic_cast<juce::ResizableCornerComponent*>(&component) != nullptr
        || dynamic_cast<juce::TooltipWindow*>(&component) != nullptr
        || dynamic_cast<openfruit::suite::PresetBrowser*>(&component) != nullptr;
}

// JUCE widgets lay out their own parts.
bool ownsItsParts(juce::Component& component) {
    return dynamic_cast<juce::Button*>(&component) != nullptr || dynamic_cast<juce::Slider*>(&component) != nullptr
        || dynamic_cast<juce::ComboBox*>(&component) != nullptr || dynamic_cast<juce::Label*>(&component) != nullptr
        || dynamic_cast<juce::ListBox*>(&component) != nullptr || dynamic_cast<juce::TextEditor*>(&component) != nullptr;
}

void checkLayout(juce::Component& parent, juce::StringArray& problems) {
    juce::Array<juce::Component*> placed;
    for (auto* child : parent.getChildren()) {
        if (!child->isVisible())
            continue;
        if (auto* button = dynamic_cast<juce::Button*>(child)) {
            const bool icon = button->getButtonText().length() == 1 && !button->getButtonText().containsAnyOf("ABCD0123456789");
            if (button->getHeight() < 36)
                problems.add(describe(*button) + " is " + juce::String(button->getHeight()) + " px tall");
            if (icon && button->getWidth() < 40)
                problems.add(describe(*button) + " is " + juce::String(button->getWidth()) + " px wide");
            if (button->getTooltip().isEmpty())
                problems.add(describe(*button) + " has no tooltip");
        }
        if (auto* slider = dynamic_cast<juce::Slider*>(child);
            slider != nullptr && slider->getSliderStyle() == juce::Slider::LinearHorizontal
            && slider->getWidth() - slider->getTextBoxWidth() < 48)
            problems.add(describe(*slider) + " has a short track");
        if (!floatsAbove(*child)) {
            for (auto* other : placed)
                if (other->getBounds().intersects(child->getBounds()))
                    problems.add(describe(*child) + " overlaps " + describe(*other));
            placed.add(child);
        }
        if (auto* viewport = dynamic_cast<juce::Viewport*>(child)) {
            if (auto* viewed = viewport->getViewedComponent())
                checkLayout(*viewed, problems);
            continue;
        }
        if (!ownsItsParts(*child))
            checkLayout(*child, problems);
    }
}

} // namespace

TEST_CASE("Editor controls stay apart and buttons stay large enough", "[editor][layout]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto [width, height, songMode] = GENERATE(table<int, int, bool>({
        {1200, 792, false}, {1120, 680, true}, {1920, 1200, true} }));
    OpenFruitAudioProcessor processor;
    processor.getAPVTS().getParameter(openfruit::params::kSongMode)->setValueNotifyingHost(songMode ? 1.0f : 0.0f);
    auto editor = openEditor(processor);
    editor->setSize(width, height);
    editor->refresh();

    juce::StringArray problems;
    checkLayout(*editor, problems);
    CHECK(problems.joinIntoString("\n").toStdString() == "");
}

TEST_CASE("Previous and next step through the kit list and wrap", "[editor][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openfruit-step-kits");
    directory.deleteRecursively();
    OpenFruitAudioProcessor processor;
    processor.getPresets().setDirectory(directory);
    auto editor = openEditor(processor);
    const auto kits = processor.getPresets().list();
    REQUIRE(kits[1].name == processor.getPresetName());

    click(*findButton(*editor, ">"));
    CHECK(processor.getPresetName() == kits[2].name);
    click(*findButton(*editor, "<"));
    click(*findButton(*editor, "<"));
    click(*findButton(*editor, "<"));
    CHECK(processor.getPresetName() == kits.back().name);
    auto* previous = findButton(*editor, "<");
    CHECK(previous->getWidth() >= 40);
    CHECK(previous->getHeight() >= 40);
    CHECK(previous->getTooltip() == "Previous preset");
}

TEST_CASE("A click selects a kit and a double-click loads it", "[editor][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openfruit-click-kits");
    directory.deleteRecursively();
    OpenFruitAudioProcessor processor;
    processor.getPresets().setDirectory(directory);
    auto editor = openEditor(processor);
    click(*findButton(*editor, "Presets"));
    auto* list = findVisible<juce::ListBox>(*editor);
    REQUIRE(list != nullptr);
    auto* firstRow = list->getComponentAt(list->getRowPosition(0, true).getCentre());
    REQUIRE(firstRow != nullptr);

    click(*firstRow);
    CHECK(list->getSelectedRow() == 0);
    CHECK(processor.getPresetName() == "Citrus");
    click(*firstRow, 2);
    CHECK(processor.getPresetName() == processor.getPresets().list().front().name);
}

TEST_CASE("Replacing or deleting a saved kit takes a second click", "[editor][presets]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openfruit-confirm-kits");
    directory.deleteRecursively();
    OpenFruitAudioProcessor processor;
    processor.getPresets().setDirectory(directory);
    auto editor = openEditor(processor);
    click(*findButton(*editor, "Presets"));
    auto* list = findVisible<juce::ListBox>(*editor);
    auto* browser = findVisible<openfruit::suite::PresetBrowser>(*editor);
    REQUIRE(browser != nullptr);
    auto* name = dynamic_cast<juce::TextEditor*>(browser->getChildComponent(1));
    REQUIRE(name != nullptr);
    name->setText("Room Kit");

    click(*findButton(*editor, "Save"));
    const auto file = directory.getChildFile("Room Kit.preset");
    REQUIRE(file.existsAsFile());
    click(*findButton(*editor, "Save"));
    REQUIRE(findButton(*editor, "Replace?") != nullptr);
    click(*findButton(*editor, "Replace?"));
    CHECK(findButton(*editor, "Save") != nullptr);

    const int lastRow = list->getListBoxModel()->getNumRows() - 1;
    click(*list->getComponentAt(list->getRowPosition(lastRow, true).getCentre()));
    click(*findButton(*editor, "Delete"));
    CHECK(file.existsAsFile());
    click(*findButton(*editor, "Delete?"));
    CHECK_FALSE(file.existsAsFile());
    directory.deleteRecursively();
}

TEST_CASE("The wheel scrolls the grid unless Alt or Cmd is held", "[editor][wheel]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto* tracks = processor.getAPVTS().getParameter(openfruit::params::kTrackCount);
    tracks->setValueNotifyingHost(tracks->convertTo0to1(3.0f));
    auto editor = openEditor(processor);
    auto* viewport = findVisible<juce::Viewport>(*editor);
    auto* pad = findVisible<openfruit::ui::CyberStepPad>(*editor);
    REQUIRE(pad != nullptr);
    const auto before = processor.getEngine().getStep(0, openfruit::core::PageIndex::PageA, 0);
    REQUIRE(before.active);

    pad->mouseWheelMove(mouseAt(*pad, {}), { 0.0f, -0.5f, false, false, false });
    CHECK(viewport->getViewPositionY() > 0);
    CHECK(processor.getEngine().getStep(0, openfruit::core::PageIndex::PageA, 0) == before);

    pad->mouseWheelMove(mouseAt(*pad, juce::ModifierKeys::altModifier), { 0.5f, 0.0f, false, false, false });
    CHECK(processor.getEngine().getStep(0, openfruit::core::PageIndex::PageA, 0) == before);
    pad->mouseWheelMove(mouseAt(*pad, juce::ModifierKeys::altModifier), { 0.0f, 0.5f, false, false, false });
    CHECK(processor.getEngine().getStep(0, openfruit::core::PageIndex::PageA, 0).semitoneOffset == 1);
}

TEST_CASE("The editor reopens at its saved size", "[editor][session]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    openEditor(processor)->setSize(1400, 900);
    juce::MemoryBlock session;
    processor.getStateInformation(session);

    OpenFruitAudioProcessor restored;
    restored.setStateInformation(session.getData(), static_cast<int>(session.getSize()));
    auto editor = openEditor(restored);
    CHECK(editor->getWidth() == 1400);
    CHECK(editor->getHeight() == 900);
}

TEST_CASE("Envelope sliders give short times more travel", "[editor][layout]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    juce::Slider attack;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment(processor.getAPVTS(), openfruit::params::trackAttackId(0), attack);
    openfruit::ui::TrackVoiceSettingsPanel panel(0, processor.getAPVTS());
    const auto children = panel.getChildren();
    const auto found = std::find_if(children.begin(), children.end(), [](juce::Component* child) {
        auto* slider = dynamic_cast<juce::Slider*>(child);
        return slider != nullptr && slider->getTooltip().startsWith("Attack");
    });
    REQUIRE(found != children.end());

    CHECK(dynamic_cast<juce::Slider*>(*found)->valueToProportionOfLength(0.2) > 0.3);
    CHECK(attack.valueToProportionOfLength(0.2) < 0.05);
}

TEST_CASE("Dragging across a row paints pads as one undo step", "[editor][paint]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto editor = openEditor(processor);
    auto* pad = findVisible<openfruit::ui::CyberStepPad>(*editor, "Track 7 step 1");
    REQUIRE(pad != nullptr);
    const auto down = mouseAt(*pad, juce::ModifierKeys::leftButtonModifier);
    const auto across = down.withNewPosition(juce::Point<float>(static_cast<float>(pad->getWidth()) * 3.5f, down.position.y));

    pad->mouseDown(down);
    pad->mouseDrag(across);
    pad->mouseUp(across);
    const auto& engine = processor.getEngine();
    CHECK(engine.getStep(6, openfruit::core::PageIndex::PageA, 0).active);
    CHECK(engine.getStep(6, openfruit::core::PageIndex::PageA, 3).active);
    CHECK_FALSE(engine.getStep(6, openfruit::core::PageIndex::PageA, 4).active);

    processor.getUndo().undo();
    CHECK_FALSE(engine.getStep(6, openfruit::core::PageIndex::PageA, 0).active);
    CHECK_FALSE(engine.getStep(6, openfruit::core::PageIndex::PageA, 3).active);
}

TEST_CASE("Each pad describes and draws its own step settings", "[editor][pads]") {
    juce::ScopedJuceInitialiser_GUI gui;
    openfruit::ui::CyberStepPad pad(0, 4);
    pad.setSize(48, 60);
    openfruit::core::StepData full;
    full.active = true;
    pad.setStepData(full);
    const auto fullImage = pad.createComponentSnapshot(pad.getLocalBounds());

    openfruit::core::StepData never = full;
    never.probability = 0.0f;
    never.ratchets = 3;
    never.reverse = true;
    pad.setStepData(never);
    const auto neverImage = pad.createComponentSnapshot(pad.getLocalBounds());

    CHECK(pad.getTooltip().startsWith("Step 5: velocity 85%, 0% chance, 3 ratchets, reversed."));
    CHECK(neverImage.getPixelAt(10, 30).getAlpha() < fullImage.getPixelAt(10, 30).getAlpha());
}

TEST_CASE("Muted and solo-silenced rows dim their pads", "[editor][mute]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto editor = openEditor(processor);
    auto& state = processor.getAPVTS();
    const auto padAlpha = [&](int track) {
        editor->refresh();
        return findVisible<openfruit::ui::CyberStepPad>(*editor, "Track " + juce::String(track) + " step 1")->getAlpha();
    };
    REQUIRE(padAlpha(1) == 1.0f);

    state.getParameter(openfruit::params::trackMuteId(0))->setValueNotifyingHost(1.0f);
    CHECK(padAlpha(1) < 0.5f);
    state.getParameter(openfruit::params::trackMuteId(0))->setValueNotifyingHost(0.0f);
    state.getParameter(openfruit::params::trackSoloId(1))->setValueNotifyingHost(1.0f);
    CHECK(padAlpha(1) < 0.5f);
    CHECK(padAlpha(2) == 1.0f);
}

TEST_CASE("Follow shows the playing page and the playhead dims elsewhere", "[editor][song]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto& state = processor.getAPVTS();
    state.getParameter(openfruit::params::kSongMode)->setValueNotifyingHost(1.0f);
    auto* firstPage = state.getParameter(openfruit::params::songSectionPageId(0));
    firstPage->setValueNotifyingHost(firstPage->convertTo0to1(2.0f));
    auto& engine = processor.getEngine();
    engine.prepare(48000.0, 480);
    engine.setPlaying(true);
    std::vector<float> left(480), right(480);
    engine.process(left.data(), right.data(), 480, 120.0, 0.5, true);
    auto editor = openEditor(processor);
    auto* header = findVisible<openfruit::ui::HeaderPanel>(*editor);
    REQUIRE(header != nullptr);
    CHECK(header->shownPage() == openfruit::core::PageIndex::PageA);
    CHECK_FALSE(header->playheadLive());

    processor.getSessionState().setProperty("followSong", true, nullptr);
    editor->refresh();
    CHECK(header->shownPage() == openfruit::core::PageIndex::PageC);
    CHECK(header->playheadLive());
}

TEST_CASE("Mix view shows level and pan for every lane", "[editor][mix]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto editor = openEditor(processor);
    click(*findButton(*editor, "Mix"));

    CHECK(findVisible<openfruit::ui::CyberStepPad>(*editor) == nullptr);
    auto* level = findVisible<juce::Slider>(*findVisible<openfruit::ui::TrackRowComponent>(*editor));
    REQUIRE(level != nullptr);
    CHECK(level->getTooltip() == "Level for track 1");
    CHECK(static_cast<bool>(processor.getSessionState().getProperty("mixView")));
    juce::StringArray problems;
    checkLayout(*editor, problems);
    CHECK(problems.joinIntoString("\n").toStdString() == "");
}

TEST_CASE("Voice settings step between tracks and group the filters", "[editor][settings]") {
    juce::ScopedJuceInitialiser_GUI gui;
    OpenFruitAudioProcessor processor;
    auto editor = openEditor(processor);
    click(*findButton(*editor, "Edit"));
    click(*findButton(*findVisible<openfruit::ui::TrackVoiceSettingsPanel>(*editor), ">"));

    auto* panel = findVisible<openfruit::ui::TrackVoiceSettingsPanel>(*editor);
    REQUIRE(panel != nullptr);
    const auto children = panel->getChildren();
    const auto sliderWithTooltip = [&children](const juce::String& tooltip) -> juce::Component* {
        const auto found = std::find_if(children.begin(), children.end(), [&tooltip](juce::Component* child) {
            auto* slider = dynamic_cast<juce::Slider*>(child);
            return slider != nullptr && slider->getTooltip() == tooltip;
        });
        return found == children.end() ? nullptr : *found;
    };
    REQUIRE(sliderWithTooltip("High-pass for track 2") != nullptr);
    CHECK(sliderWithTooltip("Pitch for track 2") != nullptr);
    CHECK(sliderWithTooltip("High-pass for track 2")->getX() == sliderWithTooltip("Low-pass for track 2")->getX());
    juce::StringArray problems;
    checkLayout(*editor, problems);
    CHECK(problems.joinIntoString("\n").toStdString() == "");
}

namespace {

void writeTestWav(const juce::File& file, double durationSeconds) {
    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    REQUIRE(out != nullptr);
    constexpr uint32_t sampleRate = 44100;
    const uint32_t numSamples = static_cast<uint32_t>(sampleRate * durationSeconds);
    const uint32_t dataBytes = numSamples * 2;
    out->write("RIFF", 4);
    out->writeInt(static_cast<int>(36 + dataBytes));
    out->write("WAVEfmt ", 8);
    out->writeInt(16);
    out->writeShort(1);
    out->writeShort(1);
    out->writeInt(static_cast<int>(sampleRate));
    out->writeInt(static_cast<int>(sampleRate * 2));
    out->writeShort(2);
    out->writeShort(16);
    out->write("data", 4);
    out->writeInt(static_cast<int>(dataBytes));
    for (uint32_t i = 0; i < numSamples; ++i) {
        out->writeShort(0);
    }
    out->flush();
}

} // namespace

TEST_CASE("Sample browser filters, remembers its folder, sorts unanalysed last, and handles Enter/Esc", "[editor][browser]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openfruit-browser-test");
    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());
    const auto kickWav = dir.getChildFile("kick_short.wav");
    const auto snareWav = dir.getChildFile("snare_unanalysed.wav");
    writeTestWav(kickWav, 0.1);
    writeTestWav(snareWav, 0.3);

    OpenFruitAudioProcessor processor;
    // Pre-cache only kickWav so snareWav is unanalysed when the browser opens.
    int64_t modTime = 0;
    int64_t fileSize = 0;
    REQUIRE(openfruit::core::ToneCache::getFileStats(kickWav.getFullPathName().toStdString(), modTime, fileSize));
    openfruit::core::ToneAnalysisResult cachedTone;
    cachedTone.durationSeconds = 0.1;
    cachedTone.hasClearTone = true;
    cachedTone.midiNote = 36;
    cachedTone.noteName = "C2";
    processor.getToneCache().put(kickWav.getFullPathName().toStdString(), modTime, fileSize, cachedTone);

    {
        auto editor = openEditor(processor);
        editor->openSampleBrowser(0, dir);
        auto* browser = findVisible<openfruit::ui::SampleBrowserModal>(*editor);
        REQUIRE(browser != nullptr);

        juce::StringArray problems;
        checkLayout(*editor, problems);
        CHECK(problems.joinIntoString("\n").toStdString() == "");

        auto* table = findVisible<juce::TableListBox>(*browser);
        REQUIRE(table != nullptr);
        REQUIRE(browser->getNumRows() == 2);

        // Unanalysed snareWav must sort after analysed kickWav in default shortest-first mode.
        table->selectRow(0);
        auto* search = findVisible<juce::TextEditor>(*browser);
        REQUIRE(search != nullptr);
        search->setText("snare", false);
        if (search->onTextChange) search->onTextChange();
        CHECK(browser->getNumRows() == 1);

        // Esc closes without replacing track 0's sample.
        const auto originalSample = processor.getEngine().loadedSample(0);
        CHECK(browser->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)));
        CHECK(findVisible<openfruit::ui::SampleBrowserModal>(*editor) == nullptr);
        CHECK(processor.getEngine().loadedSample(0) == originalSample);
    }

    // Reopen editor and verify remembered folder persists in session state.
    {
        auto editor = openEditor(processor);
        editor->openSampleBrowser(0);
        auto* browser = findVisible<openfruit::ui::SampleBrowserModal>(*editor);
        REQUIRE(browser != nullptr);
        CHECK(browser->getNumRows() == 2);

        auto* table = findVisible<juce::TableListBox>(*browser);
        REQUIRE(table != nullptr);
        table->selectRow(0);
        CHECK(browser->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)));
        CHECK(findVisible<openfruit::ui::SampleBrowserModal>(*editor) == nullptr);
        const auto loaded = processor.getEngine().loadedSample(0);
        REQUIRE(loaded != nullptr);
        CHECK(loaded->sampleName == "kick_short");
    }

    dir.deleteRecursively();
}

TEST_CASE("Track lanes accept dropped audio files", "[editor][dnd]") {
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openfruit-dnd-test");
    dir.deleteRecursively();
    REQUIRE(dir.createDirectory());
    const auto wav = dir.getChildFile("dropped_hat.wav");
    writeTestWav(wav, 0.15);

    OpenFruitAudioProcessor processor;
    auto editor = openEditor(processor);
    auto* row = findVisible<openfruit::ui::TrackRowComponent>(*editor);
    REQUIRE(row != nullptr);

    CHECK_FALSE(row->isInterestedInFileDrag(juce::StringArray{ dir.getChildFile("notes.txt").getFullPathName() }));
    CHECK(row->isInterestedInFileDrag(juce::StringArray{ wav.getFullPathName() }));
    row->filesDropped(juce::StringArray{ wav.getFullPathName() }, 10, 10);

    const auto loaded = processor.getEngine().loadedSample(row->getTrackIndex());
    REQUIRE(loaded != nullptr);
    CHECK(loaded->sampleName == "dropped_hat");
    dir.deleteRecursively();
}
