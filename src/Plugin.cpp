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
constexpr std::array<Control, 11> controls {{
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
    {"link", "Stereo link", " %", 0, 100, 100, 1, 1}
}};
struct Preset
{
    const char* group;
    const char* name;
    std::array<float, controls.size()> values;
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
    for (const auto* id : {"external", "listen", "autoRelease", "bypass"})
        layout.add(std::make_unique<AudioParameterBool>(ParameterID{id, 1}, id, false));
    return layout;
}

class Processor final : public AudioProcessor
{
public:
    Processor() : AudioProcessor(BusesProperties()
        .withInput("Input", AudioChannelSet::stereo(), true)
        .withOutput("Output", AudioChannelSet::stereo(), true)
        .withInput("Sidechain", AudioChannelSet::stereo(), false)),
        state(*this, nullptr, "NXProC3", makeLayout())
    {
        for (size_t i = 0; i < controls.size(); ++i)
            parameters[i] = state.getRawParameterValue(controls[i].id);
        external = state.getRawParameterValue("external");
        listen = state.getRawParameterValue("listen");
        autoRelease = state.getRawParameterValue("autoRelease");
        bypass = state.getRawParameterValue("bypass");
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
            parameter.setValueNotifyingHost(parameter.convertTo0to1(presets[index].values[i]));
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
    void prepareToPlay(double rate, int) override
    {
        sampleRate = rate;
        followers = {};
        mix.reset(rate, .015);
        makeup.reset(rate, .015);
        mix.setCurrentAndTargetValue(value(8) * .01f);
        makeup.setCurrentAndTargetValue(nx::gain(value(7)));
    }
    void releaseResources() override {}
    using AudioProcessor::processBlock;
    void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        ScopedNoDenormals guard;
        auto audio = getBusBuffer(buffer, false, 0);
        auto side = getBusBuffer(buffer, true, 1);
        const bool useExternal = external->load() > .5f && side.getNumChannels() > 0;
        const auto channels = audio.getNumChannels();
        const auto threshold = value(0), ratio = value(1), knee = value(4), range = value(5);
        const auto attack = nx::smoothing(value(2), sampleRate);
        const auto release = nx::smoothing(value(3), sampleRate);
        const auto fastRelease = nx::smoothing(value(3) * .45f, sampleRate);
        const auto holdSamples = static_cast<int>(value(6) * .001 * sampleRate);
        const auto filter = static_cast<float>(std::exp(-MathConstants<double>::twoPi * value(9) / sampleRate));
        const auto link = value(10) * .01f;
        const bool adapt = autoRelease->load() > .5f;
        const bool monitor = listen->load() > .5f;
        const bool dry = bypass->load() > .5f;
        mix.setTargetValue(value(8) * .01f);
        makeup.setTargetValue(nx::gain(value(7)));
        float inPeak = 0, outPeak = 0, maximumReduction = 0;

        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            std::array<float, 2> detectors {};
            float linked = 0;
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto source = useExternal ? side.getSample(std::min(ch, side.getNumChannels() - 1), i)
                                                : audio.getSample(ch, i);
                auto& low = followers[static_cast<size_t>(ch)].low;
                low = source + filter * (low - source);
                detectors[static_cast<size_t>(ch)] = source - low;
                linked = std::max(linked, std::abs(source - low));
            }
            const auto wet = mix.getNextValue();
            const auto outputGain = makeup.getNextValue();
            for (int ch = 0; ch < channels; ++ch)
            {
                const auto index = static_cast<size_t>(ch);
                auto& follower = followers[index];
                const auto detector = std::abs(detectors[index]);
                const auto target = nx::reduction(nx::decibels(detector + link * (linked - detector)),
                                                  threshold, ratio, knee, range);
                if (target > follower.reduction)
                {
                    follower.reduction = target + attack * (follower.reduction - target);
                    follower.hold = holdSamples;
                }
                else if (follower.hold > 0)
                    --follower.hold;
                else
                {
                    const auto coefficient = adapt && follower.reduction < 8.f ? fastRelease : release;
                    follower.reduction = target + coefficient * (follower.reduction - target);
                }
                const auto input = audio.getSample(ch, i);
                const auto output = dry ? input : monitor ? detectors[index]
                    : input * ((1.f - wet) + wet * outputGain * nx::gain(-follower.reduction));
                audio.setSample(ch, i, output);
                inPeak = std::max(inPeak, std::abs(input));
                outPeak = std::max(outPeak, std::abs(output));
                maximumReduction = std::max(maximumReduction, follower.reduction);
            }
        }
        inputMeter = nx::decibels(inPeak);
        outputMeter = nx::decibels(outPeak);
        reductionMeter = dry ? 0.f : maximumReduction;
    }

    void getStateInformation(MemoryBlock& block) override
    {
        auto snapshot = state.copyState();
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
            state.replaceState(snapshot);
        }
    }
    float value(size_t i) const noexcept { return parameters[i]->load(); }

    State state;
    std::atomic<float> inputMeter {-100}, outputMeter {-100}, reductionMeter {0};
