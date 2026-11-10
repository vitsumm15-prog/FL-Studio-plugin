// Zarya Synth - plain parameter snapshot passed from the plugin wrapper to the engine each block.
#pragma once

#include "Common.h"

namespace zarya
{
enum class OscMode { Wavetable, Sample, Granular, Count };
enum class VoiceMode { Poly, Mono, Legato, Count };
enum class ArpMode { Up, Down, UpDown, Random, AsPlayed, Count };

enum class ModSource { None, Env2, Env3, Env4, Lfo1, Lfo2, Lfo3, Lfo4, Velocity, ModWheel, Note, Random, Count };

enum class ModDest
{
    None, PitchAll,
    Osc1Pitch, Osc2Pitch, Osc3Pitch,
    Osc1Pos, Osc2Pos, Osc3Pos,
    Osc1Level, Osc2Level, Osc3Level,
    Osc1Pan, Osc2Pan, Osc3Pan,
    Cutoff, Resonance, Drive, Amp,
    Count
};

inline const char* const* oscModeNames() noexcept
{
    static const char* const n[(int) OscMode::Count] = { "Wavetable", "Sample", "Granular" };
    return n;
}
inline const char* const* voiceModeNames() noexcept
{
    static const char* const n[(int) VoiceMode::Count] = { "Poly", "Mono", "Legato" };
    return n;
}
inline const char* const* arpModeNames() noexcept
{
    static const char* const n[(int) ArpMode::Count] = { "Up", "Down", "Up-Down", "Random", "As Played" };
    return n;
}
inline const char* const* modSourceNames() noexcept
{
    static const char* const n[(int) ModSource::Count] = {
        "None", "Env 2", "Env 3", "Env 4", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
        "Velocity", "Mod Wheel", "Note", "Random"
    };
    return n;
}
inline const char* const* modDestNames() noexcept
{
    static const char* const n[(int) ModDest::Count] = {
        "None", "Pitch (all)",
        "Osc 1 Pitch", "Osc 2 Pitch", "Osc 3 Pitch",
        "Osc 1 Position", "Osc 2 Position", "Osc 3 Position",
        "Osc 1 Level", "Osc 2 Level", "Osc 3 Level",
        "Osc 1 Pan", "Osc 2 Pan", "Osc 3 Pan",
        "Filter Cutoff", "Filter Resonance", "Filter Drive", "Amp"
    };
    return n;
}

struct OscParams
{
    bool on = false;
    int mode = 0;               // OscMode
    int table = 0;              // wavetable index
    float position = 0.0f;      // wavetable frame / sample start / granular scan start (0..1)
    int octave = 0;
    int semitone = 0;
    float fine = 0.0f;          // cents
    float level = 0.8f;
    float pan = 0.0f;
    int unison = 1;
    float detune = 0.15f;       // 0..1 -> spread up to +-1 semitone
    bool loop = true;           // sample mode
    float grainSizeMs = 80.0f;
    float grainDensity = 20.0f; // grains per second
    float grainSpray = 0.2f;
    float grainScan = 0.5f;     // playhead speed (0 = frozen)
    bool toFilter = true;
};

struct FilterParams
{
    bool on = true;
    int type = 1;               // FilterType
    float cutoff = 4000.0f;     // Hz
    float resonance = 0.2f;
    float drive = 0.0f;
    float keytrack = 0.0f;
};

struct EnvParams { float attack = 0.005f, decay = 0.3f, sustain = 0.8f, release = 0.3f; };

struct LfoParams
{
    int shape = 0;              // LfoShape
    float rateHz = 1.0f;
    bool sync = false;
    int division = 6;           // index into divisionBeats()
};

struct ModSlot { int source = 0; int dest = 0; float amount = 0.0f; };

struct ArpParams
{
    bool on = false;
    int mode = 0;               // ArpMode
    int division = 12;          // 1/16
    int octaves = 1;
    float gate = 0.5f;
};

struct VoiceParams
{
    int polyphony = 8;
    int mode = 0;               // VoiceMode
    float glide = 0.0f;         // seconds
    float velSens = 0.5f;
    int bendRange = 2;          // semitones
};

struct FxParams
{
    bool distOn = false;   float distDrive = 0.3f, distMix = 0.5f;
    bool chorusOn = false; float chorusRate = 0.6f, chorusDepth = 0.5f, chorusMix = 0.4f;
    bool delayOn = false;  int delayDivision = 9; float delayFeedback = 0.35f, delayMix = 0.25f; bool delayPingPong = true;
    bool reverbOn = false; float reverbSize = 0.6f, reverbDamp = 0.5f, reverbMix = 0.25f;
};

struct SynthParams
{
    OscParams osc[kNumOsc];
    FilterParams filter;
    EnvParams env[kNumEnv];
    LfoParams lfo[kNumLfo];
    ModSlot mod[kNumModSlots];
    ArpParams arp;
    VoiceParams voice;
    FxParams fx;
    float masterDb = -6.0f;
};

// One timestamped event inside the current block.
struct MidiEvent
{
    enum Type { NoteOn, NoteOff, PitchBend, ModWheel, Sustain, AllNotesOff };
    int sample = 0;
    Type type = NoteOn;
    int note = 0;
    float value = 0.0f;  // velocity 0..1, bend -1..1, wheel 0..1, sustain 0/1
};

struct TransportInfo
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool hasPpq = false;
    bool isPlaying = false;
};
} // namespace zarya
