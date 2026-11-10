// Zarya Synth - editor implementation.
#include "PluginEditor.h"

namespace
{
constexpr int kCellW = 84, kCellH = 100, kTitleH = 30, kSampleViewH = 112;
const char* kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";
namespace col = retro::colours;

int itemHeight (juce::Component& c, int width)
{
    if (auto* g = dynamic_cast<ParamGroup*> (&c)) return g->getHeightForWidth (width);
    return kSampleViewH;
}

// Places its items side by side with equal widths.
class Row : public juce::Component
{
public:
    template <typename T>
    Row& add (std::unique_ptr<T> c)
    {
        addAndMakeVisible (*c);
        items.push_back (std::move (c));
        return *this;
    }
    int heightFor (int width) const
    {
        const int w = width / std::max (1, (int) items.size());
        int h = 0;
        for (auto& it : items) h = std::max (h, itemHeight (*it, w));
        return h;
    }
    void resized() override
    {
        const int n = std::max (1, (int) items.size());
        const int w = getWidth() / n;
        for (int i = 0; i < (int) items.size(); ++i)
            items[(size_t) i]->setBounds (i * w, 0, i == n - 1 ? getWidth() - i * w : w, getHeight());
    }
private:
    std::vector<std::unique_ptr<juce::Component>> items;
};

// A vertically scrolling stack of rows.
class Page : public juce::Component
{
public:
    Page()
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }
    Row& addRow()
    {
        rows.push_back (std::make_unique<Row>());
        content.addAndMakeVisible (*rows.back());
        return *rows.back();
    }
    void resized() override
    {
        viewport.setBounds (getLocalBounds());
        const int w = std::max (100, getWidth() - viewport.getScrollBarThickness() - 2);
        int y = 6;
        for (auto& r : rows)
        {
            const int h = r->heightFor (w - 12);
            r->setBounds (6, y, w - 12, h);
            y += h + 6;
        }
        content.setSize (w, y);
    }
private:
    juce::Component content;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<Row>> rows;
};
} // namespace

//==============================================================================
ParamGroup::ParamGroup (ZaryaProcessor& p, const juce::String& t, const juce::StringArray& idPrefixes,
                        const juce::String& strip, int columns)
    : title (t), forcedColumns (columns)
{
    for (auto* ap : p.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap);
        if (rp == nullptr) continue;
        const auto pid = rp->getParameterID();
        bool match = false;
        for (const auto& pre : idPrefixes) match = match || pid.startsWith (pre);
        if (! match) continue;

        Control c;
        juce::String name = rp->getName (64);
        if (strip.isNotEmpty() && name.startsWith (strip)) name = name.substring (strip.length());
        c.label = std::make_unique<juce::Label> (juce::String(), name);
        c.label->setJustificationType (juce::Justification::centred);
        c.label->setFont (retro::sans (11.5f));
        c.label->setColour (juce::Label::textColourId, col::engraved);
        addAndMakeVisible (*c.label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (rp))
        {
            auto box = std::make_unique<juce::ComboBox>();
            box->addItemList (choice->choices, 1);
            c.comboAttachment = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pid, *box);
            c.comp = std::move (box);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr)
        {
            auto toggle = std::make_unique<juce::ToggleButton>();
            c.buttonAttachment = std::make_unique<APVTS::ButtonAttachment> (p.apvts, pid, *toggle);
            c.comp = std::move (toggle);
        }
        else
        {
            auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 17);
            s->setColour (juce::Slider::textBoxTextColourId, col::phosphor);
            s->setColour (juce::Slider::textBoxBackgroundColourId, col::screen);
            s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff6f6c60));
            c.sliderAttachment = std::make_unique<APVTS::SliderAttachment> (p.apvts, pid, *s);
            c.isSlider = true;
            c.comp = std::move (s);
        }
        addAndMakeVisible (*c.comp);
        controls.push_back (std::move (c));
    }
}