private:
    struct Follower { float low = 0, reduction = 0; int hold = 0; };
    std::array<Follower, 2> followers {};
    std::array<std::atomic<float>*, controls.size()> parameters {};
    std::atomic<float>* external = nullptr;
    std::atomic<float>* listen = nullptr;
    std::atomic<float>* autoRelease = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<int> program {0};
    SmoothedValue<float> mix, makeup;
    double sampleRate = 44100;
};

const Colour ink {0xffe7eef5};
const Colour muted {0xffa3b2c4};
const Colour teal {0xff54dec9};
const Colour amber {0xffffc175};

class DialLook final : public LookAndFeel_V4
{
public:
    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float value,
                          float start, float end, Slider&) override
    {
        const auto radius = std::min(width, height) * .38f;
        const Point<float> centre {x + width * .5f, y + height * .5f};
        Rectangle<float> face {centre.x - radius, centre.y - radius, radius * 2, radius * 2};
        g.setColour(Colours::black.withAlpha(.48f));
        g.fillEllipse(face.translated(0, 3));
        g.setGradientFill(ColourGradient(Colour(0xff2a394b), face.getTopLeft(),
                                         Colour(0xff131d2b), face.getBottomRight(), false));
        g.fillEllipse(face);
        g.setColour(Colour(0xff526375));
        g.drawEllipse(face, 1.f);
        const auto angle = start + value * (end - start);
        Path track, progress;
        track.addCentredArc(centre.x, centre.y, radius + 4, radius + 4, 0, start, end, true);
        progress.addCentredArc(centre.x, centre.y, radius + 4, radius + 4, 0, start, angle, true);
        const PathStrokeType stroke {2.5f, PathStrokeType::curved, PathStrokeType::rounded};
        g.setColour(Colour(0xff354656));
        g.strokePath(track, stroke);
        g.setColour(teal);
        g.strokePath(progress, stroke);
        const Point<float> direction {std::sin(angle), -std::cos(angle)};
        g.drawLine({centre + direction * (radius * .57f), centre + direction * (radius * .84f)}, 2.f);
    }
};

class Editor final : public AudioProcessorEditor, private Timer
{
public:
    explicit Editor(Processor& processor) : AudioProcessorEditor(processor), owner(processor)
    {
        addAndMakeVisible(panel);
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto& slider = sliders[i];
            const auto& control = controls[i];
            slider.setName(control.label);
            slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(Slider::TextBoxBelow, false, 66, 21);
            slider.setTextValueSuffix(control.unit);
            slider.setDoubleClickReturnValue(true, control.initial);
            slider.setLookAndFeel(&dialLook);
            slider.setColour(Slider::textBoxTextColourId, ink);
            slider.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
            slider.setColour(Slider::textBoxBackgroundColourId, Colour(0x50111b29));
            panel.addAndMakeVisible(slider);
            attachments[i] = std::make_unique<State::SliderAttachment>(owner.state, control.id, slider);
        }

