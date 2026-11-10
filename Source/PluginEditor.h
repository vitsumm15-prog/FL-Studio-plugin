// Zarya Synth - editor: tabbed pages with auto-generated parameter groups, sample views and a keyboard.
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

// Top instrument panel: output oscilloscope, LFO traces, level meter, logo and voice counter.
class Dashboard : public juce::Component, private juce::Timer
{
public:
    explicit Dashboard (ZaryaProcessor& p);
    ~Dashboard() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintScope (juce::Graphics&, juce::Rectangle<float>);
    void paintLfos (juce::Graphics&, juce::Rectangle<float>);
    void paintMeter (juce::Graphics&, juce::Rectangle<float>);
    void paintLogo (juce::Graphics&, juce::Rectangle<float>);
    void paintCounter (juce::Graphics&, juce::Rectangle<float>);

    static constexpr int kScopeLen = 1024, kLfoHistory = 96;

    ZaryaProcessor& proc;
    juce::Rectangle<float> scopeArea, lfoArea, meterArea, logoArea, counterArea;
    float scopeData[kScopeLen] {};
    float lfoHistory[zarya::kNumLfo][kLfoHistory] {};
    int lfoPos = 0;
    float level = 0.0f, peakHold = 0.0f;
    int peakHoldFrames = 0, voices = 0;
};

// A titled box with one control per parameter whose ID starts with one of the prefixes.
class ParamGroup : public juce::Component
{
public:
    ParamGroup (ZaryaProcessor& p, const juce::String& title, const juce::StringArray& idPrefixes,
                const juce::String& stripFromNames, int forcedColumns = 0);

    int getHeightForWidth (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    struct Control
    {
        bool isSlider = false;
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<APVTS::SliderAttachment> sliderAttachment;
        std::unique_ptr<APVTS::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<APVTS::ButtonAttachment> buttonAttachment;
    };

    int columnsFor (int width) const;

    juce::String title;
    int forcedColumns = 0;
    std::vector<Control> controls;
};

// Waveform overview of an oscillator's sample with load/reset buttons and drag-and-drop.
class SampleView : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    SampleView (ZaryaProcessor& p, int oscIndex);
    ~SampleView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void load (const juce::File& f);

    ZaryaProcessor& proc;
    const int osc;
    juce::TextButton loadButton { "Load sample..." }, resetButton { "Built-in" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<const zarya::SampleData> current;
    juce::String status;
    bool dragOver = false;
};

// Floating CRT window with a user image that hops on each note; drag it anywhere in the editor.
class AvatarWindow : public juce::Component, private juce::Timer
{
public:
    explicit AvatarWindow (ZaryaProcessor& p);
    ~AvatarWindow() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void loadImage (const juce::File&);

    ZaryaProcessor& proc;
    juce::Image image;
    juce::TextButton loadButton { "Load" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    int seenNotes = 0;
    float hop = 0.0f, hopVel = 0.0f, squash = 0.0f, xPos = 0.5f, xTarget = 0.5f, tilt = 0.0f, glow = 0.0f;
};

class ZaryaEditor : public juce::AudioProcessorEditor
{
public:
    explicit ZaryaEditor (ZaryaProcessor&);
    ~ZaryaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    retro::LookAndFeel lnf;
    ZaryaProcessor& proc;
    Dashboard dashboard;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::MidiKeyboardComponent keyboard;
    AvatarWindow avatar;
    juce::Rectangle<int> keyboardPlate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZaryaEditor)
};
