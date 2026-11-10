// Zarya Synth - retro hardware look: metal panels, chrome knobs, keycap buttons, green LCD screens.
#pragma once

#include <JuceHeader.h>

namespace retro
{
namespace colours
{
const juce::Colour chassis   (0xff34372f);
const juce::Colour metal     (0xffc4c1b2);
const juce::Colour metalDark (0xff9d9a8b);
const juce::Colour metalLite (0xffe6e3d6);
const juce::Colour engraved  (0xff26271f);
const juce::Colour screen    (0xff07100c);
const juce::Colour phosphor  (0xff5dffa8);
const juce::Colour cyan      (0xff52e6dc);
const juce::Colour amber     (0xffff8c1a);
const juce::Colour red       (0xffff4040);
const juce::Colour keycap    (0xff4b4e53);
} // namespace colours

inline juce::Font mono (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font sans (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// Brushed metal plate with bevelled edges and optional corner screws.
inline void drawPlate (juce::Graphics& g, juce::Rectangle<float> r, bool screws = true, float corner = 4.0f)
{
    using namespace colours;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.5f, 2.0f), corner);
    g.setGradientFill (juce::ColourGradient (metalLite, r.getX(), r.getY(), metalDark, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);

    // Fine horizontal brushing.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (float y = r.getY() + 3.0f; y < r.getBottom() - 2.0f; y += 3.0f)
        g.drawHorizontalLine ((int) y, r.getX() + 2.0f, r.getRight() - 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.drawLine (r.getX() + corner, r.getY() + 0.5f, r.getRight() - corner, r.getY() + 0.5f, 1.0f);
    g.drawLine (r.getX() + 0.5f, r.getY() + corner, r.getX() + 0.5f, r.getBottom() - corner, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    if (! screws) return;
    const float s = 7.0f;
    for (auto c : { r.getTopLeft() + juce::Point<float> (7, 7), r.getTopRight() + juce::Point<float> (-7, 7),
                    r.getBottomLeft() + juce::Point<float> (7, -7), r.getBottomRight() + juce::Point<float> (-7, -7) })
    {
        auto sr = juce::Rectangle<float> (s, s).withCentre (c);
        g.setGradientFill (juce::ColourGradient (metalLite, sr.getX(), sr.getY(), juce::Colour (0xff5d5b52), sr.getRight(), sr.getBottom(), false));
        g.fillEllipse (sr);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (sr, 0.8f);
        g.drawLine (sr.getX() + 1.5f, sr.getBottom() - 1.5f, sr.getRight() - 1.5f, sr.getY() + 1.5f, 1.0f);
    }
}

// Dark recessed screen with an inner shadow; returns the usable inner area.
inline juce::Rectangle<float> drawScreen (juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f)
{
    using namespace colours;
    g.setColour (juce::Colour (0xff2a2b25));
    g.fillRoundedRectangle (r, corner + 1.0f);
    auto inner = r.reduced (3.0f);
    g.setColour (screen);
    g.fillRoundedRectangle (inner, corner);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (r.getX() + 2.0f, r.getBottom() - 0.5f, r.getRight() - 2.0f, r.getBottom() - 0.5f, 1.0f);
    return inner;
}

// CRT scanlines and a soft vignette on top of screen content.
inline void drawScanlines (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    for (float y = r.getY(); y < r.getBottom(); y += 3.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
    juce::ColourGradient v (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(),
                            juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), true);
    g.setGradientFill (v);
    g.fillRect (r);
}

inline void drawKeycap (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down, juce::Colour body)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.5f), 6.0f);
    if (down) r = r.translated (0.0f, 1.5f);
    if (over) body = body.brighter (0.12f);

    g.setGradientFill (juce::ColourGradient (body.brighter (0.25f), r.getX(), r.getY(), body.darker (0.6f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    auto top = r.withTrimmedLeft (4.0f).withTrimmedRight (4.0f).withTrimmedTop (2.0f).withTrimmedBottom (5.0f);
    g.setGradientFill (juce::ColourGradient (body.brighter (0.35f), top.getX(), top.getY(), body.brighter (0.05f), top.getX(), top.getBottom(), false));
    g.fillRoundedRectangle (top, 4.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        using namespace colours;
        setColour (juce::Label::textColourId, engraved);
        setColour (juce::Slider::textBoxTextColourId, phosphor);
        setColour (juce::Slider::textBoxBackgroundColourId, screen);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2a2b25));
        setColour (juce::Slider::textBoxHighlightColourId, phosphor.withAlpha (0.35f));
        setColour (juce::TextEditor::textColourId, phosphor);
        setColour (juce::TextEditor::backgroundColourId, screen);
        setColour (juce::TextEditor::highlightColourId, phosphor.withAlpha (0.35f));
        setColour (juce::CaretComponent::caretColourId, phosphor);
        setColour (juce::ComboBox::textColourId, phosphor);
        setColour (juce::PopupMenu::backgroundColourId, screen);
        setColour (juce::PopupMenu::textColourId, phosphor);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, phosphor.withAlpha (0.8f));
        setColour (juce::PopupMenu::highlightedTextColourId, screen);
        setColour (juce::PopupMenu::headerTextColourId, cyan);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, amber);
        setColour (juce::TextButton::buttonColourId, keycap);
        setColour (juce::ScrollBar::thumbColourId, metalDark);
        setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TabbedComponent::backgroundColourId, chassis);
        setColour (juce::ResizableWindow::backgroundColourId, chassis);
        setColour (juce::AlertWindow::backgroundColourId, chassis);
        setColour (juce::AlertWindow::textColourId, metalLite);
        setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe9e5d6));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff2b2c2e));
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6d6a5e));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, amber.withAlpha (0.25f));
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, amber.withAlpha (0.75f));
        setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.4f));
        setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, keycap);
        setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace colours;
        auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
        const float radius = std::min (b.getWidth(), b.getHeight()) * 0.5f - 1.0f;
        const auto c = b.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Orange LED ring marking the value.
        const int dots = 11;
        for (int i = 0; i < dots; ++i)
        {
            const float t = (float) i / (float) (dots - 1);
            const float a = startAngle + t * (endAngle - startAngle);
            const auto p = c.getPointOnCircumference (radius - 2.0f, a);
            const bool lit = s.isEnabled() && t <= pos + 0.001f;
            g.setColour (lit ? amber : juce::Colour (0xff4a3a2a));
            g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (p));
            if (lit)
            {
                g.setColour (amber.withAlpha (0.25f));
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            }
        }

        const float kr = radius * 0.74f;
        auto knob = juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (knob.translated (1.5f, 2.5f).expanded (1.0f));

        // Knurled skirt.
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2ee), knob.getX(), knob.getY(),
                                                 juce::Colour (0xff4c4e50), knob.getRight(), knob.getBottom(), false));
        g.fillEllipse (knob);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (int i = 0; i < 36; ++i)
        {
            const float a = (float) i / 36.0f * juce::MathConstants<float>::twoPi;
            g.drawLine (juce::Line<float> (c.getPointOnCircumference (kr * 0.86f, a), c.getPointOnCircumference (kr, a)), 0.8f);
        }

        // Polished cap with a radial highlight.
        auto cap = knob.reduced (kr * 0.18f);
        juce::ColourGradient cg (juce::Colours::white, cap.getX() + cap.getWidth() * 0.35f, cap.getY() + cap.getHeight() * 0.3f,
                                 juce::Colour (0xff7f8285), cap.getRight(), cap.getBottom(), true);
        cg.addColour (0.5, juce::Colour (0xffc9cbcc));
        g.setGradientFill (cg);
        g.fillEllipse (cap);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (cap, 0.8f);
        g.drawEllipse (knob, 1.0f);

        const auto tip = c.getPointOnCircumference (kr * 0.78f, angle);
        const auto from = c.getPointOnCircumference (kr * 0.25f, angle);
        g.setColour (juce::Colour (0xff1c1c1c));
        g.drawLine (juce::Line<float> (from, tip), 2.6f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (mono (12.0f));
        l->setColour (juce::Label::backgroundColourId, colours::screen);
        l->setColour (juce::Label::textColourId, colours::phosphor);
        l->setColour (juce::Label::outlineColourId, juce::Colour (0xff6f6c60));
        return l;
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace colours;
        auto r = drawScreen (g, juce::Rectangle<int> (width, height).toFloat(), 2.0f);
        if (box.hasKeyboardFocus (true))
        {
            g.setColour (phosphor.withAlpha (0.4f));
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
        }
        juce::Path arrow;
        const float ax = r.getRight() - 12.0f, ay = r.getCentreY();
        arrow.addTriangle (ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
        g.setColour (phosphor.withAlpha (box.isEnabled() ? 0.9f : 0.3f));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (13.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getPopupMenuFont() override { return mono (14.0f, false); }

    // Slide switch with a status LED.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace colours;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        auto slot = juce::Rectangle<float> (40.0f, 16.0f).withCentre (r.getCentre().translated (8.0f, 0.0f));
        auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ slot.getX() - 12.0f, slot.getCentreY() });

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led.expanded (1.5f));
        g.setColour (on ? phosphor : juce::Colour (0xff1d3326));
        g.fillEllipse (led);
        if (on)
        {
            g.setColour (phosphor.withAlpha (0.3f));
            g.fillEllipse (led.expanded (4.0f));
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.fillEllipse (led.reduced (3.0f).translated (-1.0f, -1.0f));
        }

        g.setColour (juce::Colour (0xff1b1c18));
        g.fillRoundedRectangle (slot, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawLine (slot.getX() + 2.0f, slot.getBottom() + 0.5f, slot.getRight() - 2.0f, slot.getBottom() + 0.5f, 1.0f);

        auto nub = slot.reduced (2.0f).withWidth (slot.getWidth() * 0.5f - 2.0f);
        if (on) nub.setX (slot.getCentreX());
        g.setGradientFill (juce::ColourGradient (over ? juce::Colours::white : metalLite, nub.getX(), nub.getY(),
                                                 juce::Colour (0xff6c6a60), nub.getX(), nub.getBottom(), false));
        g.fillRoundedRectangle (nub, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 1; i <= 3; ++i)
            g.drawVerticalLine ((int) (nub.getX() + nub.getWidth() * (float) i / 4.0f), nub.getY() + 2.0f, nub.getBottom() - 2.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        drawKeycap (g, b.getLocalBounds().toFloat().reduced (1.0f, 0.0f).withTrimmedBottom (3.0f), over, down, bg);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont (sans (13.0f));
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId));
        auto r = b.getLocalBounds().withTrimmedBottom (6).translated (0, down ? 1 : 0);
        g.drawFittedText (b.getButtonText(), r.reduced (6, 0), juce::Justification::centred, 1);
    }

    int getTabButtonBestWidth (juce::TabBarButton& b, int depth) override
    {
        return (int) juce::GlyphArrangement::getStringWidth (sans (14.0f), b.getButtonText()) + depth + 24;
    }

    void drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool down) override
    {
        using namespace colours;
        const bool front = b.isFrontTab();
        auto r = b.getLocalBounds().toFloat().reduced (3.0f, 2.0f).withTrimmedBottom (3.0f);
        drawKeycap (g, r, over, down || front, keycap);

        auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 14.0f, r.getCentreY() - 1.0f + (front ? 1.5f : 0.0f) });
        g.setColour (front ? amber : juce::Colour (0xff3a2a1a));
        g.fillEllipse (led);
        if (front)
        {
            g.setColour (amber.withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setFont (sans (14.0f));
        g.setColour (front ? amber : juce::Colours::white);
        g.drawText (b.getButtonText(), r.withTrimmedLeft (24.0f).withTrimmedBottom (4.0f).translated (0.0f, front ? 1.5f : 0.0f),
                    juce::Justification::centred);
    }

    void drawTabbedButtonBarBackground (juce::TabbedButtonBar&, juce::Graphics&) override {}
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override
    {
        auto track = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (track, 3.0f);
        auto thumb = vertical ? juce::Rectangle<float> ((float) x, (float) thumbStart, (float) w, (float) thumbSize)
                              : juce::Rectangle<float> ((float) thumbStart, (float) y, (float) thumbSize, (float) h);
        thumb = thumb.reduced (2.0f);
        g.setColour (colours::metal.brighter (over || down ? 0.15f : 0.0f));
        g.fillRoundedRectangle (thumb, 3.0f);
    }
};
} // namespace retro