        for (auto* button : {&presetButton, &previous, &next, &external, &listen, &autoRelease, &bypass})
        {
            button->setColour(TextButton::buttonColourId, Colour(0x88517388));
            button->setColour(TextButton::buttonOnColourId, Colour(0xff26645d));
            button->setColour(TextButton::textColourOffId, ink);
            button->setColour(TextButton::textColourOnId, ink);
            panel.addAndMakeVisible(*button);
        }
        previous.setTooltip("Previous preset");
        next.setTooltip("Next preset");
        previous.onClick = [this] { changePreset((owner.getCurrentProgram() + owner.getNumPrograms() - 1) % owner.getNumPrograms()); };
        next.onClick = [this] { changePreset((owner.getCurrentProgram() + 1) % owner.getNumPrograms()); };
        presetButton.onClick = [this] { openPresets(); };
        externalAttachment = bindToggle("external", external);
        listenAttachment = bindToggle("listen", listen);
        releaseAttachment = bindToggle("autoRelease", autoRelease);
        bypassAttachment = bindToggle("bypass", bypass);
        updatePresetName();
        setResizable(true, true);
        setResizeLimits(700, 467, 7680, 5120);
        if (auto* constrainer = getConstrainer())
            constrainer->setFixedAspectRatio(1.5);
        setSize(960, 640);
        startTimerHz(30);
    }
    ~Editor() override
    {
        stopTimer();
        for (auto& slider : sliders)
            slider.setLookAndFeel(nullptr);
    }
    void paint(Graphics& g) override { g.fillAll(Colour(0xff0c121b)); }
    void resized() override
    {
        panel.setTransform(AffineTransform());
        panel.setBounds(0, 0, 960, 640);
        panel.setTransform(AffineTransform::scale(getWidth() / 960.f, getHeight() / 640.f));

        previous.setBounds(556, 26, 31, 32);
        presetButton.setBounds(595, 26, 192, 32);
        next.setBounds(795, 26, 31, 32);
        bypass.setBounds(844, 26, 88, 32);
        external.setBounds(34, 591, 117, 27);
        listen.setBounds(159, 591, 98, 27);
        autoRelease.setBounds(267, 591, 110, 27);
        for (int i = 0; i < 4; ++i)
            sliders[static_cast<size_t>(i)].setBounds(35 + i * 152, 329, 135, 112);
        for (int i = 4; i < static_cast<int>(controls.size()); ++i)
            sliders[static_cast<size_t>(i)].setBounds(33 + (i - 4) * 128, 499, 122, 67);
    }

