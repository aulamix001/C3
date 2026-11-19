#include <juce_audio_processors/juce_audio_processors.h>
#include "Compressor.h"
#include <array>
#include <vector>

namespace
{
using namespace juce;
using State = AudioProcessorValueTreeState;

struct Control
{
    const char* id;
    const char* label;
    const char* unit;
    float low, high, initial, step, skew;
};
constexpr std::array<Control, 13> controls {{
    {"threshold", "Threshold", " dB", -60, 0, -18, .1f, 1},
    {"ratio", "Ratio", ":1", 1, 20, 4, .1f, .55f},
    {"attack", "Attack", " ms", .05f, 250, 12, .01f, .35f},
    {"release", "Release", " ms", 5, 2000, 120, 1, .4f},
    {"knee", "Knee", " dB", 0, 36, 6, .1f, 1},
    {"range", "Range", " dB", 0, 60, 36, .1f, 1},
    {"hold", "Hold", " ms", 0, 500, 0, 1, .45f},
    {"makeup", "Makeup", " dB", -24, 24, 0, .1f, 1},
    {"mix", "Mix", " %", 0, 100, 100, 1, 1},
    {"sidefreq", "Detector HP", " Hz", 20, 500, 20, 1, .5f},
    {"link", "Stereo link", " %", 0, 100, 100, 1, 1},
    {"lookahead", "Lookahead", " ms", 0, 10, 0, .1f, 1},
    {"sideLowPass", "Detector LP", " Hz", 500, 22000, 22000, 1, .4f}
}};
struct Preset
{
    const char* group;
    const char* name;
    std::array<float, 11> values;
};
constexpr Preset presets[] {
    {"Basic", "Default", {-18,4,12,120,6,36,0,0,100,20,100}},
    {"Bass", "Steady Low End", {-20,5,20,160,9,16,8,2,100,35,100}},
    {"Drums", "Transient Punch", {-17,4,28,85,4,16,0,1,75,55,90}},
    {"Drums", "Parallel Crush", {-28,12,2,65,3,24,10,5,45,70,100}},
    {"Guitar", "Chord Control", {-21,3,18,150,12,12,0,2,90,80,85}},
    {"Keys", "Gentle Leveling", {-24,2,35,240,18,8,0,1,100,60,100}},
    {"Mixbus", "Subtle Glue", {-14,2,35,180,12,7,0,1,100,95,100}},
    {"Synth", "Rhythmic Shape", {-22,6,6,220,8,20,12,0,85,65,90}},
    {"Vocals", "Smooth Lead", {-23,3,8,125,14,13,5,3,100,95,100}},
    {"Vocals", "Fast Peaks", {-15,6,1,75,4,12,0,1,100,110,100}}
};

State::ParameterLayout makeLayout()
{
    State::ParameterLayout layout;
    for (const auto& c : controls)
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID{c.id, 1}, c.label,
            NormalisableRange<float>{c.low, c.high, c.step, c.skew}, c.initial,
            AudioParameterFloatAttributes{}.withLabel(String(c.unit).trim())));
    for (const auto* id : {"external", "listen", "autoRelease", "bypass", "autoGain"})
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, id, false));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID{"detector", 1},
        "Detector", StringArray{"Peak", "RMS"}, 0));
    return layout;
}

class Processor final : public AudioProcessor
{
public:
    Processor() : AudioProcessor(BusesProperties()
        .withInput("Input", AudioChannelSet::stereo(), true)
        .withOutput("Output", AudioChannelSet::stereo(), true)
        .withInput("Sidechain", AudioChannelSet::stereo(), false)),
        state(*this, &undo, "NXProC3", makeLayout())
    {
        for (size_t i = 0; i < controls.size(); ++i)
            parameters[i] = state.getRawParameterValue(controls[i].id);
        external = state.getRawParameterValue("external");
        listen = state.getRawParameterValue("listen");
        autoRelease = state.getRawParameterValue("autoRelease");
        bypass = state.getRawParameterValue("bypass");
        autoGain = state.getRawParameterValue("autoGain");
        detectorMode = state.getRawParameterValue("detector");
    }

