#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <utility>
#include <vector>

namespace openfruit::suite {

struct PresetEntry {
    juce::String name;
    bool factory = false;
    bool favorite = false;
    juce::File file;
};

// Factory presets stay in memory. User presets live in
// ~/Documents/Alan Marcero/<plugin>/Presets.
class PresetLibrary {
public:
    explicit PresetLibrary(juce::String pluginName);
    PresetLibrary(juce::File directory, juce::String pluginName);

    void setFactory(std::vector<std::pair<juce::String, juce::MemoryBlock>> factory);
    void setDirectory(juce::File directory);

    [[nodiscard]] const juce::File& directory() const noexcept { return directory_; }
    [[nodiscard]] std::vector<PresetEntry> list(const juce::String& search = {}) const;
    [[nodiscard]] bool exists(const juce::String& name) const;

    bool save(const juce::String& name, const juce::MemoryBlock& state, juce::String& error);
    bool rename(const PresetEntry& preset, const juce::String& newName, juce::String& error);
    bool remove(const PresetEntry& preset, juce::String& error);
    bool setFavorite(const PresetEntry& preset, bool favorite);
    bool load(const PresetEntry& preset, juce::MemoryBlock& state, juce::String& error) const;

    [[nodiscard]] static juce::File defaultDirectory(const juce::String& pluginName);
    [[nodiscard]] static juce::String sanitiseName(const juce::String& name);

private:
    [[nodiscard]] bool isFactoryName(const juce::String& name) const;
    [[nodiscard]] bool isFavorite(bool factory, const juce::String& name) const;
    void writeFavorite(bool factory, const juce::String& name, bool favorite) const;
    [[nodiscard]] juce::File fileFor(const juce::String& name) const;

    juce::String pluginName_;
    juce::File directory_;
    std::vector<std::pair<juce::String, juce::MemoryBlock>> factory_;
};

}
