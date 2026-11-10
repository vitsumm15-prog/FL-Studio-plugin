// Zarya Synth - editor implementation.
#include "PluginEditor.h"

namespace
{
constexpr int kCellW = 84, kCellH = 96, kTitleH = 22, kSampleViewH = 104;
const juce::Colour kPanel (0xff1b2130), kAccent (0xffffa64d), kText (0xffdde3ee);
const char* kAudioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";

juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

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
        c.label->setFont (font (12.0f));
        c.label->setColour (juce::Label::textColourId, kText);
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
            s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
            s->setColour (juce::Slider::rotarySliderFillColourId, kAccent);
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
    return juce::jlimit (1, std::max (1, (int) controls.size()), (width - 12) / kCellW);
}

int ParamGroup::getHeightForWidth (int width) const
{
    const int cols = columnsFor (width);
    const int rows = ((int) controls.size() + cols - 1) / cols;
    return kTitleH + rows * kCellH + 12;
}

void ParamGroup::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (kAccent);
    g.setFont (font (14.0f, true));
    g.drawText (title, getLocalBounds().reduced (10, 4).removeFromTop (kTitleH - 4), juce::Justification::centredLeft);
}

void ParamGroup::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (kTitleH - 4);
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
    auto r = getLocalBounds().reduced (8);
    auto buttons = r.removeFromRight (130);
    loadButton.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (6);
    resetButton.setBounds (buttons.removeFromTop (28));
}

void SampleView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (kPanel.darker (0.3f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (dragOver ? kAccent : kAccent.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 6.0f, dragOver ? 2.0f : 1.0f);

    auto wave = getLocalBounds().reduced (10).withTrimmedRight (140);
    auto caption = wave.removeFromTop (16);
    g.setFont (font (12.0f));
    g.setColour (kText);
    juce::String text = "Osc " + juce::String (osc + 1) + " sample: ";
    text += current != nullptr ? juce::String::fromUTF8 (current->name.c_str()) : juce::String ("none");
    if (current != nullptr && current->sampleRate > 0.0)
        text += juce::String::formatted ("  (%.2f s)", (double) current->length() / current->sampleRate);
    g.drawText (text, caption, juce::Justification::centredLeft);
    if (status.isNotEmpty())
    {
        g.setColour (juce::Colours::orangered);
        g.drawText (status, caption, juce::Justification::centredRight);
    }

    if (current == nullptr || wave.getWidth() <= 0) return;
    g.setColour (kAccent.withAlpha (0.85f));
    const float mid = (float) wave.getCentreY(), half = (float) wave.getHeight() * 0.5f;
    const int n = (int) current->overviewMin.size();
    for (int x = 0; x < wave.getWidth(); ++x)
    {
        const int b = juce::jlimit (0, n - 1, x * n / wave.getWidth());
        const float y0 = mid - current->overviewMax[(size_t) b] * half;
        const float y1 = mid - current->overviewMin[(size_t) b] * half;
        g.drawVerticalLine (wave.getX() + x, std::min (y0, y1), std::max (y0, y1) + 1.0f);
    }
}

//==============================================================================
ZaryaEditor::ZaryaEditor (ZaryaProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    lnf.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
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

    const auto tabBg = juce::Colour (0xff121722);
    tabs.addTab ("Oscillators", tabBg, osc, true);
    tabs.addTab ("Filter / Envelopes", tabBg, filterEnv, true);
    tabs.addTab ("LFO / Mod Matrix", tabBg, mod, true);
    tabs.addTab ("Arp / Voice", tabBg, play, true);
    tabs.addTab ("Effects", tabBg, fx, true);
    tabs.setTabBarDepth (30);
    addAndMakeVisible (tabs);

    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    addAndMakeVisible (keyboard);

    header.setText ("ZARYA", juce::dontSendNotification);
    header.setFont (font (20.0f, true));
    header.setColour (juce::Label::textColourId, kAccent);
    addAndMakeVisible (header);
    status.setJustificationType (juce::Justification::centredRight);
    status.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (status);

    setResizable (true, true);
    setResizeLimits (760, 520, 2000, 1500);
    setSize (1060, 780);
    startTimerHz (10);
}

ZaryaEditor::~ZaryaEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ZaryaEditor::timerCallback()
{
    juce::String s = "Voices: " + juce::String (proc.activeVoices.load());
    for (int l = 0; l < zarya::kNumLfo; ++l)
        s += "   LFO" + juce::String (l + 1) + " " + juce::String (proc.lfoDisplay[l].load(), 2);
    status.setText (s, juce::dontSendNotification);
}

void ZaryaEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0d111a)); }

void ZaryaEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (34).reduced (10, 4);
    header.setBounds (top.removeFromLeft (140));
    status.setBounds (top);
    keyboard.setBounds (r.removeFromBottom (80).reduced (6, 4));
    tabs.setBounds (r.reduced (6, 0));
}