private:
    using ButtonAttachment = State::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> bindToggle(const char* id, TextButton& button)
    {
        button.setClickingTogglesState(true);
        return std::make_unique<ButtonAttachment>(owner.state, id, button);
    }
    void changePreset(int index)
    {
        owner.setCurrentProgram(index);
        updatePresetName();
    }
    void updatePresetName()
    {
        const auto name = owner.getProgramName(owner.getCurrentProgram());
        if (presetButton.getButtonText() != name)
            presetButton.setButtonText(name);
    }
    void openPresets()
    {
        PopupMenu menu;
        for (int first = 0; first < owner.getNumPrograms();)
        {
            const String group = presets[first].group;
            PopupMenu category;
            auto index = first;
            while (index < owner.getNumPrograms() && group == presets[index].group)
            {
                category.addItem(index + 1, presets[index].name, true, index == owner.getCurrentProgram());
                ++index;
            }
            menu.addSubMenu(group, category);
            first = index;
        }
        const Component::SafePointer<Editor> safe(this);
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(presetButton), [safe](int result)
        {
            if (safe != nullptr && result > 0)
                safe->changePreset(result - 1);
        });
    }
    void timerCallback() override
    {
        updatePresetName();
        panel.cursor = (panel.cursor + 1) % panel.input.size();
        panel.input[panel.cursor] = owner.inputMeter.load();
        panel.output[panel.cursor] = owner.outputMeter.load();
        panel.reduction[panel.cursor] = owner.reductionMeter.load();
        panel.repaint();
    }

    struct Panel final : Component
    {
        explicit Panel(Processor& processor) : owner(processor)
        {
            input.fill(-72.f);
            output.fill(-72.f);
        }
        void paint(Graphics& g) override
        {
            const Rectangle<float> bounds {0, 0, 960, 640};
            g.setGradientFill(ColourGradient(Colour(0xff23364a), bounds.getTopLeft(),
                                             Colour(0xff101723), bounds.getBottomRight(), false));
            g.fillAll();
            g.setColour(Colour(0x71162433));
            g.fillRect(0, 0, 960, 79);
            g.setColour(teal.withAlpha(.17f));
            g.drawHorizontalLine(78, 0, 960);
            g.setColour(amber.withAlpha(.15f));
            g.fillRoundedRectangle(28, 22, 54, 36, 8);
            g.setColour(amber);
            g.drawRoundedRectangle(28, 22, 54, 36, 8, 1);
            label(g, "NX", {28, 22, 54, 36}, amber, 22, Justification::centred);
            label(g, "PRO C 3", {94, 17, 230, 31}, ink, 27);
            label(g, "NX AUDIO  /  DYNAMICS", {96, 49, 260, 16}, muted, 11);

            card(g, {26, 94, 645, 183});
            card(g, {687, 94, 247, 353});
            card(g, {26, 289, 645, 158});
            card(g, {26, 459, 908, 116});
            label(g, "SIGNAL HISTORY", {43, 104, 220, 22}, ink, 13);
            label(g, "INPUT", {449, 104, 56, 22}, muted, 11);
            label(g, "OUTPUT", {503, 104, 64, 22}, teal, 11);
            label(g, "REDUCTION", {566, 104, 91, 22}, amber, 11);
            label(g, "DYNAMICS", {44, 301, 150, 20}, ink, 13);
            label(g, "TRANSFER", {707, 104, 155, 22}, ink, 13);
            label(g, "LEVELS", {707, 307, 140, 22}, ink, 13);
            label(g, "SHAPING & DETECTOR", {43, 467, 260, 20}, ink, 13);

            drawHistory(g, {44, 137, 607, 121});
            drawTransfer(g, {711, 135, 199, 153});
            meter(g, "IN", owner.inputMeter.load(), {710, 337, 200, 24}, muted, false);
            meter(g, "OUT", owner.outputMeter.load(), {710, 368, 200, 24}, teal, false);
            meter(g, "GR", owner.reductionMeter.load(), {710, 399, 200, 24}, amber, true);
            for (int i = 0; i < 4; ++i)
                label(g, controls[static_cast<size_t>(i)].label, {35 + i * 152, 319, 135, 16},
                      muted, 12, Justification::centred);
            for (int i = 4; i < static_cast<int>(controls.size()); ++i)
                label(g, controls[static_cast<size_t>(i)].label, {33 + (i - 4) * 128, 485, 122, 15},
                      muted, 11, Justification::centred);
        }
        static void label(Graphics& g, const String& text, Rectangle<int> area, Colour colour,
                          float size, Justification alignment = Justification::centredLeft)
        {
            g.setColour(colour);
            g.setFont(Font(FontOptions(size)));
            g.drawFittedText(text, area, alignment, 1);
        }
        static void card(Graphics& g, Rectangle<float> area)
        {
            g.setColour(Colour(0x40060a12));
            g.fillRoundedRectangle(area.translated(0, 3), 12);
            g.setColour(Colour(0x92101824));
            g.fillRoundedRectangle(area, 12);
            g.setColour(Colour(0x485c839a));
            g.drawRoundedRectangle(area, 12, 1);
        }
        static void grid(Graphics& g, Rectangle<float> area, int columns, int rows)
        {
            g.setColour(Colour(0x335f8299));
            for (int x = 0; x <= columns; ++x)
                g.drawVerticalLine(roundToInt(area.getX() + area.getWidth() * x / columns), area.getY(), area.getBottom());
            for (int y = 0; y <= rows; ++y)
                g.drawHorizontalLine(roundToInt(area.getY() + area.getHeight() * y / rows), area.getX(), area.getRight());
        }
        void drawHistory(Graphics& g, Rectangle<float> area)
        {
            grid(g, area, 12, 4);
            const auto trace = [&](const auto& data, Colour colour, bool gainReduction)
            {
                Path path;
                for (size_t i = 0; i < data.size(); ++i)
                {
                    const auto value = data[(cursor + 1 + i) % data.size()];
                    const auto x = area.getX() + area.getWidth() * static_cast<float>(i) / (data.size() - 1);
                    const auto position = gainReduction ? value / 36.f : -value / 72.f;
                    const auto y = area.getY() + area.getHeight() * jlimit(0.f, 1.f, position);
                    if (i == 0) path.startNewSubPath(x, y);
                    else path.lineTo(x, y);
                }
                g.setColour(colour);
                g.strokePath(path, PathStrokeType(1.5f));
            };
            trace(input, muted.withAlpha(.55f), false);
            trace(output, teal, false);
            trace(reduction, amber, true);
        }
        void drawTransfer(Graphics& g, Rectangle<float> area)
        {
            grid(g, area, 6, 6);
            g.setColour(muted.withAlpha(.3f));
            g.drawLine({area.getBottomLeft(), area.getTopRight()});
            Path curve;
            for (int i = 0; i <= 100; ++i)
            {
                const auto level = -60.f + .6f * i;
                const auto outputDb = level - nx::reduction(level, owner.value(0), owner.value(1),
                                                           owner.value(4), owner.value(5));
                const auto x = area.getX() + area.getWidth() * i / 100.f;
                const auto y = area.getY() + area.getHeight() * jlimit(0.f, 1.f, -outputDb / 60.f);
                if (i == 0) curve.startNewSubPath(x, y);
                else curve.lineTo(x, y);
            }
            g.setColour(teal);
            g.strokePath(curve, PathStrokeType(2));
        }
        static void meter(Graphics& g, const char* name, float value, Rectangle<int> area,
                          Colour colour, bool gainReduction)
        {
            label(g, name, area.removeFromLeft(35), muted, 11);
            label(g, String(value, 1), area.removeFromRight(41), colour, 11, Justification::centredRight);
            auto track = area.toFloat().reduced(0, 8);
            g.setColour(Colour(0xff243649));
            g.fillRoundedRectangle(track, 3);
            const auto amount = gainReduction ? value / 36.f : (value + 72.f) / 72.f;
            g.setColour(colour);
            g.fillRoundedRectangle(track.withWidth(track.getWidth() * jlimit(0.f, 1.f, amount)), 3);
        }

        Processor& owner;
        std::array<float, 160> input {}, output {}, reduction {};
        size_t cursor = 0;
    };

    Processor& owner;
    Panel panel {owner};
    DialLook dialLook;
    std::array<Slider, controls.size()> sliders;
    std::array<std::unique_ptr<State::SliderAttachment>, controls.size()> attachments;
    TextButton presetButton, previous {"<"}, next {">"}, external {"External key"},
        listen {"Listen"}, autoRelease {"Auto release"}, bypass {"Bypass"};
    std::unique_ptr<ButtonAttachment> externalAttachment, listenAttachment, releaseAttachment, bypassAttachment;
};

AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Processor(); }
