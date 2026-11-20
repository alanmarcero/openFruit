#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SampleVoice.h"
#include "WavReader.h"
#include <array>
#include <cmath>
#include <vector>

using namespace openfruit::core;

TEST_CASE("SampleVoice playback and pitch tuning", "[SampleVoice]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 1000;
    sample->channels.resize(1);
    sample->channels[0].assign(1000, 0.5f); // DC offset 0.5
    sample->isLoaded = true;

    SampleVoice voice;
    REQUIRE_FALSE(voice.isPlaying());

    // Start with 0 semitone offset
    voice.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f);
    REQUIRE(voice.isPlaying());

    std::vector<float> left(500, 0.0f);
    std::vector<float> right(500, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 500);

    REQUIRE(voice.isPlaying());
    // Equal power center pan has left == right > 0
    REQUIRE(left[0] > 0.0f);
    REQUIRE(right[0] > 0.0f);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(right[0], 0.01f));

    // Render remaining 500 samples
    voice.renderNextBlock(left.data(), right.data(), 500);
    // At end of sample, isPlaying becomes false
    voice.renderNextBlock(left.data(), right.data(), 10);
    REQUIRE_FALSE(voice.isPlaying());
}

TEST_CASE("SampleVoice plays a trimmed region in either direction", "[SampleVoice][regions]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 44100.0;
    sample->numChannels = 1;
    sample->numSamples = 8;
    sample->channels = {{0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f}};
    sample->isLoaded = true;

    VoiceSettings settings;
    settings.sampleStart = 0.25f;
    settings.sampleEnd = 0.75f;
    SampleVoice forward;
    forward.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f, settings);
    std::vector<float> left(4, 0.0f), right(4, 0.0f);
    forward.renderNextBlock(left.data(), right.data(), 4);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(0.2f * 0.7071068f, 0.001f));
    REQUIRE_THAT(left[3], Catch::Matchers::WithinAbs(0.5f * 0.7071068f, 0.001f));
    REQUIRE_FALSE(forward.isPlaying());

    settings.reverse = true;
    SampleVoice reversed;
    reversed.start(sample, 1.0f, 0, 0, 44100.0, 1.0f, 0.0f, settings);
    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    reversed.renderNextBlock(left.data(), right.data(), 4);
    REQUIRE_THAT(left[0], Catch::Matchers::WithinAbs(0.5f * 0.7071068f, 0.001f));
    REQUIRE_THAT(left[3], Catch::Matchers::WithinAbs(0.2f * 0.7071068f, 0.001f));
    REQUIRE_FALSE(reversed.isPlaying());
}

TEST_CASE("SampleVoice trim boundaries do not interpolate adjacent audio", "[SampleVoice][regions]") {
    auto makeSample = [](float outside) {
        auto sample = std::make_shared<AudioSampleBuffer>();
        sample->sampleRate = 8000.0;
        sample->numChannels = 1;
        sample->numSamples = 10;
        sample->channels = {{outside, outside, outside, outside, 0.4f, 0.4f, 0.4f, 0.4f, outside, outside}};
        sample->isLoaded = true;
        return sample;
    };

    VoiceSettings settings;
    settings.sampleStart = 0.4f;
    settings.sampleEnd = 0.8f;
    SampleVoice quietOutside, loudOutside;
    quietOutside.start(makeSample(0.0f), 1.0f, 0, 0, 16000.0, 1.0f, 0.0f, settings);
    loudOutside.start(makeSample(0.9f), 1.0f, 0, 0, 16000.0, 1.0f, 0.0f, settings);
    std::vector<float> quietLeft(2, 0.0f), quietRight(2, 0.0f);
    std::vector<float> loudLeft(2, 0.0f), loudRight(2, 0.0f);
    quietOutside.renderNextBlock(quietLeft.data(), quietRight.data(), 2);
    loudOutside.renderNextBlock(loudLeft.data(), loudRight.data(), 2);

    REQUIRE_THAT(quietLeft[1], Catch::Matchers::WithinAbs(loudLeft[1], 1.0e-5f));
}

