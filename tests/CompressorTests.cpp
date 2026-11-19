#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Compressor.h"
#include <array>
#include <limits>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double pi = 3.14159265358979323846;

struct Instance
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor {createPluginFilter()};

    void set(const char* id, float value)
    {
        for (auto* raw : processor->getParameters())
            if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(raw);
                parameter != nullptr && parameter->getParameterID() == id)
            {
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
                return;
            }
        FAIL("Missing parameter: " << id);
    }
    void layout(int channels, bool external)
    {
        auto buses = processor->getBusesLayout();
        const auto main = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        buses.inputBuses.set(0, main);
        buses.outputBuses.set(0, main);
        buses.inputBuses.set(1, external ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::disabled());
        REQUIRE(processor->setBusesLayout(buses));
    }
};

template<typename Sample, typename Signal>
juce::AudioBuffer<Sample> render(Instance& instance, int channels, int sampleCount, Signal signal)
{
    juce::AudioBuffer<Sample> buffer(channels, sampleCount);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < sampleCount; ++i)
            buffer.setSample(ch, i, static_cast<Sample>(signal(ch, i)));
    juce::MidiBuffer midi;
    instance.processor->processBlock(buffer, midi);
    return buffer;
}

double sine(int index, double rate, double hz, double amplitude)
{
    return amplitude * std::sin(2 * pi * hz * index / rate);
}

double harmonic(const juce::AudioBuffer<double>& audio, int start, int length, double rate, double hz)
{
    double sineBin = 0, cosineBin = 0;
    for (int i = 0; i < length; ++i)
    {
        const auto phase = 2 * pi * hz * i / rate;
        const auto sample = audio.getSample(0, start + i);
        sineBin += sample * std::sin(phase);
        cosineBin += sample * std::cos(phase);
    }
    return 2 * std::hypot(sineBin, cosineBin) / length;
}
}

TEST_CASE("Static ratio, threshold, range and continuous soft knee")
{
    CHECK(nx::reduction(-30, -18, 4, 6, 60) == 0);
    CHECK(nx::reduction(-6, -18, 4, 0, 60) == 9);
    CHECK(nx::reduction(0, -18, 4, 0, 2) == 2);
    CHECK(nx::reduction(0, -18, 1, 0, 60) == 0);
    CHECK(nx::reduction(-18, -18, 4, 8, 60) == doctest::Approx(.75));
    for (double boundary : {-22., -14.})
        CHECK(std::abs(nx::reduction(boundary + .00001, -18, 4, 8, 60)
                     - nx::reduction(boundary - .00001, -18, 4, 8, 60)) < .00002);
    CHECK(nx::automaticMakeup(-60, 20, 0, 60) == 6);
    CHECK(nx::automaticMakeup(-2, 20, 0, 60) == 0);
}

TEST_CASE("Stereo dry transport and bypass stay aligned at each sample rate")
{
    for (const auto rate : {44100., 48000., 96000.})
    {
        Instance instance;
        instance.set("mix", 0);
        instance.set("lookahead", 8);
        instance.processor->prepareToPlay(rate, 2048);
        const auto latency = instance.processor->getLatencySamples();
        REQUIRE(latency == juce::roundToInt(.010 * rate));
        auto audio = render<double>(instance, 2, 2048, [](int channel, int i)
        { return i == 0 ? (channel == 0 ? 1.0 : -.6) : 0.0; });
        CHECK(audio.getSample(0, latency) == 1);
        CHECK(audio.getSample(1, latency) == -.6);
        CHECK(audio.getMagnitude(0, 0, latency) == 0);
        instance.set("bypass", 1);
        instance.set("mix", 100);
        instance.processor->prepareToPlay(rate, 2048);
        juce::AudioBuffer<float> bypassed(2, 2048);
        bypassed.clear();
        bypassed.setSample(0, 0, .75f);
        juce::MidiBuffer midi;
        instance.processor->processBlockBypassed(bypassed, midi);
        CHECK(bypassed.getSample(0, latency) == .75f);
    }
}

TEST_CASE("Lookahead anticipates a transient and parallel mix interpolates aligned paths")
{
    const auto measure = [](float anticipation, float wet)
    {
        Instance instance;
        instance.layout(1, true);
        instance.set("threshold", -30);
        instance.set("ratio", 20);
        instance.set("attack", 3);
        instance.set("mix", wet);
        instance.set("lookahead", anticipation);
        instance.set("external", 1);
        instance.processor->prepareToPlay(48000, 2048);
        const auto audio = render<double>(instance, 2, 2048, [](int ch, int i)
        { return ch == 0 ? (i == 512 ? 1.0 : 0.0) : i >= 512 ? ((i / 12) % 2 == 0 ? .8 : -.8) : 0.; });
        return audio.getSample(0, 512 + 480);
    };
    const auto wet = measure(10, 100);
    CHECK(wet < measure(0, 100) * .5);
    CHECK(measure(10, 50) == doctest::Approx((1 + wet) / 2));
}

