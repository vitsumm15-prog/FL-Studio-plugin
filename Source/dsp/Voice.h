// Zarya Synth - one synth voice: 3 oscillators (wavetable / sample / granular), filter, envelopes, mod matrix.
#pragma once

#include "SynthParams.h"
#include "Wavetable.h"
#include "SampleData.h"
#include "Envelope.h"
#include "Filter.h"

namespace zarya
{
// Values shared by all voices for the current control block.
struct GlobalMod
{
    float lfo[kNumLfo] {};
    float modWheel = 0.0f;
    float bendSemis = 0.0f;
};

class Voice
{
public:
    void prepare (double sampleRate);

    // legato = true keeps envelopes / phases running and only glides to the new note.
    // glideFrom < 0 means "no portamento source" (start directly on the note).
    void start (int note, float velocity, bool legato, float glideFrom, const SynthParams& p,
                const SampleData* const* samples, uint32_t seed);
    void release() noexcept;
    void reset() noexcept;

    void render (float* outL, float* outR, int numSamples, const SynthParams& p,
                 const GlobalMod& g, const SampleData* const* samples);

    bool isActive() const noexcept   { return active; }
    bool isReleased() const noexcept { return released; }
    int getNote() const noexcept     { return note; }
    float getLevel() const noexcept  { return ampEnv.getLevel(); }

    uint64_t startOrder = 0;   // for voice stealing (oldest first)
    bool sustained = false;     // note-off received while the sustain pedal is down

private:
    struct Grain
    {
        bool active = false;
        double pos = 0.0, inc = 1.0;
        int age = 0, length = 1;
        float gl = 0.7f, gr = 0.7f;
    };

    struct OscState
    {
        double phase[kMaxUnison] {};     // wavetable phase 0..1
        double playPos[kMaxUnison] {};   // sample mode position in samples
        bool playing[kMaxUnison] {};
        double loopStart = 0.0;
        Grain grains[kMaxGrains];
        double grainTimer = 0.0;
        double scanOffset = 0.0;         // granular playhead offset (fraction of sample)
        float prevLevel = 0.0f, prevPos = 0.0f;
        bool first = true;
    };

    void computeControl (const SynthParams& p, const GlobalMod& g, int n);
    void renderOsc (int o, const OscParams& op, const SampleData* s, int n,
                    float* busL, float* busR);
    void renderWavetable (int o, const OscParams& op, int n, float* busL, float* busR);
    void renderSample (int o, const OscParams& op, const SampleData* s, int n, float* busL, float* busR);
    void renderGranular (int o, const OscParams& op, const SampleData* s, int n, float* busL, float* busR);
    void spawnGrain (int o, const OscParams& op, const SampleData* s);

    double sr = 48000.0;
    bool active = false, released = false;
    int note = 60;
    float velocity = 1.0f;
    float currentNote = 60.0f, targetNote = 60.0f;
    float voiceRandom = 0.0f;
    Random rng;

    Envelope ampEnv, modEnv[kNumEnv - 1];
    StereoFilter filter;
    OscState osc[kNumOsc];

    // Control-rate results.
    float oscHz[kNumOsc] {};
    float oscLevel[kNumOsc] {}, oscPos[kNumOsc] {}, oscPan[kNumOsc] {};
    float filterG = 0.1f, prevFilterG = 0.1f, filterK = 2.0f, filterDrive = 0.0f;
    float ampGain = 1.0f, prevAmpGain = 1.0f;
    bool firstBlock = true;
};
} // namespace zarya
