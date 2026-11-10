// Zarya Synth - polyphonic engine: voice allocation, mono/legato, arpeggiator, LFOs and effects.
// JUCE-free so it can be unit-tested and rendered offline.
#pragma once

#include "Voice.h"
#include "Lfo.h"
#include "Effects.h"
#include "Arpeggiator.h"
#include <vector>

namespace zarya
{
class SynthEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    // events must be sorted by sample offset; samples[kNumOsc] may contain nullptr.
    void process (float* outL, float* outR, int numSamples, const SynthParams& p,
                  const MidiEvent* events, int numEvents, const TransportInfo& transport,
                  const SampleData* const* samples);

    int activeVoiceCount() const noexcept;
    float lfoValue (int index) const noexcept { return lfos[clamp (index, 0, kNumLfo - 1)].value(); }

private:
    void handleEvent (const MidiEvent& e, const SynthParams& p, const SampleData* const* samples);
    void noteOn (int note, float vel, const SynthParams& p, const SampleData* const* samples);
    void noteOff (int note, const SynthParams& p, const SampleData* const* samples);
    void releaseVoice (Voice& v);
    void allNotesOff();
    void renderSpan (float* outL, float* outR, int start, int n, const SynthParams& p,
                     const TransportInfo& transport, const SampleData* const* samples);
    Voice* findVoiceFor (int note, int polyphony);
    uint32_t nextSeed() noexcept;

    double sr = 48000.0;
    Voice voices[kMaxVoices];
    Lfo lfos[kNumLfo];
    GlobalMod gmod;
    Arpeggiator arp;
    bool arpWasOn = false;
    std::vector<MidiEvent> arpEvents;

    Chorus chorus;
    StereoDelay delay;
    Reverb reverb;

    bool keyDown[128] {};
    float keyVel[128] {};
    int monoStack[128] {};
    float monoVel[128] {};
    int monoCount = 0;
    bool sustainDown = false;
    float bend = 0.0f;       // -1..1
    float lastNote = -1.0f;  // portamento source
    uint64_t orderCounter = 0;
    uint32_t seedState = 0x1234567u;
    int samplesProcessed = 0; // position inside the current host block (for LFO sync)
    float masterGain = 0.5f;
    bool firstBlock = true;
};
} // namespace zarya