TEST_CASE("External key controls mono compression, hold and release")
{
    const auto gainAt = [](float attack, float release, float hold, bool external, int sample)
    {
        Instance instance;
        instance.layout(1, true);
        instance.set("threshold", -20);
        instance.set("ratio", 8);
        instance.set("attack", attack);
        instance.set("release", release);
        instance.set("hold", hold);
        instance.set("external", external ? 1.f : 0.f);
        instance.processor->prepareToPlay(48000, 8192);
        const auto audio = render<double>(instance, 2, 8192, [](int ch, int i)
        {
            if (ch == 0) return .035;
            return i < 2048 ? ((i / 12) % 2 == 0 ? .8 : -.8) : 0.;
        });
        return audio.getSample(0, sample + 480) / .035;
    };
    CHECK(gainAt(1, 20, 0, false, 1024) == doctest::Approx(1));
    CHECK(gainAt(1, 20, 0, true, 1024) < .4);
    CHECK(gainAt(1, 20, 0, true, 144) < gainAt(40, 20, 0, true, 144));
    CHECK(gainAt(1, 20, 0, true, 4096) > gainAt(1, 150, 0, true, 4096));
    CHECK(gainAt(1, 20, 0, true, 4096) > gainAt(1, 20, 80, true, 4096));
}

TEST_CASE("Rapid automation stays finite and stereo float and double agree")
{
    Instance floats, doubles;
    constexpr auto rate = 48000.;
    floats.processor->prepareToPlay(rate, 128);
    doubles.processor->prepareToPlay(rate, 128);
    for (int block = 0; block < 150; ++block)
    {
        for (auto* instance : {&floats, &doubles})
        {
            instance->set("threshold", block % 2 == 0 ? -60.f : -3.f);
            instance->set("ratio", 1.f + block % 20);
            instance->set("lookahead", static_cast<float>(block % 11));
            instance->set("detector", block % 4 == 0 ? 1.f : 0.f);
        }
        const auto signal = [block](int channel, int i)
        {
            if (block < 20) return 0.;
            if (i == 50 && block == 35) return std::numeric_limits<double>::infinity();
            if (i == 60 && block == 36) return std::numeric_limits<double>::quiet_NaN();
            return sine(block * 128 + i, rate, channel ? 173. : 911., .6);
        };
        const auto left = render<float>(floats, 2, 128, signal);
        const auto right = render<double>(doubles, 2, 128, signal);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
            {
                CHECK(std::isfinite(left.getSample(ch, i)));
                CHECK(std::isfinite(right.getSample(ch, i)));
                CHECK(std::abs(left.getSample(ch, i) - right.getSample(ch, i)) < 1.e-5);
                if (block < 20) CHECK(right.getSample(ch, i) == 0);
            }
    }
}

TEST_CASE("Unity gain response and THD at 100 Hz, 1 kHz and 8 kHz")
{
    for (const auto frequency : {100., 1000., 8000.})
    {
        Instance instance;
        instance.layout(1, false);
        instance.set("ratio", 1);
        instance.processor->prepareToPlay(48000, 8192);
        const auto audio = render<double>(instance, 1, 8192, [frequency](int, int i)
        { return sine(i, 48000, frequency, .5); });
        const auto fundamental = harmonic(audio, 2400, 4800, 48000, frequency);
        const auto second = harmonic(audio, 2400, 4800, 48000, frequency * 2);
        CHECK(fundamental == doctest::Approx(.5).epsilon(1.e-8));
        CHECK(second / fundamental < 1.e-8);
        if (frequency < 8000)
            CHECK(harmonic(audio, 2400, 4800, 48000, frequency * 3) / fundamental < 1.e-8);
    }
}

TEST_CASE("All ten factory presets and saved parameter state round-trip")
{
    Instance instance;
    REQUIRE(instance.processor->getNumPrograms() == 10);
    for (int program = 0; program < 10; ++program)
    {
        instance.processor->setCurrentProgram(program);
        instance.set("lookahead", 6.5f);
        juce::MemoryBlock first, second;
        instance.processor->getStateInformation(first);
        Instance restored;
        restored.processor->setStateInformation(first.getData(), static_cast<int>(first.getSize()));
        CHECK(restored.processor->getCurrentProgram() == program);
        CHECK(restored.processor->getProgramName(program) == instance.processor->getProgramName(program));
        restored.processor->getStateInformation(second);
        const auto firstXml = juce::AudioProcessor::getXmlFromBinary(first.getData(), static_cast<int>(first.getSize()));
        const auto secondXml = juce::AudioProcessor::getXmlFromBinary(second.getData(), static_cast<int>(second.getSize()));
        REQUIRE(firstXml != nullptr);
        REQUIRE(secondXml != nullptr);
        for (auto* node : firstXml->getChildIterator())
        {
            const auto id = node->getStringAttribute("id");
            const auto* restoredNode = secondXml->getChildByAttribute("id", id);
            REQUIRE(restoredNode != nullptr);
            CHECK(restoredNode->getDoubleAttribute("value") == doctest::Approx(node->getDoubleAttribute("value")));
        }
    }
}