TEST_CASE("SampleVoice applies track amplitude ADSR and low-pass settings", "[SampleVoice][envelope]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 8000.0;
    sample->numChannels = 1;
    sample->numSamples = 256;
    sample->channels = {std::vector<float>(256, 1.0f)};
    sample->isLoaded = true;

    VoiceSettings settings;
    settings.attackSeconds = 0.004f;
    settings.decaySeconds = 0.004f;
    settings.sustainLevel = 0.5f;
    settings.releaseSeconds = 0.01f;
    settings.filterCutoffHz = 2000.0f;
    SampleVoice voice;
    voice.start(sample, 1.0f, 0, 0, 8000.0, 1.0f, 0.0f, settings);
    std::vector<float> left(8, 0.0f), right(8, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 8);
    REQUIRE(left[0] < left[7]);
    REQUIRE(left[7] < 0.5f);

    voice.noteOff();
    std::vector<float> release(16, 0.0f), releaseRight(16, 0.0f);
    voice.renderNextBlock(release.data(), releaseRight.data(), 16);
    REQUIRE(release[0] > release[15]);
    REQUIRE(release[15] < left[7]);
}

TEST_CASE("SampleVoice applies per-track high-pass filter and drive saturation", "[SampleVoice][filter][drive]") {
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = 48000.0;
    sample->numChannels = 1;
    sample->numSamples = 256;
    sample->channels = {std::vector<float>(256, 0.5f)};
    sample->isLoaded = true;

    VoiceSettings hpfSettings;
    hpfSettings.highPassCutoffHz = 1200.0f;
    SampleVoice hpfVoice;
    hpfVoice.start(sample, 1.0f, 0, 0, 48000.0, 1.0f, 0.0f, hpfSettings);
    std::vector<float> hpfLeft(128, 0.0f), hpfRight(128, 0.0f);
    hpfVoice.renderNextBlock(hpfLeft.data(), hpfRight.data(), 128);
    // High-pass filter attenuates DC steady state over time
    REQUIRE(std::abs(hpfLeft[127]) < std::abs(hpfLeft[0]) * 0.1f);

    VoiceSettings cleanSettings;
    SampleVoice cleanVoice;
    cleanVoice.start(sample, 1.0f, 0, 0, 48000.0, 1.0f, 0.0f, cleanSettings);
    std::vector<float> cleanLeft(16, 0.0f), cleanRight(16, 0.0f);
    cleanVoice.renderNextBlock(cleanLeft.data(), cleanRight.data(), 16);

    VoiceSettings driveSettings;
    driveSettings.drive = 1.0f;
    SampleVoice driveVoice;
    driveVoice.start(sample, 1.0f, 0, 0, 48000.0, 1.0f, 0.0f, driveSettings);
    std::vector<float> driveLeft(16, 0.0f), driveRight(16, 0.0f);
    driveVoice.renderNextBlock(driveLeft.data(), driveRight.data(), 16);
    REQUIRE(driveLeft[4] > cleanLeft[4]);
}

TEST_CASE("Fast forward and reverse sampling reject folded source harmonics", "[SampleVoice][resampling]") {
    for (double rate : {32000.0, 44100.0, 48000.0, 96000.0}) {
        for (const bool reverse : {false, true}) {
            auto sample = std::make_shared<AudioSampleBuffer>();
            sample->sampleRate = rate;
            sample->numChannels = 1;
            sample->numSamples = 8192;
            sample->channels = {std::vector<float>(8192)};
            sample->isLoaded = true;
            for (int i = 0; i < 8192; ++i)
                sample->channels.front()[static_cast<size_t>(i)] = .35f + (i % 2 == 0 ? .6f : -.6f);
            VoiceSettings settings;
            settings.reverse = reverse;
            settings.sampleStart = .25f;
            settings.sampleEnd = .75f;
            SampleVoice voice;
            voice.start(sample, 1, 24, 0, rate, 1, -1, settings);
            std::array<float,256> left{}, right{};
            voice.renderNextBlock(left.data(), right.data(), 256);
            for (int i = 16; i < 250; ++i) REQUIRE_THAT(left[static_cast<size_t>(i)], Catch::Matchers::WithinAbs(.35, .001));
        }
    }
}

