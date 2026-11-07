#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI guiInit;

    OpenFruitAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor) {
        std::cerr << "Failed to create editor\n";
        return 1;
    }

    editor->setSize(1120, 720);

    // Render snapshot of the actual native plugin component
    juce::Image snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), false, 2.0f);

    juce::File outputFile = (argc > 1) ? juce::File(argv[1])
                                       : juce::File::getCurrentWorkingDirectory().getChildFile("assets/openFruit-screenshot.png");
    outputFile.getParentDirectory().createDirectory();
    outputFile.deleteFile();

    juce::FileOutputStream stream(outputFile);
    if (!stream.openedOk()) {
        std::cerr << "Failed to open output file: " << outputFile.getFullPathName() << "\n";
        return 1;
    }

    juce::PNGImageFormat png;
    if (png.writeImageToStream(snapshot, stream)) {
        std::cout << "Successfully saved plugin screenshot to " << outputFile.getFullPathName() << " ("
                  << snapshot.getWidth() << "x" << snapshot.getHeight() << ")\n";
        return 0;
    }

    std::cerr << "Failed to write PNG to stream\n";
    return 1;
}
