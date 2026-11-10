// Zarya Synth - JUCE audio processor: parameters, state, sample loading, MIDI -> engine bridge.
#pragma once

#include <JuceHeader.h>
#include "dsp/SynthEngine.h"

class ZaryaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ZaryaProcessor();
    ~ZaryaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zarya"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Message thread only. Returns an error message, or an empty string on success.
    juce::String loadSample (int osc, const juce::File& file);
    void resetSampleToDefault (int osc);
    std::shared_ptr<const zarya::SampleData> getSample (int osc) const;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState; // on-screen keyboard
    std::atomic<int> activeVoices { 0 };
    std::atomic<float> lfoDisplay[zarya::kNumLfo] {};

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void timerCallback() override; // frees replaced samples off the audio thread
    void bindParams();
    void readParams() noexcept;
    void setSampleInternal (int osc, std::shared_ptr<const zarya::SampleData> s);

    zarya::SynthEngine engine;
    zarya::SynthParams params;
    std::vector<zarya::MidiEvent> events;
    std::vector<std::pair<std::atomic<float>*, float*>> floatBinds;
    std::vector<std::pair<std::atomic<float>*, int*>> intBinds;
    std::vector<std::pair<std::atomic<float>*, bool*>> boolBinds;
    juce::AudioFormatManager formats;

    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const zarya::SampleData> samples[zarya::kNumOsc];      // owned by message thread (guarded)
    std::shared_ptr<const zarya::SampleData> audioSamples[zarya::kNumOsc]; // audio-thread copies
    std::vector<std::shared_ptr<const zarya::SampleData>> graveyard;       // message thread only
    std::shared_ptr<const zarya::SampleData> defaultSample;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZaryaProcessor)
};