namespace {

double measureSineBinMagnitude(const float* data, int startFrame, int frameCount, double targetHz, double sampleRate) {
    constexpr double twoPi = 6.28318530717958647692;
    double sumRe = 0.0;
    double sumIm = 0.0;
    for (int i = 0; i < frameCount; ++i) {
        const double window = 0.5 * (1.0 - std::cos(twoPi * static_cast<double>(i) / static_cast<double>(frameCount - 1)));
        const double phase = twoPi * targetHz * static_cast<double>(i) / sampleRate;
        const double sampleValue = static_cast<double>(data[startFrame + i]) * window;
        sumRe += sampleValue * std::cos(phase);
        sumIm -= sampleValue * std::sin(phase);
    }
    return (4.0 / static_cast<double>(frameCount)) * std::hypot(sumRe, sumIm);
}

} // namespace

TEST_CASE("SampleVoice Kaiser polyphase sinc suppresses downward imaging and upward sidelobe aliasing", "[SampleVoice][resampling]") {
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.28318530717958647692;
    constexpr int sourceFrames = 8192;

    auto downSample = std::make_shared<AudioSampleBuffer>();
    downSample->sampleRate = sampleRate;
    downSample->numChannels = 1;
    downSample->numSamples = sourceFrames;
    downSample->channels = {std::vector<float>(sourceFrames, 0.0f)};
    downSample->isLoaded = true;
    for (int i = 0; i < sourceFrames; ++i) {
        downSample->channels[0][static_cast<size_t>(i)] =
            static_cast<float>(std::sin(twoPi * 16000.0 * static_cast<double>(i) / sampleRate));
    }

    SampleVoice downVoice;
    downVoice.start(downSample, 1.0f, -12, 0, sampleRate, 1.0f, -1.0f);
    std::vector<float> downLeft(4096, 0.0f), downRight(4096, 0.0f);
    downVoice.renderNextBlock(downLeft.data(), downRight.data(), 4096);

    const double fundamentalMag = measureSineBinMagnitude(downLeft.data(), 256, 2048, 8000.0, sampleRate);
    const double imageMag = measureSineBinMagnitude(downLeft.data(), 256, 2048, 16000.0, sampleRate);
    REQUIRE(fundamentalMag > 0.85);
    REQUIRE(imageMag < fundamentalMag * 0.002);

    auto upSample = std::make_shared<AudioSampleBuffer>();
    upSample->sampleRate = sampleRate;
    upSample->numChannels = 1;
    upSample->numSamples = sourceFrames;
    upSample->channels = {std::vector<float>(sourceFrames, 0.0f)};
    upSample->isLoaded = true;
    for (int i = 0; i < sourceFrames; ++i) {
        upSample->channels[0][static_cast<size_t>(i)] =
            static_cast<float>(std::sin(twoPi * 15500.0 * static_cast<double>(i) / sampleRate));
    }

    SampleVoice upVoice;
    upVoice.start(upSample, 1.0f, 12, 0, sampleRate, 1.0f, -1.0f);
    std::vector<float> upLeft(2048, 0.0f), upRight(2048, 0.0f);
    upVoice.renderNextBlock(upLeft.data(), upRight.data(), 2048);

    const double foldedAliasMag = measureSineBinMagnitude(upLeft.data(), 256, 1024, 17000.0, sampleRate);
    REQUIRE(foldedAliasMag < 0.002);
}

