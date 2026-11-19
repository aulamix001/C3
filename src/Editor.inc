namespace panelStyle
{
const Colour ink {0xffd4d0d5};
const Colour muted {0xff96919d};
const Colour metal {0xff68646b};
const Colour gold {0xffffbe2e};
const Colour light {0xffffd65c};
const Colour coral {0xfff36b6f};
const Colour blue {0xffa8c3d5};

void caption(Graphics& g, const String& text, Rectangle<int> area, Colour colour, float size,
             Justification alignment = Justification::centredLeft)
{
    g.setFont(Font(FontOptions(size)));
    g.setColour(colour);
    g.drawFittedText(text, area, alignment, 1);
}

class HardwareLook final : public LookAndFeel_V4
{
public:
    HardwareLook()
    {
        setColour(TextEditor::textColourId, ink);
        setColour(TextEditor::backgroundColourId, Colour(0xff201e24));
        setColour(TextEditor::focusedOutlineColourId, gold.withAlpha(.65f));
        setColour(PopupMenu::backgroundColourId, Colour(0xff222027));
        setColour(PopupMenu::textColourId, ink);
        setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff574423));
        setColour(PopupMenu::highlightedTextColourId, light);
    }

    Slider::SliderLayout getSliderLayout(Slider& slider) override
    {
        if (!slider.getProperties().contains("trim"))
            return LookAndFeel_V4::getSliderLayout(slider);
        auto area = slider.getLocalBounds();
        auto graphic = area.removeFromLeft(33);
        return {graphic, area.withSizeKeepingCentre(area.getWidth(), 17)};
    }

    void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float amount,
                          float start, float end, Slider& slider) override
    {
        const bool compact = slider.getProperties().contains("trim");
        const auto radius = std::max(2.0f, std::min(w, h) * .5f - (compact ? 4.f : 8.f));
        if (slider.getProperties().contains("dry"))
            amount = 1.f - amount;
        const Point<float> centre {x + w * .5f, y + h * .5f};
        const auto face = Rectangle<float>(radius * 2, radius * 2).withCentre(centre);
        const bool highlighted = slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true);
        const auto angle = start + amount * (end - start);

        if (!compact)
        {
            for (int i = 4; i > 0; --i)
            {
                g.setColour(Colours::black.withAlpha(.06f * (5 - i)));
                g.fillEllipse(face.expanded(static_cast<float>(i)).translated(0, 2.5f));
            }
            const auto bevel = face.expanded(1.5f);
            g.setGradientFill(ColourGradient(Colour(0xff858087), bevel.getCentreX(), bevel.getY(),
                                             Colour(0xff151419), bevel.getCentreX(), bevel.getBottom(), false));
            g.fillEllipse(bevel);
            g.setGradientFill(ColourGradient(Colour(highlighted ? 0xff514c54 : 0xff433e46),
                centre.x - radius * .25f, centre.y - radius * .40f,
                Colour(0xff242228), centre.x + radius * .65f, centre.y + radius * .70f, true));
            g.fillEllipse(face.reduced(1.1f));
            Path rim;
            rim.addCentredArc(centre.x, centre.y, radius - 2, radius - 2, 0, -1.10f, .95f, true);
            g.setColour(Colour(0xffaaa4ab).withAlpha(.29f));
            g.strokePath(rim, PathStrokeType(.75f));
            g.setColour(Colours::black.withAlpha(.44f));
            g.drawEllipse(face, 1.0f);
        }

        const auto arcRadius = radius + (compact ? 1.0f : 4.5f);
        Path rail, arc;
        rail.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, end, true);
        arc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, start, angle, true);
        const auto stroke = compact ? 1.15f : 1.8f;
        g.setColour(metal.withAlpha(.54f));
        g.strokePath(rail, PathStrokeType(stroke));
        if (amount > .0001f)
        {
            g.setColour(gold.withAlpha(highlighted ? .27f : .16f));
            g.strokePath(arc, PathStrokeType(stroke + 3.2f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setGradientFill(ColourGradient(light, centre.x, face.getY(), gold,
                                             centre.x, face.getBottom(), false));
            g.strokePath(arc, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const Point<float> direction {std::sin(angle), -std::cos(angle)};
        const auto marker = centre + direction * (radius * (compact ? .64f : .71f));
        g.setColour(light.withAlpha(highlighted ? 1.0f : .85f));
        g.fillEllipse(Rectangle<float>(compact ? 2.2f : 3.0f, compact ? 2.2f : 3.0f).withCentre(marker));
    }

    void drawButtonBackground(Graphics& g, Button& button, const Colour&, bool hover, bool down) override
    {
        auto area = button.getLocalBounds().toFloat().reduced(.8f, 1.4f);
        if (down) area.translate(0, 1);
        const auto radius = button.getProperties().contains("capsule") ? area.getHeight() * .48f : 6.f;
        g.setColour(Colours::black.withAlpha(.50f));
        g.fillRoundedRectangle(area.translated(0, 1.4f), radius);
        const auto active = button.getToggleState();
        const Colour top {active ? 0xff55401e : hover ? 0xff47424b : 0xff343139};
        const Colour bottom {active ? 0xff302618 : down ? 0xff25232a : 0xff242228};
        g.setGradientFill(ColourGradient(down ? bottom : top, area.getTopLeft(), bottom, area.getBottomLeft(), false));
        g.fillRoundedRectangle(area, radius);
        g.setColour((active ? gold : metal).withAlpha(active ? .55f : .60f));
        g.drawRoundedRectangle(area, radius, .8f);
    }

    void drawButtonText(Graphics& g, TextButton& button, bool, bool down) override
    {
        auto bounds = button.getLocalBounds().reduced(4, 0);
        if (down) bounds.translate(0, 1);
        caption(g, button.getButtonText(), bounds, button.isEnabled()
            ? (button.getToggleState() ? light : ink) : muted.withAlpha(.65f),
            11.5f, Justification::centred);
    }
};

class PlateButton final : public Button
{
public:
    enum Icon { undo, redo, left, right, help, resize };
    PlateButton(const String& title, Icon symbol) : Button(title), icon(symbol) { setTooltip(title); }

    void paintButton(Graphics& g, bool hover, bool down) override
    {
        getLookAndFeel().drawButtonBackground(g, *this, {}, hover, down);
        const auto c = getLocalBounds().toFloat().getCentre() + Point<float>(0, down ? 1.f : 0.f);
        g.setColour((hover ? light : ink).withAlpha(isEnabled() ? .95f : .31f));
        Path path;
        if (icon == left || icon == right)
        {
            const auto sign = icon == left ? 1.f : -1.f;
            path.startNewSubPath(c.x + 2.5f * sign, c.y - 4);
            path.lineTo(c.x - 2 * sign, c.y);
            path.lineTo(c.x + 2.5f * sign, c.y + 4);
        }
        else if (icon == undo || icon == redo)
        {
            const auto sign = icon == undo ? 1.f : -1.f;
            path.startNewSubPath(c.x + 5 * sign, c.y + 3);
            path.quadraticTo(c.x + 5 * sign, c.y - 3, c.x - 4 * sign, c.y - 2);
            path.startNewSubPath(c.x - sign, c.y - 5);
            path.lineTo(c.x - 5 * sign, c.y - 2);
            path.lineTo(c.x - sign, c.y + 1);
        }
        else if (icon == resize)
        {
            path.startNewSubPath(c.x - 4, c.y + 4);
            path.lineTo(c.x + 4, c.y - 4);
            path.startNewSubPath(c.x - 4, c.y);
            path.lineTo(c.x - 4, c.y + 4);
            path.lineTo(c.x, c.y + 4);
            path.startNewSubPath(c.x, c.y - 4);
            path.lineTo(c.x + 4, c.y - 4);
            path.lineTo(c.x + 4, c.y);
        }
        else
        {
            g.drawEllipse(Rectangle<float>(13, 13).withCentre(c), 1.0f);
            caption(g, "?", getLocalBounds(), ink, 10, Justification::centred);
        }
        g.strokePath(path, PathStrokeType(1.25f, PathStrokeType::curved, PathStrokeType::rounded));
    }
private:
    Icon icon;
};

class Dial final : public Slider
{
public:
    void mouseDown(const MouseEvent& event) override
    {
        setMouseDragSensitivity(event.mods.isShiftDown() ? 1250 : 240);
        Slider::mouseDown(event);
    }
};
}

class Editor final : public AudioProcessorEditor, private Timer
{
    using HardwareLook = panelStyle::HardwareLook;
    using PlateButton = panelStyle::PlateButton;
    using ButtonAttachment = State::ButtonAttachment;
    static constexpr int designWidth = 920, designHeight = 622;
    static constexpr std::array<int, 13> dialX {95,224,498,604,105,239,595,705,719,455,732,490,584};

    struct Frame { float in = -100, out = -100, reduction = 0; };
    struct Panel final : Component
    {
        explicit Panel(Processor& processor) : owner(processor) {}
        void paint(Graphics& g) override
        {
            using namespace panelStyle;
            Path shell;
            shell.addRoundedRectangle(0, 0, designWidth, designHeight, 17.f);
            g.reduceClipRegion(shell);
            g.setGradientFill(ColourGradient(Colour(0xfa151418), 0, 0,
                                             Colour(0xef302c34), 0, designHeight, false));
            g.fillAll();
            g.setGradientFill(ColourGradient(Colour(0x395c4b5b), 735, 462,
                                             Colours::transparentBlack, 190, 230, true));
            g.fillAll();
            g.setColour(Colour(0xc8171719));
            g.fillRect(0, 0, designWidth, 64);
            g.setColour(Colours::white.withAlpha(.011f));
            for (int y = 65; y < designHeight; y += 3)
                g.drawHorizontalLine(y, 0, static_cast<float>(designWidth));

            const Rectangle<float> mark {20, 15, 47, 32};
            g.setGradientFill(ColourGradient(Colour(0x7a57401a), mark.getTopLeft(),
                                             Colour(0x65302519), mark.getBottomRight(), false));
            g.fillRoundedRectangle(mark, 8);
            g.setColour(gold.withAlpha(.85f));
            g.drawRoundedRectangle(mark, 8, 1);
            Path logo;
            logo.startNewSubPath(30,37); logo.lineTo(30,25); logo.lineTo(39,36); logo.lineTo(39,25);
            logo.startNewSubPath(45,25); logo.lineTo(56,37);
            logo.startNewSubPath(45,37); logo.lineTo(56,25);
            g.strokePath(logo, PathStrokeType(1.65f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(Colour(0xffead5b1));
            g.setFont(Font(FontOptions(14.f, Font::bold)));
            g.drawFittedText("NX Audio", {78, 19, 78, 26}, Justification::centredLeft, 1);
            g.setColour(gold);
            g.setFont(Font(FontOptions(Font::getDefaultSerifFontName(), 21.f, Font::bold)));
            g.drawFittedText("Pro C3", {158, 17, 100, 30}, Justification::centredLeft, 1);

            deck(g, {20, 68, 880, 461});
            deck(g, {20, 538, 880, 65});
            caption(g, "SIGNAL HISTORY", {37, 77, 180, 24}, ink, 12);
            caption(g, "INPUT", {481, 77, 60, 24}, muted, 10);
            caption(g, "OUTPUT", {541, 77, 68, 24}, blue, 10);
            caption(g, "REDUCTION", {609, 77, 85, 24}, coral, 10);
            drawHistory(g);
            const Rectangle<float> controlsPlate {30, 332, 706, 190};
            g.setGradientFill(ColourGradient(Colour(0x251c1a20), controlsPlate.getTopLeft(),
                                             Colour(0xeb17161b), controlsPlate.getBottomLeft(), false));
            g.fillRoundedRectangle(controlsPlate, 11.f);
            drawMeters(g);
            drawTransfer(g);
            caption(g, "SIDECHAIN", {34, 546, 110, 16}, gold, 11);
            caption(g, "AUTO GAIN", {684, 395, 70, 14},
                    owner.state.getRawParameterValue("autoGain")->load() > .5f ? gold : muted,
                    9, Justification::centred);

            const std::array<const char*, 13> names {"THRESHOLD", "RATIO", "ATTACK", "RELEASE",
                "KNEE", "RANGE", "HOLD", "MAKEUP", "DRY", "HP", "LINK", "LOOKAHEAD", "LP"};
            for (size_t i = 0; i < names.size(); ++i)
            {
                const auto y = i < 4 ? 461 : (i == 9 || i == 10 || i == 12) ? 544
                    : i == 8 ? 471 : 482;
                caption(g, names[i], {dialX[i] - 48, y, 96, 13}, muted, 9.5f, Justification::centred);
            }
            caption(g, "STYLE", {332, 374, 72, 16}, muted, 10, Justification::centred);
            const Rectangle<float> style {315, 395, 105, 26};
            g.setColour(Colours::black.withAlpha(.43f));
            g.fillRoundedRectangle(style.translated(0, 2), 12);
            g.setGradientFill(ColourGradient(Colour(0xff3c3740), style.getTopLeft(),
                                             Colour(0xff242128), style.getBottomLeft(), false));
            g.fillRoundedRectangle(style, 12);
            g.setColour(metal.withAlpha(.62f));
            g.drawRoundedRectangle(style, 12, .8f);
            caption(g, "CLEAN", style.toNearestInt(), ink, 11.5f, Justification::centred);
        }

        static void deck(Graphics& g, Rectangle<float> bounds)
        {
            g.setColour(Colours::black.withAlpha(.34f));
            g.fillRoundedRectangle(bounds.translated(0, 2), 13);
            g.setGradientFill(ColourGradient(Colour(0x9429252e), bounds.getTopLeft(),
                                             Colour(0xa217161b), bounds.getBottomLeft(), false));
            g.fillRoundedRectangle(bounds, 13);
            g.setColour(Colour(0xff68646b).withAlpha(.38f));
            g.drawRoundedRectangle(bounds, 13, .8f);
        }

        void drawHistory(Graphics& g) const
        {
            using namespace panelStyle;
            const Rectangle<float> chart {39, 108, 691, 400};
            const auto plotY = [this, chart](float db)
            {
                return chart.getY() + chart.getHeight() * jlimit(0.f, 1.f, -db / scale);
            };
            g.setGradientFill(ColourGradient(Colour(0xd017161a), chart.getTopLeft(),
                                             Colour(0xb5211e25), chart.getBottomLeft(), false));
            g.fillRect(chart);
            for (int db = 0; db <= scale; db += scale == 36 ? 3 : 6)
            {
                const auto y = plotY(static_cast<float>(-db));
                g.setColour((db == 0 ? coral : Colour(0xff68646b)).withAlpha(db == 0 ? .55f : .26f));
                g.drawLine(chart.getX(), y, chart.getRight(), y, .65f);
                caption(g, String(-db), {739, roundToInt(y) - 6, 25, 12}, muted, 9, Justification::right);
            }
            for (int x = 0; x <= 12; ++x)
            {
                g.setColour(muted.withAlpha(.13f));
                const auto px = chart.getX() + chart.getWidth() * x / 12.f;
                g.drawLine(px, chart.getY(), px, chart.getBottom(), .6f);
            }
            const auto thresholdY = plotY(owner.value(0));
            const float dash[] {4, 4};
            g.setColour(gold.withAlpha(.52f));
            g.drawDashedLine({chart.getX(), thresholdY, chart.getRight(), thresholdY}, dash, 2);
            const auto trace = [&](float Frame::*field, Colour colour, bool reduction, bool fill)
            {
                Path path;
                for (size_t i = 0; i < history.size(); ++i)
                {
                    const auto& frame = history[(cursor + 1 + i) % history.size()];
                    const auto x = chart.getX() + chart.getWidth() * static_cast<float>(i) / (history.size() - 1);
                    const auto y = plotY(reduction ? -(frame.*field) : frame.*field);
                    if (i == 0) path.startNewSubPath(x, y);
                    else path.lineTo(x, y);
                }
                if (fill)
                {
                    auto area = path;
                    area.lineTo(chart.getBottomRight());
                    area.lineTo(chart.getBottomLeft());
                    area.closeSubPath();
                    g.setGradientFill(ColourGradient(colour.withAlpha(.20f), chart.getTopLeft(),
                                                     colour.withAlpha(.025f), chart.getBottomLeft(), false));
                    g.fillPath(area);
                }
                g.setColour(colour);
                g.strokePath(path, PathStrokeType(reduction ? 1.55f : 1.0f));
            };
            trace(&Frame::in, muted.withAlpha(.72f), false, true);
            trace(&Frame::out, blue.withAlpha(.92f), false, false);
            trace(&Frame::reduction, coral, true, false);
        }

        void drawMeters(Graphics& g) const
        {
            using namespace panelStyle;
            const std::array<Colour, 3> colours {gold, blue, coral};
            const std::array<const char*, 3> names {"IN", "OUT", "GR"};
            for (size_t i = 0; i < colours.size(); ++i)
            {
                const auto x = 785 + static_cast<int>(i) * 36;
                const Rectangle<float> bounds {static_cast<float>(x), 108, 13, 384};
                g.setColour(Colour(0xff121115));
                g.fillRoundedRectangle(bounds.expanded(2, 1), 2);
                const auto scaleLevel = [this, i](float db)
                {
                    return jlimit(0.f, 1.f, i == 2 ? db / scale : 1.f + db / scale);
                };
                const auto level = scaleLevel(meters[i]);
                for (int segment = 0; segment < 75; ++segment)
                {
                    const auto fraction = static_cast<float>(segment) / 75;
                    g.setColour(fraction < level ? colours[i].withAlpha(.53f + .46f * fraction)
                                                 : Colour(0xff302e35));
                    g.fillRect(bounds.getX(), bounds.getBottom() - 4.0f - fraction * (bounds.getHeight() - 2), bounds.getWidth(), 3.7f);
                }
                const auto y = bounds.getBottom() - scaleLevel(peaks[i]) * bounds.getHeight();
                g.setColour(colours[i].brighter(.4f));
                g.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.2f);
                if (i < 2)
                {
                    g.setColour(clipTicks[i] > 0 ? coral : Colour(0xff43323a));
                    g.fillRoundedRectangle(bounds.getX(), 101, bounds.getWidth(), 3, 1);
                }
                caption(g, names[i], {x - 9, 499, 31, 16}, muted, 9, Justification::centred);
            }
        }

        void drawTransfer(Graphics& g) const
        {
            using namespace panelStyle;
            const Rectangle<float> tile {315, 467, 104, 49};
            g.setColour(Colour(0x99121015));
            g.fillRoundedRectangle(tile, 4);
            auto area = tile.reduced(5, 5);
            g.setColour(muted.withAlpha(.28f));
            g.drawLine({area.getBottomLeft(), area.getTopRight()}, .8f);
            Path curve;
            const auto threshold = owner.value(0);
            for (int i = 0; i <= 60; ++i)
            {
                const double input = static_cast<double>(i - 60);
                const auto output = input - nx::reduction(input, threshold, owner.value(1), owner.value(4), owner.value(5));
                const auto x = area.getX() + area.getWidth() * i / 60.f;
                const auto y = area.getBottom() - area.getHeight() * static_cast<float>((output + 60) / 60);
                if (i == 0) curve.startNewSubPath(x, y);
                else curve.lineTo(x, y);
            }
            g.setColour(gold);
            g.strokePath(curve, PathStrokeType(1.1f));
        }

        Processor& owner;
        std::array<Frame, 225> history {};
        std::array<float, 3> meters {-100, -100, 0}, peaks {-100, -100, 0};
        std::array<int, 3> peakTicks {};
        std::array<int, 2> clipTicks {};
        size_t cursor = 0;
        int scale = 72, holdFrames = 45;
    };

public:
    explicit Editor(Processor& processor) : AudioProcessorEditor(processor), owner(processor)
    {
        setOpaque(false);
        setLookAndFeel(&look);
        addAndMakeVisible(panel);
        for (size_t i = 0; i < controls.size(); ++i)
        {
            auto& dial = sliders[i];
            const auto& control = controls[i];
            dial.setName(i == 8 ? "Dry" : control.label);
            dial.setSliderStyle(Slider::RotaryVerticalDrag);
            dial.setRotaryParameters(3.78f, 8.78f, true);
            dial.setTextBoxStyle(Slider::TextBoxBelow, false, 71, 16);
            dial.setDoubleClickReturnValue(true, control.initial);
            dial.setColour(Slider::textBoxTextColourId, panelStyle::ink);
            dial.setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
            dial.setColour(Slider::textBoxBackgroundColourId, Colour(0x5718141c));
            if (i >= 4 && i != 8)
                dial.getProperties().set("trim", true);
            dial.onDragStart = [this] { owner.undo.beginNewTransaction(); };
            panel.addAndMakeVisible(dial);
            attachments[i] = std::make_unique<State::SliderAttachment>(owner.state, control.id, dial);
            if (i == 8)
            {
                dial.getProperties().set("dry", true);
                dial.textFromValueFunction = [](double wet) { return String(100 - wet, 0) + " %"; };
                dial.valueFromTextFunction = [](const String& dry) { return 100 - dry.getDoubleValue(); };
            }
            else
            {
                dial.textFromValueFunction = [control](double value)
                {
                    const int decimals = control.step < .1f ? 2 : control.step < 1.f ? 1 : 0;
                    return String(value, decimals) + control.unit;
                };
                dial.valueFromTextFunction = [](const String& text) { return text.getDoubleValue(); };
            }
            dial.updateText();
        }
        for (auto* button : std::array<Button*, 18> {&undo, &redo, &ab, &copy, &previous, &preset, &next,
             &help, &size, &bypass, &autoRelease, &autoGain, &external, &listen, &detector, &scale, &hold, &resetPeaks})
            panel.addAndMakeVisible(button);
        for (auto* button : {&preset, &autoGain, &autoRelease})
            button->getProperties().set("capsule", true);

        bypassAttachment = bind("bypass", bypass);
        releaseAttachment = bind("autoRelease", autoRelease);
        gainAttachment = bind("autoGain", autoGain);
        externalAttachment = bind("external", external);
        listenAttachment = bind("listen", listen);
        undo.onClick = [this] { owner.state.copyState(); owner.undo.undo(); };
        redo.onClick = [this] { owner.undo.redo(); };
        ab.onClick = [this] { switchSlot(); };
        copy.onClick = [this] { owner.getStateInformation(slots[static_cast<size_t>(1 - activeSlot)]); };
        owner.getStateInformation(slots[1]);
        previous.onClick = [this] { changePreset((owner.getCurrentProgram() + owner.getNumPrograms() - 1) % owner.getNumPrograms()); };
        next.onClick = [this] { changePreset((owner.getCurrentProgram() + 1) % owner.getNumPrograms()); };
        preset.onClick = [this] { openPresets(); };
        detector.onClick = [this]
        {
            owner.undo.beginNewTransaction();
            auto& parameter = *owner.state.getParameter("detector");
            parameter.beginChangeGesture();
            parameter.setValueNotifyingHost(1.f - parameter.getValue());
            parameter.endChangeGesture();
        };
        const auto selectSize = [this](int choice)
        {
            const int width = choice == 1 ? 760 : choice == 2 ? designWidth : 1280;
            setSize(width, roundToInt(static_cast<double>(width) * designHeight / designWidth));
        };
        size.onClick = [this, selectSize] { showChoices(size, {"Compact", "Normal", "Large"}, selectSize); };
        scale.onClick = [this] { showChoices(scale, {"36 dB", "48 dB", "72 dB"}, [this](int id)
        {
            panel.scale = id == 1 ? 36 : id == 2 ? 48 : 72;
            scale.setButtonText(String(panel.scale) + " dB");
        }); };
        hold.onClick = [this] { showChoices(hold, {"Peak hold off", "Peak hold 1.5 s", "Peak hold 3 s"}, [this](int id)
        {
            panel.holdFrames = id == 1 ? 0 : id == 2 ? 45 : 90;
            panel.peakTicks.fill(0);
            hold.setButtonText(id == 1 ? "PEAK OFF" : id == 2 ? "PEAK 1.5s" : "PEAK 3s");
        }); };
        resetPeaks.onClick = [this] { panel.clipTicks.fill(0); panel.peakTicks.fill(0); panel.peaks = panel.meters; };
        help.onClick = [] { AlertWindow::showMessageBoxAsync(MessageBoxIconType::InfoIcon, "NX PRO C 3",
            "Drag vertically. Hold Shift before dragging for fine adjustment. Double-click to reset or click a value to type.\n\n"
            "Route the auxiliary VST3 input and enable EXT for external sidechain. HP and LP filter the detector. "
            "Lookahead previews up to 10 ms. The plug-in always reports 10 ms of latency to keep dry and wet aligned."); };
        external.setTooltip("Use the auxiliary sidechain input when routed");
        listen.setTooltip("Audition the filtered detector signal");
        autoGain.setTooltip("Up to +6 dB of reference-based compensation");
        setResizable(true, true);
        setResizeLimits(690, 466, 7680, 5193);
        if (auto* constrainer = getConstrainer())
            constrainer->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
        setSize(designWidth, designHeight);
        updateControls();
        startTimerHz(30);
    }

    ~Editor() override
    {
        stopTimer();
        setLookAndFeel(nullptr);
    }

    void resized() override
    {
        panel.setTransform({});
        panel.setBounds(0, 0, designWidth, designHeight);
        panel.setTransform(AffineTransform::scale(getWidth() / static_cast<float>(designWidth),
                                                  getHeight() / static_cast<float>(designHeight)));
        undo.setBounds(262, 20, 27, 24); redo.setBounds(294, 20, 27, 24);
        ab.setBounds(333, 20, 41, 24); copy.setBounds(379, 20, 69, 24);
        previous.setBounds(480, 18, 27, 28); preset.setBounds(512, 18, 181, 28);
        next.setBounds(698, 18, 27, 28);
        bypass.setBounds(743, 20, 70, 24); help.setBounds(826, 20, 28, 24); size.setBounds(866, 20, 29, 24);
        scale.setBounds(697, 78, 58, 23); hold.setBounds(764, 77, 71, 23); resetPeaks.setBounds(842, 77, 48, 23);
        autoRelease.setBounds(566, 335, 77, 22);
        autoGain.setBounds(700, 356, 38, 38);
        external.setBounds(34, 567, 67, 23); listen.setBounds(107, 567, 67, 23);
        detector.setBounds(183, 567, 79, 23);
        for (size_t i = 0; i < sliders.size(); ++i)
        {
            const auto x = dialX[i];
            if (i < 4)
                sliders[i].setBounds(x - 51, 363, 102, 96);
            else if (i == 8)
                sliders[i].setBounds(x - 32, 408, 64, 61);
            else if (i == 9 || i == 10 || i == 12)
                sliders[i].setBounds(x - 46, 563, 92, 28);
            else
                sliders[i].setBounds(x - 46, 497, 92, 28);
        }
    }

private:
    std::unique_ptr<ButtonAttachment> bind(const char* id, TextButton& button)
    {
        button.setClickingTogglesState(true);
        button.onClick = [this] { owner.undo.beginNewTransaction(); };
        return std::make_unique<ButtonAttachment>(owner.state, id, button);
    }
    void switchSlot()
    {
        owner.getStateInformation(slots[static_cast<size_t>(activeSlot)]);
        activeSlot = 1 - activeSlot;
        const auto& snapshot = slots[static_cast<size_t>(activeSlot)];
        owner.applySnapshot(snapshot.getData(), static_cast<int>(snapshot.getSize()));
        ab.setButtonText(activeSlot == 0 ? "A / B" : "B / A");
    }
    void changePreset(int index)
    {
        owner.state.copyState();
        owner.undo.beginNewTransaction();
        owner.setCurrentProgram(index);
        owner.state.copyState();
        updateControls();
    }
    void updateControls()
    {
        const auto name = owner.getProgramName(owner.getCurrentProgram());
        if (preset.getButtonText() != name) preset.setButtonText(name);
        detector.setButtonText(owner.state.getRawParameterValue("detector")->load() > .5f ? "RMS" : "PEAK");
        undo.setEnabled(owner.undo.canUndo());
        redo.setEnabled(owner.undo.canRedo());
    }
    template<typename Callback>
    void showChoices(Button& target, std::initializer_list<const char*> choices, Callback callback)
    {
        PopupMenu menu;
        int id = 0;
        for (const auto* text : choices) menu.addItem(++id, text);
        showMenu(menu, target, std::move(callback));
    }
    template<typename Callback>
    void showMenu(PopupMenu& menu, Button& target, Callback callback)
    {
        menu.setLookAndFeel(&look);
        const Component::SafePointer<Editor> safe(this);
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(target), [safe, callback](int id)
        {
            if (safe != nullptr && id > 0) callback(id);
        });
    }
    void openPresets()
    {
        PopupMenu menu;
        for (int start = 0; start < owner.getNumPrograms();)
        {
            const String name = presets[start].group;
            PopupMenu group;
            int i = start;
            for (; i < owner.getNumPrograms() && name == presets[i].group; ++i)
                group.addItem(i + 1, presets[i].name, true, i == owner.getCurrentProgram());
            menu.addSubMenu(name, group);
            start = i;
        }
        showMenu(menu, preset, [this](int id) { changePreset(id - 1); });
    }
    void timerCallback() override
    {
        updateControls();
        const auto in = owner.inputPeak.exchange(-100.f), out = owner.outputPeak.exchange(-100.f);
        const auto gr = owner.reductionPeak.exchange(0.f);
        const std::array<float, 3> values {in, out, gr};
        for (size_t i = 0; i < values.size(); ++i)
        {
            panel.meters[i] = std::max(values[i], panel.meters[i] - 0.8f);
            if (values[i] >= panel.peaks[i])
            {
                panel.peaks[i] = values[i];
                panel.peakTicks[i] = panel.holdFrames;
            }
            else if (panel.peakTicks[i] > 0)
                --panel.peakTicks[i];
            else
                panel.peaks[i] = std::max(panel.meters[i], panel.peaks[i] - 0.8f);
        }
        const std::array<bool, 2> clips {owner.inputClip.exchange(false), owner.outputClip.exchange(false)};
        for (size_t i = 0; i < clips.size(); ++i)
            panel.clipTicks[i] = clips[i] ? 30 : std::max(0, panel.clipTicks[i] - 1);
        panel.cursor = (panel.cursor + 1) % panel.history.size();
        panel.history[panel.cursor] = {panel.meters[0], panel.meters[1], panel.meters[2]};
        panel.repaint(20, 68, 880, 461);
    }

    Processor& owner;
    HardwareLook look;
    Panel panel {owner};
    std::array<panelStyle::Dial, controls.size()> sliders;
    std::array<std::unique_ptr<State::SliderAttachment>, controls.size()> attachments;
    PlateButton undo {"Undo", PlateButton::undo}, redo {"Redo", PlateButton::redo},
        previous {"Previous preset", PlateButton::left}, next {"Next preset", PlateButton::right},
        help {"Help", PlateButton::help}, size {"Window size", PlateButton::resize};
    TextButton ab {"A / B"}, copy {"COPY A/B"}, preset, bypass {"BYPASS"},
        autoRelease {"AUTO RELEASE"}, autoGain {"AG"}, external {"EXT"}, listen {"LISTEN"},
        detector {"PEAK"}, scale {"72 dB"}, hold {"PEAK 1.5s"}, resetPeaks {"RESET"};
    std::unique_ptr<ButtonAttachment> bypassAttachment, releaseAttachment, gainAttachment, externalAttachment, listenAttachment;
    std::array<MemoryBlock, 2> slots;
    int activeSlot = 0;
};