int ParamGroup::columnsFor (int width) const
{
    if (forcedColumns > 0) return forcedColumns;
    return juce::jlimit (1, std::max (1, (int) controls.size()), (width - 20) / kCellW);
}

int ParamGroup::getHeightForWidth (int width) const
{
    const int cols = columnsFor (width);
    const int rows = ((int) controls.size() + cols - 1) / cols;
    return kTitleH + rows * kCellH + 16;
}

void ParamGroup::paint (juce::Graphics& g)
{
    retro::drawPlate (g, getLocalBounds().toFloat().reduced (3.0f, 3.0f).withTrimmedBottom (2.0f));
    const auto f = retro::mono (13.0f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title.toUpperCase()) + 20.0f;
    auto tag = retro::drawScreen (g, { 18.0f, 8.0f, tw, 20.0f }, 2.0f);
    g.setFont (f);
    g.setColour (col::cyan);
    g.drawText (title.toUpperCase(), tag, juce::Justification::centred);
}

void ParamGroup::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    area.removeFromTop (kTitleH - 6);
    const int cols = columnsFor (getWidth());
    const int cw = forcedColumns > 0 ? area.getWidth() / cols : kCellW;
    for (int i = 0; i < (int) controls.size(); ++i)
    {
        auto& c = controls[(size_t) i];
        auto cell = juce::Rectangle<int> (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * kCellH, cw, kCellH).reduced (2);
        c.label->setBounds (cell.removeFromTop (16));
        if (c.isSlider) c.comp->setBounds (cell);
        else c.comp->setBounds (cell.withSizeKeepingCentre (std::min (cell.getWidth(), 150), 26));
    }
}

//==============================================================================
SampleView::SampleView (ZaryaProcessor& p, int oscIndex) : proc (p), osc (oscIndex)
{
    addAndMakeVisible (loadButton);
    addAndMakeVisible (resetButton);
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load sample for oscillator " + juce::String (osc + 1), juce::File(), kAudioWildcard);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) load (f);
                              });
    };
    resetButton.onClick = [this] { proc.resetSampleToDefault (osc); status.clear(); };
    startTimerHz (10);
    timerCallback();
}

SampleView::~SampleView() { stopTimer(); }

void SampleView::load (const juce::File& f)
{
    status = proc.loadSample (osc, f);
    timerCallback();
    repaint();
}

void SampleView::timerCallback()
{
    auto s = proc.getSample (osc);
    if (s != current) { current = std::move (s); repaint(); }
}

bool SampleView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3")) { dragOver = true; repaint(); return true; }
    return false;
}

void SampleView::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (! files.isEmpty()) load (juce::File (files[0]));
}

void SampleView::resized()
{
    auto r = getLocalBounds().reduced (16, 14);
    auto buttons = r.removeFromRight (132);
    loadButton.setBounds (buttons.removeFromTop (38));
    buttons.removeFromTop (6);
    resetButton.setBounds (buttons.removeFromTop (38));
}