TEST_CASE("Detector low-pass removes high frequencies from the sidechain audition")
{
    const auto amplitude = [](float lpf, double frequency)
    {
        Instance instance;
        instance.layout(1, true);
        instance.set("external", 1);
        instance.set("listen", 1);
        instance.set("sideLowPass", lpf);
        instance.processor->prepareToPlay(48000, 8192);
        const auto audio = render<double>(instance, 2, 8192, [frequency](int channel, int i)
        { return channel == 0 ? 0. : sine(i, 48000, frequency, .5); });
        return harmonic(audio, 2400, 4800, 48000, frequency);
    };
    CHECK(amplitude(500, 100) / amplitude(22000, 100) > .95);
    CHECK(amplitude(500, 8000) / amplitude(22000, 8000) < .1);
}

TEST_CASE("Old sessions migrate the new controls to their defaults")
{
    Instance instance;
    instance.set("threshold", -23);
    instance.set("lookahead", 10);
    juce::MemoryBlock state;
    instance.processor->getStateInformation(state);
    const auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(xml != nullptr);
    xml->removeAttribute("schema");
    for (const auto* id : {"lookahead", "sideLowPass", "autoGain", "detector"})
        xml->removeChildElement(xml->getChildByAttribute("id", id), true);
    juce::AudioProcessor::copyXmlToBinary(*xml, state);
    instance.processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    instance.processor->getStateInformation(state);
    const auto restored = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    REQUIRE(restored != nullptr);
    CHECK(restored->getChildByAttribute("id", "threshold")->getDoubleAttribute("value") == doctest::Approx(-23));
    CHECK(restored->getChildByAttribute("id", "lookahead")->getDoubleAttribute("value") == 0);
    CHECK(restored->getChildByAttribute("id", "sideLowPass")->getDoubleAttribute("value") == 22000);
}

TEST_CASE("Optional editor snapshots exercise responsive and HiDPI rendering")
{
    const auto target = juce::SystemStats::getEnvironmentVariable("NX_CAPTURE_DIR", {});
    if (target.isEmpty()) return;
    const juce::File directory(target);
    REQUIRE(directory.createDirectory().wasOk());
    Instance instance;
    instance.processor->setCurrentProgram(3);
    instance.processor->prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(instance.processor->createEditor());
    for (int frame = 0; frame < 225; ++frame)
    {
        const auto amplitude = .06 + (.19 + .62 * std::exp(-(frame % 14) * .31)) *
            (.64 + .36 * std::sin(frame * .129));
        for (int block = 0; block < 3; ++block)
            render<float>(instance, 2, 512, [=](int ch, int i)
            {
                return sine((frame * 3 + block) * 512 + i, 48000, ch == 0 ? 735 : 763, amplitude);
            });
        juce::Thread::sleep(34);
        juce::Timer::callPendingTimersSynchronously();
    }
    const auto capture = [&](const juce::String& name, float scale)
    {
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
        REQUIRE(image.isValid());
        CHECK(image.getWidth() == juce::roundToInt(editor->getWidth() * scale));
        CHECK(std::abs(image.getHeight() - juce::roundToInt(editor->getHeight() * scale)) <= 1);
        auto file = directory.getChildFile(name + ".png");
        file.deleteFile();
        auto output = file.createOutputStream();
        REQUIRE(output != nullptr);
        REQUIRE(juce::PNGImageFormat{}.writeImageToStream(image, *output));
    };
    const juce::Point<int> normal {editor->getWidth(), editor->getHeight()};
    for (float scale : {1.f, 1.25f, 1.5f, 2.f})
        capture("normal-" + juce::String(static_cast<int>(scale * 100)), scale);
    for (int width : {760, 1280})
    {
        editor->setSize(width, juce::roundToInt(width * normal.y / static_cast<float>(normal.x)));
        capture("width-" + juce::String(width), 1);
    }
    editor->setSize(7680, juce::roundToInt(7680 * normal.y / static_cast<float>(normal.x)));
    const auto large = editor->createComponentSnapshot(editor->getLocalBounds());
    CHECK(large.getWidth() == 7680);
    CHECK(large.getHeight() == editor->getHeight());
}
