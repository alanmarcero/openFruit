#include "SampleState.h"
#include "SonicAnalyzer.h"
#include "WavReader.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <memory>

namespace openfruit::plugin {
namespace {

juce::String contentKey(const core::AudioSampleBuffer& sample) {
    uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](const void* data, size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
    };
    mix(&sample.sampleRate, sizeof(sample.sampleRate));
    mix(&sample.numChannels, sizeof(sample.numChannels));
    mix(&sample.numSamples, sizeof(sample.numSamples));
    for (const auto& channel : sample.channels)
        if (!channel.empty())
            mix(channel.data(), channel.size() * sizeof(float));
    return juce::String::toHexString(static_cast<juce::int64>(hash));
}

juce::MemoryBlock encodeFlac(const core::AudioSampleBuffer& sample) {
    if (sample.numChannels <= 0 || sample.numSamples <= 0 || sample.channels.empty())
        return {};
    const int channels = std::min(sample.numChannels, static_cast<int>(sample.channels.size()));
    const int frames = static_cast<int>(std::min<int64_t>(sample.numSamples, 0x7fffffff));
    if (channels <= 0 || frames <= 0)
        return {};

    juce::AudioBuffer<float> buffer(channels, frames);
    for (int channel = 0; channel < channels; ++channel) {
        const auto& source = sample.channels[static_cast<size_t>(channel)];
        const int count = std::min(frames, static_cast<int>(source.size()));
        if (count > 0)
            buffer.copyFrom(channel, 0, source.data(), count);
    }

    juce::MemoryBlock encoded;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream>(encoded, false);
    juce::FlacAudioFormat flac;
    juce::AudioFormat& format = flac;
    const double rate = sample.sampleRate > 0.0 ? sample.sampleRate : 44100.0;
    const auto options = juce::AudioFormatWriterOptions{}
        .withSampleRate(rate)
        .withNumChannels(channels)
        .withBitsPerSample(24);
    auto writer = format.createWriterFor(stream, options);
    if (writer == nullptr || !writer->writeFromAudioSampleBuffer(buffer, 0, frames))
        return {};
    writer.reset();
    return encoded;
}

std::shared_ptr<core::AudioSampleBuffer> decodeFlac(const juce::MemoryBlock& encoded,
                                                    const std::string& path,
                                                    const std::string& name) {
    if (encoded.getSize() == 0)
        return {};
    juce::FlacAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(
        new juce::MemoryInputStream(encoded.getData(), encoded.getSize(), false), true));
    if (reader == nullptr || reader->numChannels <= 0 || reader->lengthInSamples <= 0)
        return {};

    const int channels = static_cast<int>(reader->numChannels);
    const int frames = static_cast<int>(std::min<juce::int64>(reader->lengthInSamples, 0x7fffffff));
    juce::AudioBuffer<float> buffer(channels, frames);
    if (!reader->read(&buffer, 0, frames, 0, true, true))
        return {};

    auto sample = std::make_shared<core::AudioSampleBuffer>();
    sample->filePath = path;
    sample->sampleName = !name.empty() ? name : juce::File(juce::String(path)).getFileNameWithoutExtension().toStdString();
    sample->numChannels = channels;
    sample->sampleRate = reader->sampleRate;
    sample->numSamples = frames;
    sample->channels.resize(static_cast<size_t>(channels));
    for (int channel = 0; channel < channels; ++channel)
        sample->channels[static_cast<size_t>(channel)].assign(buffer.getReadPointer(channel),
                                                              buffer.getReadPointer(channel) + frames);
    if (!sample->channels[0].empty()) {
        sample->toneResult = core::SonicAnalyzer::analyze(sample->channels[0].data(),
                                                          sample->numSamples,
                                                          sample->sampleRate);
    }
    sample->isLoaded = true;
    return sample;
}

bool shouldEmbed(const core::AudioSampleBuffer& sample) {
    return sample.isLoaded
        && sample.numSamples > 0
        && !sample.channels.empty()
        && juce::File::isAbsolutePath(sample.filePath);
}

} // namespace

juce::ValueTree makeSampleFiles(const core::FruitEngine& engine) {
    juce::ValueTree files { "SampleFiles" };
    std::map<juce::String, int> blobForKey;
    int nextBlob = 0;

    for (int track = 0; track < core::kMaxTracks; ++track) {
        const auto sample = engine.loadedSample(track);
        if (sample == nullptr || !juce::File::isAbsolutePath(sample->filePath))
            continue;
        files.setProperty("track" + juce::String(track), juce::String(sample->filePath), nullptr);
        if (!shouldEmbed(*sample))
            continue;

        const auto key = contentKey(*sample);
        auto found = blobForKey.find(key);
        if (found == blobForKey.end()) {
            const auto encoded = encodeFlac(*sample);
            if (encoded.getSize() == 0)
                continue;
            const int blobId = nextBlob++;
            juce::ValueTree blob { "Blob" };
            blob.setProperty("id", blobId, nullptr);
            blob.setProperty("codec", "flac", nullptr);
            blob.setProperty("data", juce::Base64::toBase64(encoded.getData(), encoded.getSize()), nullptr);
            files.addChild(blob, -1, nullptr);
            found = blobForKey.emplace(key, blobId).first;
        }

        juce::ValueTree use { "Use" };
        use.setProperty("track", track, nullptr);
        use.setProperty("blob", found->second, nullptr);
        use.setProperty("name", juce::String(sample->sampleName), nullptr);
        files.addChild(use, -1, nullptr);
    }
    return files;
}

void restoreSampleFiles(const juce::ValueTree& files, core::FruitEngine& engine) {
    if (!files.isValid())
        return;

    std::map<int, juce::MemoryBlock> blobs;
    for (int i = 0; i < files.getNumChildren(); ++i) {
        const auto child = files.getChild(i);
        if (!child.hasType("Blob"))
            continue;
        juce::MemoryBlock encoded;
        const auto text = child.getProperty("data").toString();
        if (text.isNotEmpty()) {
            juce::MemoryOutputStream stream(encoded, false);
            juce::Base64::convertFromBase64(stream, text);
        }
        blobs[static_cast<int>(child.getProperty("id", -1))] = std::move(encoded);
    }

    bool embedded[core::kMaxTracks] {};
    for (int i = 0; i < files.getNumChildren(); ++i) {
        const auto child = files.getChild(i);
        if (!child.hasType("Use"))
            continue;
        const int track = child.getProperty("track", -1);
        const int blobId = child.getProperty("blob", -1);
        if (track < 0 || track >= core::kMaxTracks)
            continue;
        const auto blob = blobs.find(blobId);
        if (blob == blobs.end())
            continue;
        const auto path = files.getProperty("track" + juce::String(track)).toString().toStdString();
        const auto name = child.getProperty("name").toString().toStdString();
        if (auto sample = decodeFlac(blob->second, path, name)) {
            engine.loadSampleForTrack(track, std::move(sample));
            embedded[track] = true;
        }
    }

    for (int track = 0; track < core::kMaxTracks; ++track) {
        if (embedded[track])
            continue;
        const auto path = files.getProperty("track" + juce::String(track)).toString();
        if (!juce::File::isAbsolutePath(path))
            continue;
        auto sample = core::WavReader::loadSample(path.toStdString());
        if (!sample) {
            sample = std::make_shared<core::AudioSampleBuffer>();
            sample->filePath = path.toStdString();
            sample->sampleName = "Missing: " + juce::File(path).getFileNameWithoutExtension().toStdString();
        }
        engine.loadSampleForTrack(track, std::move(sample));
    }
}

} // namespace openfruit::plugin