TEST_CASE("WavReader builds octave mip pyramid and SampleVoice selects matching mip octave", "[SampleVoice][WavReader][mip]") {
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.28318530717958647692;
    constexpr int numFrames = 16384;

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("openfruit_mip_pyramid_test.wav");
    tempFile.deleteFile();
    {
        juce::AudioBuffer<float> wavBuf(1, numFrames);
        for (int i = 0; i < numFrames; ++i) {
            const double t = static_cast<double>(i) / sampleRate;
            wavBuf.setSample(0, i, static_cast<float>(0.5 * std::sin(twoPi * 300.0 * t) + 0.4 * std::sin(twoPi * 20000.0 * t)));
        }
        std::unique_ptr<juce::OutputStream> out(tempFile.createOutputStream());
        REQUIRE(out != nullptr);
        juce::WavAudioFormat wavFmt;
        juce::AudioFormat& fmt = wavFmt;
        auto writer = fmt.createWriterFor(out, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate(sampleRate)
                                                   .withNumChannels(1)
                                                   .withBitsPerSample(24));
        REQUIRE(writer != nullptr);
        REQUIRE(writer->writeFromAudioSampleBuffer(wavBuf, 0, numFrames));
    }

    const auto loaded = WavReader::loadSample(tempFile.getFullPathName().toStdString());
    tempFile.deleteFile();
    REQUIRE(loaded != nullptr);
    REQUIRE(loaded->mipLevels.size() == 3);
    CHECK(loaded->mipLevels[0].decimation == 2);
    CHECK(loaded->mipLevels[1].decimation == 4);
    CHECK(loaded->mipLevels[2].decimation == 8);

    SampleVoice v1x, v2x, v4x, v8x;
    v1x.start(loaded, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    v2x.start(loaded, 1.0f, 12, 0, sampleRate, 1.0f, -1.0f);
    v4x.start(loaded, 1.0f, 24, 0, sampleRate, 1.0f, -1.0f);
    v8x.start(loaded, 1.0f, 42, 0, sampleRate, 1.0f, -1.0f);

    CHECK(v1x.activeMipDecimation() == 1);
    CHECK(v2x.activeMipDecimation() == 2);
    CHECK(v4x.activeMipDecimation() == 4);
    CHECK(v8x.activeMipDecimation() == 8);

    std::vector<float> left(1024, 0.0f), right(1024, 0.0f);
    v8x.renderNextBlock(left.data(), right.data(), 1024);
    const double wantedMag = measureSineBinMagnitude(left.data(), 64, 512, 300.0 * std::pow(2.0, 42.0 / 12.0), sampleRate);
    // 20000 Hz * 2^(42/12) = 226274 Hz, which folds to |226274 - 5 * 48000| = 13726 Hz if un-decimated
    const double aliasedMag = measureSineBinMagnitude(left.data(), 64, 512, std::abs(20000.0 * std::pow(2.0, 42.0 / 12.0) - 240000.0), sampleRate);
    CHECK(wantedMag > 0.40);
    CHECK(aliasedMag < 0.002);
}

TEST_CASE("SampleVoice 12 dB/oct ZDF TPT SVF updates live on playing tails with resonance and pitch tracking", "[SampleVoice][filter]") {
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.28318530717958647692;
    constexpr int numFrames = 8192;

    auto makeSine = [&](double freqHz) {
        auto sample = std::make_shared<AudioSampleBuffer>();
        sample->sampleRate = sampleRate;
        sample->numChannels = 1;
        sample->numSamples = numFrames;
        sample->channels = {std::vector<float>(numFrames, 0.0f)};
        sample->isLoaded = true;
        for (int i = 0; i < numFrames; ++i) {
            sample->channels[0][static_cast<size_t>(i)] =
                static_cast<float>(std::sin(twoPi * freqHz * static_cast<double>(i) / sampleRate));
        }
        sample->buildMipPyramid();
        return sample;
    };

    // 1. Live cutoff update on an already-playing tail + 12 dB/oct rolloff steepness
    SampleVoice liveVoice;
    liveVoice.start(makeSine(4000.0), 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    std::vector<float> openLeft(512, 0.0f), openRight(512, 0.0f);
    liveVoice.renderNextBlock(openLeft.data(), openRight.data(), 512);
    const double openMag = measureSineBinMagnitude(openLeft.data(), 64, 256, 4000.0, sampleRate);
    REQUIRE(openMag > 0.90);

    liveVoice.updateFilterSettings(350.0f, 20.0f, 0.0f, 0.0f);
    std::vector<float> closedLeft(1024, 0.0f), closedRight(1024, 0.0f);
    liveVoice.renderNextBlock(closedLeft.data(), closedRight.data(), 1024);
    const double closedMag = measureSineBinMagnitude(closedLeft.data(), 512, 512, 4000.0, sampleRate);
    REQUIRE(closedMag < openMag * 0.02);

    // 2. Resonance boost at cutoff
    VoiceSettings flatSettings;
    flatSettings.filterCutoffHz = 1000.0f;
    flatSettings.filterResonance = 0.0f;
    SampleVoice flatVoice;
    flatVoice.start(makeSine(1000.0), 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, flatSettings);
    std::vector<float> flatLeft(1024, 0.0f), flatRight(1024, 0.0f);
    flatVoice.renderNextBlock(flatLeft.data(), flatRight.data(), 1024);

    VoiceSettings resSettings = flatSettings;
    resSettings.filterResonance = 0.75f;
    SampleVoice resVoice;
    resVoice.start(makeSine(1000.0), 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, resSettings);
    std::vector<float> resLeft(1024, 0.0f), resRight(1024, 0.0f);
    resVoice.renderNextBlock(resLeft.data(), resRight.data(), 1024);

    const double flatMag = measureSineBinMagnitude(flatLeft.data(), 256, 512, 1000.0, sampleRate);
    const double resMag = measureSineBinMagnitude(resLeft.data(), 256, 512, 1000.0, sampleRate);
    REQUIRE(resMag > flatMag * 2.5);

    // 3. Optional pitch-tracked cutoff scaling (+12 semitones doubles cutoff from 1200 Hz to 2400 Hz)
    const auto oneKhz = makeSine(1000.0);
    VoiceSettings unTracked;
    unTracked.filterCutoffHz = 1200.0f;
    unTracked.filterKeyTrack = 0.0f;
    SampleVoice unTrackedVoice;
    unTrackedVoice.start(oneKhz, 1.0f, 12, 0, sampleRate, 1.0f, -1.0f, unTracked);
    std::vector<float> unTrackedLeft(1024, 0.0f), unTrackedRight(1024, 0.0f);
    unTrackedVoice.renderNextBlock(unTrackedLeft.data(), unTrackedRight.data(), 1024);

    VoiceSettings keyTracked = unTracked;
    keyTracked.filterKeyTrack = 1.0f;
    SampleVoice keyTrackedVoice;
    keyTrackedVoice.start(oneKhz, 1.0f, 12, 0, sampleRate, 1.0f, -1.0f, keyTracked);
    std::vector<float> keyTrackedLeft(1024, 0.0f), keyTrackedRight(1024, 0.0f);
    keyTrackedVoice.renderNextBlock(keyTrackedLeft.data(), keyTrackedRight.data(), 1024);

    const double unTrackedMag = measureSineBinMagnitude(unTrackedLeft.data(), 256, 512, 2000.0, sampleRate);
    const double keyTrackedMag = measureSineBinMagnitude(keyTrackedLeft.data(), 256, 512, 2000.0, sampleRate);
    REQUIRE(keyTrackedMag > unTrackedMag * 1.8);
}

TEST_CASE("SampleVoice choke and voice stealing execute click-free cosine crossfade", "[SampleVoice][choke]") {
    constexpr double sampleRate = 48000.0;
    auto dcSample = std::make_shared<AudioSampleBuffer>();
    dcSample->sampleRate = sampleRate;
    dcSample->numChannels = 1;
    dcSample->numSamples = 4096;
    dcSample->channels = {std::vector<float>(4096, 0.8f)};
    dcSample->isLoaded = true;

    SampleVoice voice;
    voice.start(dcSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    std::vector<float> preLeft(64, 0.0f), preRight(64, 0.0f);
    voice.renderNextBlock(preLeft.data(), preRight.data(), 64);
    REQUIRE_THAT(preLeft[63], Catch::Matchers::WithinAbs(0.8f, 0.001f));

    voice.choke();
    REQUIRE_FALSE(voice.isPlaying());
    REQUIRE(voice.hasActiveTail());

    std::vector<float> tailLeft(256, 0.0f), tailRight(256, 0.0f);
    voice.renderNextBlock(tailLeft.data(), tailRight.data(), 256);
    CHECK(tailLeft[0] > 0.75f);
    float maxChokeStep = std::abs(tailLeft[0] - preLeft[63]);
    for (size_t i = 1; i < tailLeft.size(); ++i) {
        maxChokeStep = std::max(maxChokeStep, std::abs(tailLeft[i] - tailLeft[i - 1]));
    }
    CHECK(maxChokeStep < 0.03f);
    CHECK_THAT(tailLeft[240], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
    CHECK_FALSE(voice.hasActiveTail());

    // Voice stealing on the same voice slot crossfades smoothly into the new note
    auto silentSample = std::make_shared<AudioSampleBuffer>();
    silentSample->sampleRate = sampleRate;
    silentSample->numChannels = 1;
    silentSample->numSamples = 4096;
    silentSample->channels = {std::vector<float>(4096, 0.0f)};
    silentSample->isLoaded = true;

    voice.start(dcSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    std::fill(preLeft.begin(), preLeft.end(), 0.0f);
    voice.renderNextBlock(preLeft.data(), preRight.data(), 64);
    voice.start(silentSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    std::fill(tailLeft.begin(), tailLeft.end(), 0.0f);
    voice.renderNextBlock(tailLeft.data(), tailRight.data(), 256);
    CHECK(tailLeft[0] > 0.75f);
    CHECK_THAT(tailLeft[240], Catch::Matchers::WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("SampleVoice applies raised-cosine micro-fade at non-zero trim boundaries", "[SampleVoice][regions]") {
    constexpr double sampleRate = 48000.0;
    auto dcSample = std::make_shared<AudioSampleBuffer>();
    dcSample->sampleRate = sampleRate;
    dcSample->numChannels = 1;
    dcSample->numSamples = 4096;
    dcSample->channels = {std::vector<float>(4096, 0.8f)};
    dcSample->isLoaded = true;

    VoiceSettings trimmed;
    trimmed.sampleStart = 0.25f;
    trimmed.sampleEnd = 0.50f;
    SampleVoice voice;
    voice.start(dcSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, trimmed);

    std::vector<float> left(1024, 0.0f), right(1024, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 1024);

    CHECK(std::abs(left[0]) < 0.02f);
    CHECK_THAT(left[96], Catch::Matchers::WithinAbs(0.8f, 0.005f));
    CHECK(std::abs(left[1023]) < 0.02f);
    float maxTrimDelta = std::abs(left[0]);
    for (size_t i = 1; i < left.size(); ++i) {
        maxTrimDelta = std::max(maxTrimDelta, std::abs(left[i] - left[i - 1]));
    }
    CHECK(maxTrimDelta < 0.09f);

    // Untrimmed start (sampleStart == 0) keeps full transient punch at sample 0
    SampleVoice untrimmedVoice;
    untrimmedVoice.start(dcSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f);
    std::vector<float> untrimmedLeft(16, 0.0f), untrimmedRight(16, 0.0f);
    untrimmedVoice.renderNextBlock(untrimmedLeft.data(), untrimmedRight.data(), 16);
    CHECK_THAT(untrimmedLeft[0], Catch::Matchers::WithinAbs(0.8f, 0.01f));
}

TEST_CASE("SampleVoice ADAA saturation suppresses folded harmonics on high-frequency tones", "[SampleVoice][drive]") {
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.28318530717958647692;
    constexpr int numFrames = 8192;

    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = sampleRate;
    sample->numChannels = 1;
    sample->numSamples = numFrames;
    sample->channels = {std::vector<float>(numFrames, 0.0f)};
    sample->isLoaded = true;
    for (int i = 0; i < numFrames; ++i) {
        sample->channels[0][static_cast<size_t>(i)] =
            static_cast<float>(0.85 * std::sin(twoPi * 9500.0 * static_cast<double>(i) / sampleRate));
    }

    VoiceSettings settings;
    settings.drive = 1.0f;
    SampleVoice voice;
    voice.start(sample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, settings);
    std::vector<float> left(4096, 0.0f), right(4096, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 4096);

    // 5 * 9500 Hz = 47500 Hz folds to 500 Hz at 1x naive tanh (> 0.06 magnitude)
    const double fundamentalMag = measureSineBinMagnitude(left.data(), 256, 2048, 9500.0, sampleRate);
    const double folded500Hz = measureSineBinMagnitude(left.data(), 256, 2048, 500.0, sampleRate);
    REQUIRE(fundamentalMag > 0.70);
    REQUIRE(folded500Hz < 0.005);
}

TEST_CASE("SampleVoice ADSR uses exponential RC curves with convex attack and concave decay", "[SampleVoice][envelope]") {
    constexpr double sampleRate = 48000.0;
    auto sample = std::make_shared<AudioSampleBuffer>();
    sample->sampleRate = sampleRate;
    sample->numChannels = 1;
    sample->numSamples = 4096;
    sample->channels = {std::vector<float>(4096, 1.0f)};
    sample->isLoaded = true;

    VoiceSettings settings;
    settings.attackSeconds = 0.01f;  // 480 samples
    settings.decaySeconds = 0.02f;   // 960 samples
    settings.sustainLevel = 0.0f;
    SampleVoice voice;
    voice.start(sample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, settings);

    std::vector<float> left(1600, 0.0f), right(1600, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 1600);

    // Mid-attack (240 / 480): convex curve is above linear 0.5
    CHECK(left[240] > 0.58f);
    CHECK(left[240] < 0.92f);
    // Mid-decay (480 + 480 = 960): concave drum decay is well below linear 0.5
    CHECK(left[960] < 0.30f);
    CHECK(left[960] > 0.01f);
}

TEST_CASE("SampleVoice key-normalized pitch mode applies SonicAnalyzer detuneCents and root-note offset", "[SampleVoice][tuning]") {
    constexpr double sampleRate = 48000.0;
    constexpr double twoPi = 6.28318530717958647692;
    constexpr int numFrames = 24000;

    // 445.17 Hz is A4 (MIDI 69) + 20 cents
    const double detunedA4Hz = 440.0 * std::pow(2.0, 20.0 / 1200.0);
    auto pitchedSample = std::make_shared<AudioSampleBuffer>();
    pitchedSample->sampleRate = sampleRate;
    pitchedSample->numChannels = 1;
    pitchedSample->numSamples = numFrames;
    pitchedSample->channels = {std::vector<float>(numFrames, 0.0f)};
    pitchedSample->isLoaded = true;
    for (int i = 0; i < numFrames; ++i) {
        pitchedSample->channels[0][static_cast<size_t>(i)] =
            static_cast<float>(0.8 * std::sin(twoPi * detunedA4Hz * static_cast<double>(i) / sampleRate));
    }
    pitchedSample->toneResult = SonicAnalyzer::analyze(pitchedSample->channels[0].data(), numFrames, sampleRate);
    pitchedSample->buildMipPyramid();
    REQUIRE(pitchedSample->toneResult.hasClearTone);
    REQUIRE(pitchedSample->toneResult.midiNote == 69);
    REQUIRE(std::abs(pitchedSample->toneResult.detuneCents - 20) <= 1);

    VoiceSettings normSettings;
    normSettings.keyNormalizePitch = true;
    SampleVoice voice;
    voice.start(pitchedSample, 1.0f, 0, 0, sampleRate, 1.0f, -1.0f, normSettings);
    std::vector<float> left(12000, 0.0f), right(12000, 0.0f);
    voice.renderNextBlock(left.data(), right.data(), 12000);

    const auto outTone = SonicAnalyzer::analyze(left.data() + 256, 10000, sampleRate);
    REQUIRE(outTone.hasClearTone);
    CHECK(outTone.midiNote == 72); // C5 = 523.25 Hz
    CHECK(std::abs(outTone.detuneCents) <= 1);
    CHECK_THAT(outTone.dominantFrequencyHz, Catch::Matchers::WithinAbs(523.251, 1.0));
}