    const String getName() const override { return "NX PRO C 3"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;
    AudioProcessorParameter* getBypassParameter() const override { return state.getParameter("bypass"); }
    int getNumPrograms() override { return static_cast<int>(std::size(presets)); }
    int getCurrentProgram() override { return program.load(); }
    const String getProgramName(int index) override
    {
        return isPositiveAndBelow(index, getNumPrograms()) ? presets[index].name : "";
    }
    void changeProgramName(int, const String&) override {}
    void setCurrentProgram(int index) override
    {
        if (!isPositiveAndBelow(index, getNumPrograms()))
            return;
        program = index;
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto& parameter = *state.getParameter(controls[i].id);
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost(parameter.convertTo0to1(i < presets[index].values.size() ? presets[index].values[i] : controls[i].initial));
            parameter.endChangeGesture();
        }
    }

    bool isBusesLayoutSupported(const BusesLayout& layout) const override
    {
        const auto main = layout.getMainInputChannelSet();
        const auto side = layout.getChannelSet(true, 1);
        return main == layout.getMainOutputChannelSet()
            && (main == AudioChannelSet::mono() || main == AudioChannelSet::stereo())
            && (side.isDisabled() || side == AudioChannelSet::mono() || side == AudioChannelSet::stereo());
    }
    bool supportsDoublePrecisionProcessing() const override { return true; }
    void prepareToPlay(double rate, int) override
    {
        sampleRate = rate;
        followers = {};
        position = 0;
        latency = roundToInt(rate * .010);
        setLatencySamples(latency);
        for (auto& channel : followers)
            channel.delay.resize(static_cast<size_t>(latency + 1));

        for (size_t i = 0; i < ramps.size(); ++i)
        {
            ramps[i].reset(rate, .015);
            ramps[i].setCurrentAndTargetValue(value(i));
        }
        autoTrim.reset(rate, .120);
        autoTrim.setCurrentAndTargetValue(0);
        bypassMix.reset(rate, .010);
        bypassMix.setCurrentAndTargetValue(bypass->load());
        monitorMix.reset(rate, .010);
        monitorMix.setCurrentAndTargetValue(listen->load());
    }
    void releaseResources() override {}
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override { process(buffer, false); }
    void processBlock(AudioBuffer<double>& buffer, MidiBuffer&) override { process(buffer, false); }
    void processBlockBypassed(AudioBuffer<float>& buffer, MidiBuffer&) override { process(buffer, true); }
    void processBlockBypassed(AudioBuffer<double>& buffer, MidiBuffer&) override { process(buffer, true); }

