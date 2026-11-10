// Zarya Synth - parameter layout and the APVTS -> SynthParams snapshot.
#include "PluginProcessor.h"

using namespace zarya;
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
juce::StringArray names (const char* const* list, int count)
{
    juce::StringArray a;
    for (int i = 0; i < count; ++i) a.add (list[i]);
    return a;
}

juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

struct Builder
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> list;

    void f (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, const juce::String& unit = {})
    {
        // Default float formatting prints 7 decimals; scale precision to the magnitude instead.
        auto toText = [] (float v, int maxLen)
        {
            const float a = std::abs (v);
            const int decimals = a >= 1000.0f ? 0 : a >= 100.0f ? 1 : a >= 10.0f ? 2 : 3;
            auto s = juce::String (v, decimals);
            return maxLen > 0 ? s.substring (0, maxLen) : s;
        };
        list.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                                                                     juce::AudioParameterFloatAttributes()
                                                                         .withLabel (unit)
                                                                         .withStringFromValueFunction (toText)));
    }
    void f (const juce::String& id, const juce::String& name, float lo, float hi, float def, const juce::String& unit = {})
    {
        f (id, name, juce::NormalisableRange<float> (lo, hi), def, unit);
    }
    void i (const juce::String& id, const juce::String& name, int lo, int hi, int def)
    {
        list.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def));
    }
    void b (const juce::String& id, const juce::String& name, bool def)
    {
        list.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    }
    void c (const juce::String& id, const juce::String& name, const juce::StringArray& choices, int def)
    {
        list.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
    }
};

juce::String id (const char* prefix, int index, const char* suffix)
{
    return juce::String (prefix) + juce::String (index + 1) + "_" + suffix;
}
} // namespace