void SampleView::paint (juce::Graphics& g)
{
    retro::drawPlate (g, getLocalBounds().toFloat().reduced (3.0f, 3.0f).withTrimmedBottom (2.0f));
    auto screen = retro::drawScreen (g, getLocalBounds().reduced (16, 12).withTrimmedRight (146).toFloat());
    if (dragOver)
    {
        g.setColour (col::phosphor);
        g.drawRoundedRectangle (screen, 3.0f, 2.0f);
    }

    auto inner = screen.reduced (8.0f, 4.0f);
    auto caption = inner.removeFromTop (16.0f);
    g.setFont (retro::mono (12.0f));
    g.setColour (col::cyan);
    juce::String text = "OSC" + juce::String (osc + 1) + " SAMPLE> ";
    text += current != nullptr ? juce::String::fromUTF8 (current->name.c_str()) : juce::String ("NONE");
    if (current != nullptr && current->sampleRate > 0.0)
        text += juce::String::formatted ("  [%.2fs]", (double) current->length() / current->sampleRate);
    g.drawText (text, caption, juce::Justification::centredLeft);
    if (status.isNotEmpty())
    {
        g.setColour (col::red);
        g.drawText (status, caption, juce::Justification::centredRight);
    }

    if (current != nullptr && inner.getWidth() > 0)
    {
        const int n = (int) current->overviewMin.size();
        const float mid = inner.getCentreY(), half = inner.getHeight() * 0.5f;
        const int x0 = (int) inner.getX(), w = (int) inner.getWidth();
        g.setColour (col::phosphor.withAlpha (0.12f));
        g.drawHorizontalLine ((int) mid, inner.getX(), inner.getRight());
        g.setColour (col::phosphor.withAlpha (0.85f));
        for (int x = 0; x < w && n > 0; ++x)
        {
            const int b = juce::jlimit (0, n - 1, x * n / w);
            const float y0 = mid - current->overviewMax[(size_t) b] * half;
            const float y1 = mid - current->overviewMin[(size_t) b] * half;
            g.drawVerticalLine (x0 + x, std::min (y0, y1), std::max (y0, y1) + 1.0f);
        }
    }
    retro::drawScanlines (g, screen);
}

//==============================================================================
Dashboard::Dashboard (ZaryaProcessor& p) : proc (p)
{
    setOpaque (false);
    startTimerHz (30);
}

Dashboard::~Dashboard() { stopTimer(); }

void Dashboard::resized()
{
    auto r = getLocalBounds().toFloat().reduced (18.0f, 14.0f);
    auto right = r.removeFromRight (236.0f);
    logoArea = right.removeFromTop (r.getHeight() * 0.58f);
    right.removeFromTop (8.0f);
    counterArea = right;
    r.removeFromRight (10.0f);
    meterArea = r.removeFromRight (42.0f);
    r.removeFromRight (10.0f);
    lfoArea = r.removeFromRight (std::min (220.0f, r.getWidth() * 0.32f));
    r.removeFromRight (10.0f);
    scopeArea = r;
}

void Dashboard::timerCallback()
{
    // Trigger on a rising zero crossing so periodic waves stand still on screen.
    const int end = proc.scopeWrite.load (std::memory_order_acquire);
    constexpr int size = ZaryaProcessor::kScopeSize, mask = size - 1, search = 2048;
    int start = (end - kScopeLen) & mask;
    for (int back = kScopeLen; back < kScopeLen + search; ++back)
    {
        const int i = (end - back) & mask;
        const float a = proc.scope[(i - 1) & mask].load (std::memory_order_relaxed);
        const float b = proc.scope[i].load (std::memory_order_relaxed);
        if (a < 0.0f && b >= 0.0f) { start = i; break; }
    }
    for (int i = 0; i < kScopeLen; ++i)
        scopeData[i] = proc.scope[(start + i) & mask].load (std::memory_order_relaxed);

    for (int l = 0; l < zarya::kNumLfo; ++l)
        lfoHistory[l][lfoPos] = proc.lfoDisplay[l].load (std::memory_order_relaxed);
    lfoPos = (lfoPos + 1) % kLfoHistory;

    const float peak = proc.outputPeak.exchange (0.0f, std::memory_order_relaxed);
    level = std::max (peak, level * 0.85f);
    if (level >= peakHold) { peakHold = level; peakHoldFrames = 30; }
    else if (--peakHoldFrames < 0) peakHold *= 0.92f;
    voices = proc.activeVoices.load (std::memory_order_relaxed);
    repaint();
}

void Dashboard::paint (juce::Graphics& g)
{
    retro::drawPlate (g, getLocalBounds().toFloat().reduced (3.0f, 3.0f));
    paintScope (g, scopeArea);
    paintLfos (g, lfoArea);
    paintMeter (g, meterArea);
    paintLogo (g, logoArea);
    paintCounter (g, counterArea);
}

