// Zarya Synth - editor: tabbed pages with auto-generated parameter groups, sample views and a keyboard.
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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

class ZaryaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZaryaEditor (ZaryaProcessor&);
    ~ZaryaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::LookAndFeel_V4 lnf;
    ZaryaProcessor& proc;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::MidiKeyboardComponent keyboard;
    juce::Label header, status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZaryaEditor)
};