APVTS::ParameterLayout ZaryaProcessor::createLayout()
{
    Builder b;
    const auto tables = names (WavetableBank::tableNames(), WavetableBank::kNumTables);
    const auto divs = names (divisionNames(), kNumDivisions);

    for (int o = 0; o < kNumOsc; ++o)
    {
        const juce::String n = "Osc " + juce::String (o + 1) + " ";
        b.b (id ("osc", o, "on"), n + "On", o == 0);
        b.c (id ("osc", o, "mode"), n + "Mode", names (oscModeNames(), (int) OscMode::Count), 0);
        b.c (id ("osc", o, "table"), n + "Wavetable", tables, o == 0 ? 0 : (o == 1 ? 2 : 3));
        b.f (id ("osc", o, "pos"), n + "Position", 0.0f, 1.0f, o == 0 ? 0.66f : 0.0f);
        b.i (id ("osc", o, "oct"), n + "Octave", -3, 3, 0);
        b.i (id ("osc", o, "semi"), n + "Semitone", -12, 12, 0);
        b.f (id ("osc", o, "fine"), n + "Fine", -100.0f, 100.0f, 0.0f, "ct");
        b.f (id ("osc", o, "level"), n + "Level", 0.0f, 1.0f, 0.8f);
        b.f (id ("osc", o, "pan"), n + "Pan", -1.0f, 1.0f, 0.0f);
        b.i (id ("osc", o, "unison"), n + "Unison", 1, kMaxUnison, 1);
        b.f (id ("osc", o, "detune"), n + "Detune", 0.0f, 1.0f, 0.15f);
        b.b (id ("osc", o, "loop"), n + "Sample Loop", true);
        b.f (id ("osc", o, "gsize"), n + "Grain Size", skewed (5.0f, 500.0f, 80.0f), 80.0f, "ms");
        b.f (id ("osc", o, "gdens"), n + "Grain Density", skewed (1.0f, 100.0f, 20.0f), 20.0f, "/s");
        b.f (id ("osc", o, "gspray"), n + "Grain Spray", 0.0f, 1.0f, 0.2f);
        b.f (id ("osc", o, "gscan"), n + "Grain Scan", -1.0f, 1.0f, 0.5f);
        b.b (id ("osc", o, "tofilter"), n + "To Filter", true);
    }

    b.b ("filter_on", "Filter On", true);
    b.c ("filter_type", "Filter Type", names (filterTypeNames(), (int) FilterType::Count), 1);
    b.f ("filter_cutoff", "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 4000.0f, "Hz");
    b.f ("filter_res", "Resonance", 0.0f, 1.0f, 0.2f);
    b.f ("filter_drive", "Drive", 0.0f, 1.0f, 0.0f);
    b.f ("filter_key", "Keytrack", 0.0f, 1.0f, 0.0f);

    for (int e = 0; e < kNumEnv; ++e)
    {
        const juce::String n = e == 0 ? juce::String ("Amp Env ") : "Env " + juce::String (e + 1) + " ";
        b.f (id ("env", e, "a"), n + "Attack", skewed (0.0005f, 10.0f, 0.5f), e == 0 ? 0.005f : 0.01f, "s");
        b.f (id ("env", e, "d"), n + "Decay", skewed (0.0005f, 10.0f, 0.5f), 0.3f, "s");
        b.f (id ("env", e, "s"), n + "Sustain", 0.0f, 1.0f, e == 0 ? 0.8f : 0.3f);
        b.f (id ("env", e, "r"), n + "Release", skewed (0.001f, 10.0f, 0.5f), 0.3f, "s");
    }

    for (int l = 0; l < kNumLfo; ++l)
    {
        const juce::String n = "LFO " + juce::String (l + 1) + " ";
        b.c (id ("lfo", l, "shape"), n + "Shape", names (lfoShapeNames(), (int) LfoShape::Count), 0);
        b.f (id ("lfo", l, "rate"), n + "Rate", skewed (0.01f, 20.0f, 1.0f), 1.0f, "Hz");
        b.b (id ("lfo", l, "sync"), n + "Sync", false);
        b.c (id ("lfo", l, "div"), n + "Division", divs, 6);
    }

    for (int m = 0; m < kNumModSlots; ++m)
    {
        const juce::String n = "Mod " + juce::String (m + 1) + " ";
        b.c (id ("mod", m, "src"), n + "Source", names (modSourceNames(), (int) ModSource::Count), m == 0 ? (int) ModSource::Env2 : 0);
        b.c (id ("mod", m, "dst"), n + "Destination", names (modDestNames(), (int) ModDest::Count), m == 0 ? (int) ModDest::Cutoff : 0);
        b.f (id ("mod", m, "amt"), n + "Amount", -1.0f, 1.0f, m == 0 ? 0.3f : 0.0f);
    }

    b.b ("arp_on", "Arp On", false);
    b.c ("arp_mode", "Arp Mode", names (arpModeNames(), (int) ArpMode::Count), 0);
    b.c ("arp_div", "Arp Rate", divs, 12);
    b.i ("arp_oct", "Arp Octaves", 1, 4, 1);
    b.f ("arp_gate", "Arp Gate", 0.05f, 1.0f, 0.5f);

    b.i ("voice_poly", "Polyphony", 1, kMaxVoices, 8);
    b.c ("voice_mode", "Voice Mode", names (voiceModeNames(), (int) VoiceMode::Count), 0);
    b.f ("voice_glide", "Glide", skewed (0.0f, 2.0f, 0.2f), 0.0f, "s");
    b.f ("voice_vel", "Velocity Sens", 0.0f, 1.0f, 0.5f);
    b.i ("voice_bend", "Bend Range", 0, 24, 2);

    b.b ("fx_dist_on", "Distortion On", false);
    b.f ("fx_dist_drive", "Dist Drive", 0.0f, 1.0f, 0.3f);
    b.f ("fx_dist_mix", "Dist Mix", 0.0f, 1.0f, 0.5f);
    b.b ("fx_chorus_on", "Chorus On", false);
    b.f ("fx_chorus_rate", "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, "Hz");
    b.f ("fx_chorus_depth", "Chorus Depth", 0.0f, 1.0f, 0.5f);
    b.f ("fx_chorus_mix", "Chorus Mix", 0.0f, 1.0f, 0.4f);
    b.b ("fx_delay_on", "Delay On", false);
    b.c ("fx_delay_div", "Delay Time", divs, 9);
    b.f ("fx_delay_fb", "Delay Feedback", 0.0f, 0.95f, 0.35f);
    b.f ("fx_delay_mix", "Delay Mix", 0.0f, 1.0f, 0.25f);
    b.b ("fx_delay_pp", "Delay Ping-Pong", true);
    b.b ("fx_reverb_on", "Reverb On", false);
    b.f ("fx_reverb_size", "Reverb Size", 0.0f, 1.0f, 0.6f);
    b.f ("fx_reverb_damp", "Reverb Damping", 0.0f, 1.0f, 0.5f);
    b.f ("fx_reverb_mix", "Reverb Mix", 0.0f, 1.0f, 0.25f);

    b.f ("master_db", "Master", -60.0f, 6.0f, -6.0f, "dB");

    return { b.list.begin(), b.list.end() };
}

// Parameter snapshot: pointers are resolved once on the message thread, so the audio thread
// only performs atomic loads (no string building, no map lookups, no allocation).
void ZaryaProcessor::bindParams()
{
    SynthParams& p = params;
    auto raw = [this] (const juce::String& pid)
    {
        auto* ptr = apvts.getRawParameterValue (pid);
        jassert (ptr != nullptr); // parameter ID typo
        return ptr;
    };
    auto f = [&] (const juce::String& pid, float& dst) { floatBinds.push_back ({ raw (pid), &dst }); };
    auto i = [&] (const juce::String& pid, int& dst) { intBinds.push_back ({ raw (pid), &dst }); };
    auto b = [&] (const juce::String& pid, bool& dst) { boolBinds.push_back ({ raw (pid), &dst }); };

    for (int o = 0; o < kNumOsc; ++o)
    {
        auto& op = p.osc[o];
        b (id ("osc", o, "on"), op.on);
        i (id ("osc", o, "mode"), op.mode);
        i (id ("osc", o, "table"), op.table);
        f (id ("osc", o, "pos"), op.position);
        i (id ("osc", o, "oct"), op.octave);
        i (id ("osc", o, "semi"), op.semitone);
        f (id ("osc", o, "fine"), op.fine);
        f (id ("osc", o, "level"), op.level);
        f (id ("osc", o, "pan"), op.pan);
        i (id ("osc", o, "unison"), op.unison);
        f (id ("osc", o, "detune"), op.detune);
        b (id ("osc", o, "loop"), op.loop);
        f (id ("osc", o, "gsize"), op.grainSizeMs);
        f (id ("osc", o, "gdens"), op.grainDensity);
        f (id ("osc", o, "gspray"), op.grainSpray);
        f (id ("osc", o, "gscan"), op.grainScan);
        b (id ("osc", o, "tofilter"), op.toFilter);
    }

    b ("filter_on", p.filter.on);
    i ("filter_type", p.filter.type);
    f ("filter_cutoff", p.filter.cutoff);
    f ("filter_res", p.filter.resonance);
    f ("filter_drive", p.filter.drive);
    f ("filter_key", p.filter.keytrack);

    for (int e = 0; e < kNumEnv; ++e)
    {
        f (id ("env", e, "a"), p.env[e].attack);
        f (id ("env", e, "d"), p.env[e].decay);
        f (id ("env", e, "s"), p.env[e].sustain);
        f (id ("env", e, "r"), p.env[e].release);
    }

    for (int l = 0; l < kNumLfo; ++l)
    {
        i (id ("lfo", l, "shape"), p.lfo[l].shape);
        f (id ("lfo", l, "rate"), p.lfo[l].rateHz);
        b (id ("lfo", l, "sync"), p.lfo[l].sync);
        i (id ("lfo", l, "div"), p.lfo[l].division);
    }

    for (int m = 0; m < kNumModSlots; ++m)
    {
        i (id ("mod", m, "src"), p.mod[m].source);
        i (id ("mod", m, "dst"), p.mod[m].dest);
        f (id ("mod", m, "amt"), p.mod[m].amount);
    }

    b ("arp_on", p.arp.on);
    i ("arp_mode", p.arp.mode);
    i ("arp_div", p.arp.division);
    i ("arp_oct", p.arp.octaves);
    f ("arp_gate", p.arp.gate);

    i ("voice_poly", p.voice.polyphony);
    i ("voice_mode", p.voice.mode);
    f ("voice_glide", p.voice.glide);
    f ("voice_vel", p.voice.velSens);
    i ("voice_bend", p.voice.bendRange);

    auto& fx = p.fx;
    b ("fx_dist_on", fx.distOn);       f ("fx_dist_drive", fx.distDrive);     f ("fx_dist_mix", fx.distMix);
    b ("fx_chorus_on", fx.chorusOn);   f ("fx_chorus_rate", fx.chorusRate);   f ("fx_chorus_depth", fx.chorusDepth);
    f ("fx_chorus_mix", fx.chorusMix);
    b ("fx_delay_on", fx.delayOn);     i ("fx_delay_div", fx.delayDivision);  f ("fx_delay_fb", fx.delayFeedback);
    f ("fx_delay_mix", fx.delayMix);   b ("fx_delay_pp", fx.delayPingPong);
    b ("fx_reverb_on", fx.reverbOn);   f ("fx_reverb_size", fx.reverbSize);   f ("fx_reverb_damp", fx.reverbDamp);
    f ("fx_reverb_mix", fx.reverbMix);

    f ("master_db", p.masterDb);
}

void ZaryaProcessor::readParams() noexcept
{
    for (auto& [src, dst] : floatBinds) *dst = src->load (std::memory_order_relaxed);
    for (auto& [src, dst] : intBinds) *dst = (int) std::lround (src->load (std::memory_order_relaxed));
    for (auto& [src, dst] : boolBinds) *dst = src->load (std::memory_order_relaxed) >= 0.5f;
}