void Dashboard::paintScope (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Light bezel around the CRT, as on bench oscilloscopes.
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (area.translated (1.0f, 2.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (col::metalLite, area.getX(), area.getY(), col::metalDark.darker (0.2f), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 10.0f);
    auto tube = area.reduced (9.0f);
    g.setColour (juce::Colour (0xff0b1712));
    g.fillRoundedRectangle (tube, 14.0f);
    juce::ColourGradient glass (juce::Colour (0xff10261b), tube.getCentreX(), tube.getCentreY(),
                                juce::Colour (0xff030805), tube.getX(), tube.getY(), true);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (tube, 14.0f);

    auto plot = tube.reduced (12.0f, 10.0f);
    g.setColour (col::phosphor.withAlpha (0.1f));
    for (int i = 1; i < 10; ++i)
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 10.0f), plot.getY(), plot.getBottom());
    for (int i = 1; i < 8; ++i)
        g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 8.0f), plot.getX(), plot.getRight());
    g.setColour (col::phosphor.withAlpha (0.22f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());
    g.drawVerticalLine ((int) plot.getCentreX(), plot.getY(), plot.getBottom());
    for (int i = 0; i <= 50; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 50.0f;
        g.drawVerticalLine ((int) x, plot.getCentreY() - (i % 5 == 0 ? 4.0f : 2.0f), plot.getCentreY() + (i % 5 == 0 ? 4.0f : 2.0f));
    }

    juce::Path trace;
    const float half = plot.getHeight() * 0.46f;
    for (int i = 0; i < kScopeLen; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (kScopeLen - 1);
        const float y = plot.getCentreY() - juce::jlimit (-1.08f, 1.08f, scopeData[i]) * half;
        if (i == 0) trace.startNewSubPath (x, y);
        else trace.lineTo (x, y);
    }
    // Phosphor glow: wide faint strokes under a sharp bright core.
    g.setColour (col::phosphor.withAlpha (0.08f));
    g.strokePath (trace, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::phosphor.withAlpha (0.22f));
    g.strokePath (trace, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::phosphor.brighter (0.4f));
    g.strokePath (trace, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (retro::mono (11.0f));
    g.setColour (col::phosphor.withAlpha (0.8f));
    g.drawText ("CH1 OUT  0.5V/DIV", plot.removeFromTop (14.0f), juce::Justification::topLeft);
    g.drawText ("TRIG+ AUTO", plot.removeFromBottom (14.0f), juce::Justification::bottomRight);

    retro::drawScanlines (g, tube);
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillEllipse (tube.withTrimmedBottom (tube.getHeight() * 0.55f).reduced (tube.getWidth() * 0.12f, 0.0f).translated (0.0f, 4.0f));
}

void Dashboard::paintLfos (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto screen = retro::drawScreen (g, area);
    const juce::Colour traceCol[zarya::kNumLfo] = { col::phosphor, col::red, juce::Colour (0xffffe14a), col::cyan };
    const float rowH = screen.getHeight() / (float) zarya::kNumLfo;
    for (int l = 0; l < zarya::kNumLfo; ++l)
    {
        auto row = juce::Rectangle<float> (screen.getX(), screen.getY() + rowH * (float) l, screen.getWidth(), rowH).reduced (5.0f, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawRect (row, 1.0f);
        auto label = row.removeFromTop (12.0f);
        g.setFont (retro::mono (10.0f));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText ("Wave(LFO" + juce::String (l + 1) + ")", label.translated (3.0f, 0.0f), juce::Justification::centredLeft);

        auto plot = row.reduced (3.0f, 2.0f);
        juce::Path p;
        for (int i = 0; i < kLfoHistory; ++i)
        {
            const float v = juce::jlimit (-1.0f, 1.0f, lfoHistory[l][(lfoPos + i) % kLfoHistory]);
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (kLfoHistory - 1);
            const float y = plot.getCentreY() - v * plot.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
        g.setColour (traceCol[l].withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (3.5f));
        g.setColour (traceCol[l]);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
    retro::drawScanlines (g, screen);
}

void Dashboard::paintMeter (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto screen = retro::drawScreen (g, area);
    auto label = screen.removeFromBottom (14.0f);
    g.setFont (retro::mono (9.0f));
    g.setColour (col::cyan);
    g.drawText ("LEVEL", label, juce::Justification::centred);

    constexpr int segs = 24;
    auto bars = screen.reduced (6.0f, 5.0f);
    const float segH = bars.getHeight() / (float) segs;
    const auto toNorm = [] (float x) { return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (x, -48.0f) + 48.0f) / 48.0f); };
    const int lit = (int) std::round (toNorm (level) * segs);
    const int hold = (int) std::round (toNorm (peakHold) * segs);
    for (int i = 0; i < segs; ++i)
    {
        const auto c = i >= segs - 3 ? col::red : i >= segs - 8 ? juce::Colour (0xffffd84a) : juce::Colour (0xff3fa8ff);
        auto seg = juce::Rectangle<float> (bars.getX(), bars.getBottom() - segH * (float) (i + 1), bars.getWidth(), segH).reduced (0.0f, 1.0f);
        const bool on = i < lit || i == hold - 1;
        g.setColour (on ? c : c.withAlpha (0.12f));
        g.fillRect (seg);
    }
    retro::drawScanlines (g, area.reduced (3.0f));
}

void Dashboard::paintLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto screen = retro::drawScreen (g, area).reduced (10.0f, 6.0f);
    auto big = screen.removeFromTop (screen.getHeight() * 0.55f);
    g.setFont (retro::mono (big.getHeight() * 0.62f));
    g.setColour (juce::Colours::white);
    g.drawText ("ZARYA", big.removeFromTop (big.getHeight() * 0.62f), juce::Justification::centredLeft);
    g.setFont (retro::mono (big.getHeight() * 0.85f));
    g.setColour (col::cyan);
    g.drawText ("OS v.9", big, juce::Justification::centredLeft);

    g.setFont (retro::mono (10.0f, false));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    const char* lines[] = { "The ZARYA AUDIO", "DSP-Machine Series 3F", "Zarya OS v0.9.1  WT/SMP/GRN" };
    const float lh = screen.getHeight() / 3.0f;
    for (auto* line : lines)
        g.drawText (line, screen.removeFromTop (lh), juce::Justification::centredLeft);
    retro::drawScanlines (g, area.reduced (3.0f));
}

void Dashboard::paintCounter (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto digitsRow = area.removeFromTop (area.getHeight() * 0.52f);
    const juce::String digits = juce::String (juce::jlimit (0, 99, voices)).paddedLeft ('0', 2);
    const float boxW = digitsRow.getHeight() * 0.72f;

    g.setFont (retro::sans (11.0f));
    g.setColour (col::engraved);
    g.drawFittedText ("VOICES", digitsRow.removeFromLeft (56.0f).toNearestInt(), juce::Justification::centred, 1);

    const auto drawDigit = [&] (juce::Rectangle<float> box, const juce::String& d)
    {
        auto inner = retro::drawScreen (g, box, 3.0f);
        g.setColour (juce::Colour (0xff1c0e04));
        g.fillRoundedRectangle (inner, 2.0f);
        g.setFont (retro::mono (inner.getHeight() * 0.85f));
        g.setColour (col::amber.withAlpha (0.12f));
        g.drawText ("8", inner, juce::Justification::centred);
        g.setColour (col::amber.withAlpha (0.3f));
        g.drawText (d, inner.translated (0.0f, 1.0f).expanded (1.0f), juce::Justification::centred);
        g.setColour (col::amber);
        g.drawText (d, inner, juce::Justification::centred);
    };
    for (int i = 0; i < 2; ++i)
        drawDigit (digitsRow.removeFromLeft (boxW + 6.0f).withTrimmedRight (6.0f), digits.substring (i, i + 1));

    digitsRow.removeFromLeft (4.0f);
    const auto dbText = level > 1.0e-4f ? juce::String (juce::roundToInt (juce::Decibels::gainToDecibels (level))) : juce::String ("--");
    auto dbScreen = retro::drawScreen (g, digitsRow.reduced (0.0f, digitsRow.getHeight() * 0.18f), 3.0f);
    g.setFont (retro::mono (16.0f));
    g.setColour (col::amber);
    g.drawText (dbText + "dB", dbScreen, juce::Justification::centred);

    area.removeFromTop (6.0f);
    auto lcd = retro::drawScreen (g, area, 3.0f);
    const int hashes = juce::jlimit (0, 16, voices);
    juce::String line;
    for (int i = 0; i < 16; ++i) line << (i < hashes ? "#" : ".");
    g.setFont (retro::mono (juce::jlimit (10.0f, 13.0f, lcd.getHeight() * 0.42f)));
    g.setColour (col::cyan);
    g.drawText (line, lcd.removeFromTop (lcd.getHeight() * 0.5f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    g.drawText (voices > 0 ? "PLAY::ON" : "LOAD::OK", lcd.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
}

//==============================================================================
AvatarWindow::AvatarWindow (ZaryaProcessor& p) : proc (p)
{
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose avatar image", juce::File(), "*.png;*.jpg;*.jpeg;*.gif");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) loadImage (fc.getResult()); });
    };
    addAndMakeVisible (loadButton);

    const juce::File saved (proc.avatarPath);
    if (proc.avatarPath.isNotEmpty() && saved.existsAsFile()) image = juce::ImageFileFormat::loadFrom (saved);
    if (! image.isValid()) image = juce::ImageCache::getFromMemory (BinaryData::avatar_png, BinaryData::avatar_pngSize);

    constrainer.setMinimumOnscreenAmounts (0xffffff, 0xffffff, 0xffffff, 0xffffff); // keep fully inside the editor
    seenNotes = proc.noteOnCount.load();
    startTimerHz (60);
}

AvatarWindow::~AvatarWindow() { stopTimer(); }

void AvatarWindow::loadImage (const juce::File& f)
{
    auto img = juce::ImageFileFormat::loadFrom (f);
    if (! img.isValid()) return;
    image = img;
    proc.avatarPath = f.getFullPathName();
    repaint();
}

void AvatarWindow::resized() { loadButton.setBounds (getWidth() - 58, 6, 50, 26); }

void AvatarWindow::mouseDown (const juce::MouseEvent& e) { dragger.startDraggingComponent (this, e); }
void AvatarWindow::mouseDrag (const juce::MouseEvent& e) { dragger.dragComponent (this, e, &constrainer); }

void AvatarWindow::timerCallback()
{
    const int count = proc.noteOnCount.load (std::memory_order_acquire);
    if (count != seenNotes)
    {
        seenNotes = count;
        const float vel = juce::jlimit (0.2f, 1.0f, proc.lastVelocity.load (std::memory_order_relaxed));
        hopVel = 0.75f + 0.6f * vel;                 // jump strength follows velocity
        squash = 0.25f;
        xTarget = juce::jlimit (0.0f, 1.0f, (float) (proc.lastNote.load() - 36) / 48.0f); // pitch -> position
        tilt = (xTarget > xPos ? 1.0f : -1.0f) * 0.12f;
    }
    // Simple ballistic hop with gravity and a squash on landing.
    constexpr float dt = 1.0f / 60.0f;
    hopVel -= 9.0f * dt;
    hop += hopVel * dt * 5.0f;
    if (hop <= 0.0f) { if (hopVel < -0.5f) squash = 0.18f; hop = 0.0f; hopVel = 0.0f; }
    if (hop > 1.0f) { hop = 1.0f; hopVel = std::min (hopVel, 0.0f); } // fast notes re-launch mid-air; stay inside the screen
    squash *= 0.85f;
    tilt *= 0.9f;
    xPos += (xTarget - xPos) * 0.12f;
    glow = std::max (proc.outputPeak.load (std::memory_order_relaxed), glow * 0.9f);
    repaint();
}

void AvatarWindow::paint (juce::Graphics& g)
{
    retro::drawPlate (g, getLocalBounds().toFloat().reduced (1.0f));
    g.setFont (retro::mono (11.0f));
    g.setColour (col::engraved);
    g.drawText ("AVATAR.SYS", getLocalBounds().reduced (12, 0).removeFromTop (38), juce::Justification::centredLeft);

    auto screen = retro::drawScreen (g, getLocalBounds().reduced (10).withTrimmedTop (28).toFloat());
    // Light "paper" screen so the artwork keeps its original colours; it pulses faintly with the output.
    g.setColour (juce::Colour (0xffeeeee6).interpolatedWith (juce::Colour (0xffd8ffe6), juce::jlimit (0.0f, 1.0f, glow)));
    g.fillRoundedRectangle (screen, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawHorizontalLine ((int) screen.getBottom() - 10, screen.getX() + 4.0f, screen.getRight() - 4.0f);

    if (image.isValid())
    {
        const float h = (screen.getHeight() - 10.0f) * 0.78f;
        const float w = std::min (h * (float) image.getWidth() / (float) image.getHeight(), screen.getWidth() - 8.0f);
        const float range = std::max (0.0f, screen.getWidth() - w - 8.0f);
        const float x = screen.getX() + 4.0f + range * xPos;
        const float baseY = screen.getBottom() - 8.0f;
        const float lift = hop * std::max (12.0f, screen.getHeight() - h - 10.0f);
        const float sy = 1.0f - squash, sx = 1.0f + squash * 0.6f;

        g.setColour (juce::Colours::black.withAlpha (0.45f / (1.0f + hop * 3.0f)));
        g.fillEllipse (x + w * 0.15f, baseY - 4.0f, w * 0.7f, 8.0f);

        const auto t = juce::AffineTransform::scale (w * sx / (float) image.getWidth(), h * sy / (float) image.getHeight())
                           .translated (x - w * (sx - 1.0f) * 0.5f, baseY - h * sy - lift)
                           .rotated (tilt, x + w * 0.5f, baseY - lift);
        g.setOpacity (1.0f); // the shadow colour's alpha would otherwise fade the image
        g.drawImageTransformed (image, t);
    }
}

//==============================================================================
ZaryaEditor::ZaryaEditor (ZaryaProcessor& p)
    : AudioProcessorEditor (&p), proc (p), dashboard (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard), avatar (p)
{
    setLookAndFeel (&lnf);
    using SA = juce::StringArray;
    auto group = [&p] (const juce::String& title, const SA& prefixes, const juce::String& strip, int cols = 0)
    {
        return std::make_unique<ParamGroup> (p, title, prefixes, strip, cols);
    };
    auto num = [] (const char* pre, int i) { return juce::String (pre) + juce::String (i + 1) + "_"; };

    auto* osc = new Page();
    for (int o = 0; o < zarya::kNumOsc; ++o)
    {
        osc->addRow().add (group ("Oscillator " + juce::String (o + 1), SA (num ("osc", o)), "Osc " + juce::String (o + 1) + " "));
        osc->addRow().add (std::make_unique<SampleView> (p, o));
    }

    auto* filterEnv = new Page();
    filterEnv->addRow().add (group ("Filter", SA ("filter_"), "Filter "));
    filterEnv->addRow().add (group ("Amp Envelope", SA (num ("env", 0)), "Amp Env "))
                       .add (group ("Envelope 2", SA (num ("env", 1)), "Env 2 "));
    filterEnv->addRow().add (group ("Envelope 3", SA (num ("env", 2)), "Env 3 "))
                       .add (group ("Envelope 4", SA (num ("env", 3)), "Env 4 "));

    auto* mod = new Page();
    mod->addRow().add (group ("LFO 1", SA (num ("lfo", 0)), "LFO 1 ")).add (group ("LFO 2", SA (num ("lfo", 1)), "LFO 2 "));
    mod->addRow().add (group ("LFO 3", SA (num ("lfo", 2)), "LFO 3 ")).add (group ("LFO 4", SA (num ("lfo", 3)), "LFO 4 "));
    mod->addRow().add (group ("Modulation Matrix", SA ("mod"), {}, 6));

    auto* play = new Page();
    play->addRow().add (group ("Arpeggiator", SA ("arp_"), "Arp ")).add (group ("Voice & Master", SA ({ "voice_", "master_" }), {}));

    auto* fx = new Page();
    fx->addRow().add (group ("Distortion", SA ("fx_dist_"), "Dist ")).add (group ("Chorus", SA ("fx_chorus_"), "Chorus "));
    fx->addRow().add (group ("Delay", SA ("fx_delay_"), "Delay ")).add (group ("Reverb", SA ("fx_reverb_"), "Reverb "));

    const auto tabBg = col::chassis;
    tabs.addTab ("OSC", tabBg, osc, true);
    tabs.addTab ("FILTER / ENV", tabBg, filterEnv, true);
    tabs.addTab ("LFO / MATRIX", tabBg, mod, true);
    tabs.addTab ("ARP / VOICE", tabBg, play, true);
    tabs.addTab ("FX", tabBg, fx, true);
    tabs.setTabBarDepth (42);
    tabs.setOutline (0);
    tabs.setIndent (0);
    addAndMakeVisible (tabs);

    addAndMakeVisible (dashboard);

    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (22.0f);
    addAndMakeVisible (keyboard);

    avatar.setSize (250, 300);
    addAndMakeVisible (avatar); // added last so it floats above everything

    setResizable (true, true);
    setResizeLimits (940, 720, 2200, 1600);
    setSize (1140, 900);
}

ZaryaEditor::~ZaryaEditor() { setLookAndFeel (nullptr); }

void ZaryaEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::chassis);
    // Faint diagonal hatching gives the dark chassis a cast-metal texture.
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    for (int x = -getHeight(); x < getWidth(); x += 6)
        g.drawLine ((float) x, 0.0f, (float) (x + getHeight()), (float) getHeight(), 1.0f);

    retro::drawPlate (g, keyboardPlate.toFloat());
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRect (keyboard.getBounds().expanded (3).toFloat());
}

void ZaryaEditor::resized()
{
    auto r = getLocalBounds().reduced (6);
    dashboard.setBounds (r.removeFromTop (juce::jlimit (190, 260, getHeight() / 4)));
    r.removeFromTop (4);
    keyboardPlate = r.removeFromBottom (104).reduced (3);
    keyboard.setBounds (keyboardPlate.reduced (22, 14));
    r.removeFromBottom (4);
    tabs.setBounds (r);

    // Default spot: bottom-right above the keyboard; afterwards only keep it inside the window.
    if (avatar.getX() == 0 && avatar.getY() == 0)
        avatar.setTopLeftPosition (getWidth() - avatar.getWidth() - 24, keyboardPlate.getY() - avatar.getHeight() - 10);
    avatar.setTopLeftPosition (juce::jlimit (0, std::max (0, getWidth() - avatar.getWidth()), avatar.getX()),
                               juce::jlimit (0, std::max (0, getHeight() - avatar.getHeight()), avatar.getY()));
}