    template<typename Sample>
    void process(AudioBuffer<Sample>& buffer, bool hostBypass)
    {
        ScopedNoDenormals guard;
        auto audio = getBusBuffer(buffer, false, 0);
        auto side = getBusBuffer(buffer, true, 1);
        const bool useExternal = external->load() > .5f && side.getNumChannels() > 0;
        const auto channels = audio.getNumChannels();
        const auto detectorChannels = useExternal ? side.getNumChannels() : channels;
        const bool adapt = autoRelease->load() > .5f;
        monitorMix.setTargetValue(listen->load());
        const bool rms = detectorMode->load() > .5f;
        bypassMix.setTargetValue(hostBypass ? 1.0 : bypass->load());
        for (size_t i = 0; i < ramps.size(); ++i)
            ramps[i].setTargetValue(value(i));
        autoTrim.setTargetValue(autoGain->load() > .5f
            ? nx::automaticMakeup(value(0), value(1), value(4), value(5)) : 0.0);
        const auto rmsCoefficient = nx::smoothing(10, sampleRate);
        double inPeak = 0, outPeak = 0, maximumReduction = 0;

        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const auto threshold = ramps[0].getNextValue(), ratio = ramps[1].getNextValue();
            const auto attack = nx::smoothing(ramps[2].getNextValue(), sampleRate);
            const auto releaseMs = ramps[3].getNextValue();
            const auto release = nx::smoothing(releaseMs, sampleRate);
            const auto fastRelease = nx::smoothing(releaseMs * .45, sampleRate);
            const auto knee = ramps[4].getNextValue(), range = ramps[5].getNextValue();
            const auto holdSamples = roundToInt(ramps[6].getNextValue() * .001 * sampleRate);
            const auto outputGain = nx::gain(ramps[7].getNextValue() + autoTrim.getNextValue());
            const auto wet = ramps[8].getNextValue() * .01;
            const auto highPass = std::exp(-MathConstants<double>::twoPi * ramps[9].getNextValue() / sampleRate);
            const auto link = ramps[10].getNextValue() * .01;
            const auto advance = jlimit(0.0, static_cast<double>(latency),
                                       ramps[11].getNextValue() * .001 * sampleRate);
            const auto lpFrequency = ramps[12].getNextValue();
            const auto lowPass = std::exp(-MathConstants<double>::twoPi * lpFrequency / sampleRate);
            const auto bypassAmount = bypassMix.getNextValue();
            const auto monitorAmount = monitorMix.getNextValue();
            std::array<double, 2> detectorSignal {}, levels {};
            double linked = 0;
            for (int ch = 0; ch < detectorChannels; ++ch)
            {
                const double raw = useExternal ? side.getSample(ch, i) : audio.getSample(ch, i);
                const auto source = std::isfinite(raw) ? raw : 0.0;
                auto& follower = followers[static_cast<size_t>(ch)];
                follower.low = source + highPass * (follower.low - source);
                const auto high = source - follower.low;
                follower.filtered = high + lowPass * (follower.filtered - high);
                const auto lowPassBypass = jlimit(0.0, 1.0, (lpFrequency - 20000.0) / 2000.0);
                const auto filtered = follower.filtered + lowPassBypass * (high - follower.filtered);
                detectorSignal[static_cast<size_t>(ch)] = filtered;
                follower.energy = filtered * filtered + rmsCoefficient * (follower.energy - filtered * filtered);
                const auto level = rms ? std::sqrt(std::max(0.0, follower.energy)) : std::abs(filtered);
                levels[static_cast<size_t>(ch)] = level;
                linked = std::max(linked, level);
            }

            const auto earliest = (position + 1) % (latency + 1);
            const auto offset = static_cast<int>(advance);
            const auto fraction = advance - offset;
            const auto read = (earliest + offset) % (latency + 1);
            const auto next = (read + 1) % (latency + 1);
            for (int ch = 0; ch < channels; ++ch)
            {
                auto& follower = followers[static_cast<size_t>(ch)];
                const auto detector = static_cast<size_t>(std::min(ch, detectorChannels - 1));
                const auto level = channels == 1 ? linked : levels[detector] + link * (linked - levels[detector]);
                const auto target = nx::reduction(nx::decibels(level), threshold, ratio, knee, range);
                if (target > follower.reduction)
                {
                    follower.reduction = target + attack * (follower.reduction - target);
                    follower.hold = holdSamples;
                }
                else if (follower.hold > 0)
                    --follower.hold;
                else
                {
                    const auto coefficient = adapt && follower.reduction < 8.0 ? fastRelease : release;
                    follower.reduction = target + coefficient * (follower.reduction - target);
                }

                const double input = audio.getSample(ch, i);
                auto& delay = follower.delay;
                delay[static_cast<size_t>(position)] = {std::isfinite(input) ? input : 0.0,
                    detectorSignal[detector], follower.reduction};
                const auto& delayed = delay[static_cast<size_t>(earliest)];
                const auto applied = std::min(range,
                    delay[static_cast<size_t>(read)].reduction * (1.0 - fraction)
                    + delay[static_cast<size_t>(next)].reduction * fraction);
                const auto compressed = delayed.input * ((1.0 - wet) + wet * outputGain * nx::gain(-applied));
                const auto processed = compressed + monitorAmount * (delayed.detector - compressed);
                const auto output = processed + bypassAmount * (delayed.input - processed);
                audio.setSample(ch, i, static_cast<Sample>(output));
                inPeak = std::max(inPeak, std::abs(delayed.input));
                outPeak = std::max(outPeak, std::abs(output));
                maximumReduction = std::max(maximumReduction, applied * (1.0 - bypassAmount));
            }
            position = earliest;
        }
        inputMeter = static_cast<float>(nx::decibels(inPeak));
        outputMeter = static_cast<float>(nx::decibels(outPeak));
        reductionMeter = static_cast<float>(maximumReduction);
        pushPeak(inputPeak, inputMeter.load());
        pushPeak(outputPeak, outputMeter.load());
        pushPeak(reductionPeak, reductionMeter.load());
        if (inPeak >= 1.0) inputClip.store(true);
        if (outPeak >= 1.0) outputClip.store(true);
    }

    void getStateInformation(MemoryBlock& block) override
    {
        auto snapshot = state.copyState();
        snapshot.setProperty("schema", 2, nullptr);
        snapshot.setProperty("program", program.load(), nullptr);
        if (const auto xml = snapshot.createXml())
            copyXmlToBinary(*xml, block);
    }
    void setStateInformation(const void* data, int bytes) override
    {
        if (auto xml = getXmlFromBinary(data, bytes); xml && xml->hasTagName(state.state.getType()))
        {
            auto snapshot = ValueTree::fromXml(*xml);
            program = jlimit(0, getNumPrograms() - 1, static_cast<int>(snapshot.getProperty("program", 0)));
            if (!snapshot.hasProperty("schema"))
            {
                for (const auto* id : {"lookahead", "sideLowPass", "autoGain", "detector"})
                {
                    auto& parameter = *state.getParameter(id);
                    ValueTree child("PARAM");
                    child.setProperty("id", id, nullptr);
                    child.setProperty("value", parameter.convertFrom0to1(parameter.getDefaultValue()), nullptr);
                    snapshot.addChild(child, -1, nullptr);
                }
            }
            state.replaceState(snapshot);
        }
    }
    void applySnapshot(const void* data, int bytes)
    {
        if (auto xml = getXmlFromBinary(data, bytes); xml && xml->hasTagName(state.state.getType()))
        {
            state.copyState();
            undo.beginNewTransaction();
            program = jlimit(0, getNumPrograms() - 1, xml->getIntAttribute("program"));
            for (auto* child : xml->getChildIterator())
            {
                const auto id = child->getStringAttribute("id");
                if (auto* parameter = state.getParameter(id))
                {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(
                        static_cast<float>(child->getDoubleAttribute("value"))));
                    parameter->endChangeGesture();
                }
            }
            state.copyState();
        }
    }
    float value(size_t i) const noexcept { return parameters[i]->load(); }

    UndoManager undo;
    State state;
    std::atomic<float> inputMeter {-100}, outputMeter {-100}, reductionMeter {0};
    std::atomic<float> inputPeak {-100}, outputPeak {-100}, reductionPeak {0};
    std::atomic<bool> inputClip {false}, outputClip {false};
private:
    static void pushPeak(std::atomic<float>& peak, float value) noexcept
    {
        auto previous = peak.load(std::memory_order_relaxed);
        if (value > previous && !peak.compare_exchange_strong(previous, value, std::memory_order_relaxed)
            && value > previous)
            peak.compare_exchange_strong(previous, value, std::memory_order_relaxed);
    }
    struct Delayed { double input = 0, detector = 0, reduction = 0; };
    struct Follower
    {
        double low = 0, filtered = 0, energy = 0, reduction = 0;
        int hold = 0;
        std::vector<Delayed> delay;
    };
    std::array<Follower, 2> followers {};
    std::array<std::atomic<float>*, controls.size()> parameters {};
    std::atomic<float>* external = nullptr;
    std::atomic<float>* listen = nullptr;
    std::atomic<float>* autoRelease = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* autoGain = nullptr;
    std::atomic<float>* detectorMode = nullptr;
    std::atomic<int> program {0};
    std::array<SmoothedValue<double>, controls.size()> ramps;
    SmoothedValue<double> autoTrim, bypassMix, monitorMix;
    double sampleRate = 44100;
    int latency = 0, position = 0;
};

#include "Editor.inc"

AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor(); }
