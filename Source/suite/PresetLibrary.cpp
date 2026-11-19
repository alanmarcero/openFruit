#include "PresetLibrary.h"
#include <algorithm>

namespace openfruit::suite {
namespace {

constexpr const char* kExtension = ".preset";

juce::String favoriteKey(bool factory, const juce::String& name) {
    return (factory ? "f\t" : "u\t") + name;
}

} // namespace

PresetLibrary::PresetLibrary(juce::String pluginName)
    : PresetLibrary(defaultDirectory(pluginName), std::move(pluginName)) {}

PresetLibrary::PresetLibrary(juce::File directory, juce::String pluginName)
    : pluginName_(std::move(pluginName)), directory_(std::move(directory)) {}

juce::File PresetLibrary::defaultDirectory(const juce::String& pluginName) {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Alan Marcero")
        .getChildFile(pluginName)
        .getChildFile("Presets");
}

void PresetLibrary::setFactory(std::vector<std::pair<juce::String, juce::MemoryBlock>> factory) {
    factory_ = std::move(factory);
}

void PresetLibrary::setDirectory(juce::File directory) {
    directory_ = std::move(directory);
}

juce::String PresetLibrary::sanitiseName(const juce::String& name) {
    auto cleaned = name.trim();
    if (cleaned.length() > 80)
        cleaned = cleaned.substring(0, 80).trim();
    const juce::String forbidden { "/\\:*?\"<>|" };
    juce::String result;
    for (int index = 0; index < cleaned.length(); ++index) {
        const auto character = cleaned[index];
        if (character >= 32 && !forbidden.containsChar(character))
            result += character;
    }
    return result.trim();
}

bool PresetLibrary::isFactoryName(const juce::String& name) const {
    for (const auto& preset : factory_)
        if (preset.first.equalsIgnoreCase(name))
            return true;
    return false;
}

juce::File PresetLibrary::fileFor(const juce::String& name) const {
    return directory_.getChildFile(name + kExtension);
}

bool PresetLibrary::isFavorite(bool factory, const juce::String& name) const {
    const auto file = directory_.getChildFile("favorites.txt");
    if (!file.existsAsFile())
        return false;
    const auto key = favoriteKey(factory, name);
    for (const auto& line : juce::StringArray::fromLines(file.loadFileAsString()))
        if (line.trim() == key)
            return true;
    return false;
}

void PresetLibrary::writeFavorite(bool factory, const juce::String& name, bool favorite) const {
    directory_.createDirectory();
    const auto file = directory_.getChildFile("favorites.txt");
    juce::StringArray lines;
    if (file.existsAsFile())
        lines.addLines(file.loadFileAsString());
    const auto key = favoriteKey(factory, name);
    for (int i = lines.size(); --i >= 0;)
        if (lines[i].trim() == key || lines[i].trim().isEmpty())
            lines.remove(i);
    if (favorite)
        lines.add(key);
    file.replaceWithText(lines.joinIntoString("\n"));
}

std::vector<PresetEntry> PresetLibrary::list(const juce::String& search) const {
    std::vector<PresetEntry> entries;
    const auto matches = [&](const juce::String& name) {
        return search.trim().isEmpty() || name.containsIgnoreCase(search.trim());
    };

    for (const auto& preset : factory_) {
        if (!matches(preset.first))
            continue;
        PresetEntry entry;
        entry.name = preset.first;
        entry.factory = true;
        entry.favorite = isFavorite(true, preset.first);
        entries.push_back(entry);
    }

    if (directory_.isDirectory()) {
        for (const auto& file : directory_.findChildFiles(juce::File::findFiles, false, "*" + juce::String(kExtension))) {
            const auto name = file.getFileNameWithoutExtension();
            if (name.isEmpty() || !matches(name))
                continue;
            PresetEntry entry;
            entry.name = name;
            entry.factory = false;
            entry.favorite = isFavorite(false, name);
            entry.file = file;
            entries.push_back(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const PresetEntry& left, const PresetEntry& right) {
        if (left.favorite != right.favorite)
            return left.favorite;
        if (left.factory != right.factory)
            return left.factory;
        return left.name.compareIgnoreCase(right.name) < 0;
    });
    return entries;
}

bool PresetLibrary::save(const juce::String& name, const juce::MemoryBlock& state, juce::String& error) {
    const auto cleaned = sanitiseName(name);
    if (cleaned.isEmpty()) {
        error = "Enter a preset name.";
        return false;
    }
    if (isFactoryName(cleaned)) {
        error = "Factory presets stay read-only.";
        return false;
    }
    if (state.getSize() == 0) {
        error = "Nothing to save.";
        return false;
    }
    directory_.createDirectory();
    const auto file = fileFor(cleaned);
    if (!file.replaceWithData(state.getData(), state.getSize())) {
        error = "Could not write the preset.";
        return false;
    }
    error.clear();
    return true;
}

bool PresetLibrary::rename(const PresetEntry& preset, const juce::String& newName, juce::String& error) {
    if (preset.factory) {
        error = "Factory presets stay read-only.";
        return false;
    }
    const auto cleaned = sanitiseName(newName);
    if (cleaned.isEmpty()) {
        error = "Enter a preset name.";
        return false;
    }
    if (isFactoryName(cleaned)) {
        error = "Factory presets stay read-only.";
        return false;
    }
    const auto destination = fileFor(cleaned);
    if (destination != preset.file && destination.existsAsFile()) {
        error = "A preset with that name already exists.";
        return false;
    }
    const bool favorite = preset.favorite;
    if (!preset.file.moveFileTo(destination)) {
        error = "Could not rename the preset.";
        return false;
    }
    if (favorite) {
        writeFavorite(false, preset.name, false);
        writeFavorite(false, cleaned, true);
    }
    error.clear();
    return true;
}

bool PresetLibrary::remove(const PresetEntry& preset, juce::String& error) {
    if (preset.factory) {
        error = "Factory presets stay read-only.";
        return false;
    }
    if (!preset.file.deleteFile()) {
        error = "Could not delete the preset.";
        return false;
    }
    writeFavorite(false, preset.name, false);
    error.clear();
    return true;
}

bool PresetLibrary::setFavorite(const PresetEntry& preset, bool favorite) {
    writeFavorite(preset.factory, preset.name, favorite);
    return true;
}

bool PresetLibrary::load(const PresetEntry& preset, juce::MemoryBlock& state, juce::String& error) const {
    if (preset.factory) {
        for (const auto& factory : factory_) {
            if (factory.first == preset.name) {
                state = factory.second;
                error.clear();
                return state.getSize() > 0;
            }
        }
        error = "Factory preset is missing.";
        return false;
    }
    if (!preset.file.existsAsFile() || !preset.file.loadFileAsData(state) || state.getSize() == 0) {
        error = "Could not read the preset.";
        return false;
    }
    error.clear();
    return true;
}

}
