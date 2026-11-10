// Zarya Synth - offline engine test: renders scenarios to WAV, checks NaN/peaks, measures speed.
#include "../Source/dsp/SynthEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace zarya;

namespace
{
void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) l.size();
    const uint32_t dataBytes = n * 2 * 2;
    auto w32 = [f] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [f] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t a = (int16_t) (clamp (l[i], -1.0f, 1.0f) * 32767.0f);
        const int16_t b = (int16_t) (clamp (r[i], -1.0f, 1.0f) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

struct Note { double t, dur; int note; float vel; };

int failures = 0;

void run (const char* name, SynthParams p, const std::vector<Note>& notes, double seconds,
          const SampleData* const* samples, double bpm = 120.0, bool transportPlaying = false)
{
    const int sr = 48000, block = 256;
    SynthEngine eng;
    eng.prepare (sr, block);
    const int total = (int) (seconds * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    std::vector<MidiEvent> ev;
    int maxVoices = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        ev.clear();
        for (const auto& nt : notes)
        {
            const int on = (int) (nt.t * sr), off = (int) ((nt.t + nt.dur) * sr);
            if (on >= pos && on < pos + n) { MidiEvent e; e.sample = on - pos; e.type = MidiEvent::NoteOn; e.note = nt.note; e.value = nt.vel; ev.push_back (e); }
            if (off >= pos && off < pos + n) { MidiEvent e; e.sample = off - pos; e.type = MidiEvent::NoteOff; e.note = nt.note; ev.push_back (e); }
        }
        std::stable_sort (ev.begin(), ev.end(), [] (const MidiEvent& a, const MidiEvent& b) { return a.sample < b.sample; });
        TransportInfo tr;
        tr.bpm = bpm; tr.isPlaying = transportPlaying; tr.hasPpq = transportPlaying;
        tr.ppq = (double) pos / sr * bpm / 60.0;
        eng.process (L.data() + pos, R.data() + pos, n, p, ev.data(), (int) ev.size(), tr, samples);
        maxVoices = std::max (maxVoices, eng.activeVoiceCount());
    }
    const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    float peak = 0.0f; double rms = 0.0; bool nan = false; float maxJump = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) nan = true;
        peak = std::max (peak, std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
        rms += (double) L[(size_t) i] * L[(size_t) i];
        if (i > 0) maxJump = std::max (maxJump, std::abs (L[(size_t) i] - L[(size_t) i - 1]));
    }
    rms = std::sqrt (rms / std::max (1, total));
    const bool ok = ! nan && peak > 0.01f && peak < 2.0f;
    if (! ok) ++failures;
    std::printf ("%-16s %s peak=%.3f rms=%.4f maxVoices=%d realtime=%.1fx maxStep=%.3f\n", name, ok ? "OK  " : "FAIL",
                 peak, rms, maxVoices, seconds / std::max (1e-9, cpu), maxJump);
    writeWav (std::string ("zarya_test_") + name + ".wav", L, R, sr);
}
} // namespace

int main()
{
    auto pad = SampleData::makeDefault (48000.0);
    const SampleData* samples[kNumOsc] = { pad.get(), pad.get(), pad.get() };
    const std::vector<Note> chord = { { 0.1, 1.5, 48, 0.9f }, { 0.1, 1.5, 55, 0.8f }, { 0.1, 1.5, 60, 0.8f }, { 0.1, 1.5, 64, 0.7f } };

    SynthParams base;
    base.osc[0].on = true;
    base.osc[0].table = 0; base.osc[0].position = 0.66f;
    base.mod[0] = { (int) ModSource::Env2, (int) ModDest::Cutoff, 0.4f };

    { auto p = base; run ("wt_basic", p, chord, 2.5, samples); }
    {
        auto p = base;
        p.osc[0].unison = 7; p.osc[0].detune = 0.3f; p.osc[0].table = 2;
        p.osc[1].on = true; p.osc[1].table = 3; p.osc[1].octave = -1;
        p.mod[1] = { (int) ModSource::Lfo1, (int) ModDest::Osc2Pos, 0.5f };
        p.lfo[0].rateHz = 0.5f;
        run ("wt_supersaw", p, chord, 2.5, samples);
    }
    { auto p = base; p.osc[0].mode = (int) OscMode::Sample; p.osc[0].position = 0.0f; run ("sample", p, chord, 2.5, samples); }
    {
        auto p = base; p.osc[0].mode = (int) OscMode::Granular; p.osc[0].grainDensity = 40.0f; p.osc[0].grainSpray = 0.4f;
        p.osc[0].unison = 3; run ("granular", p, chord, 2.5, samples);
    }
    {
        auto p = base; p.arp.on = true; p.arp.mode = (int) ArpMode::UpDown; p.arp.octaves = 2;
        p.env[0] = { 0.002f, 0.15f, 0.3f, 0.1f };
        run ("arp", p, chord, 2.5, samples, 128.0, true);
    }
    {
        auto p = base; p.voice.mode = (int) VoiceMode::Legato; p.voice.glide = 0.08f;
        const std::vector<Note> line = { { 0.1, 0.5, 48, 0.9f }, { 0.4, 0.5, 55, 0.9f }, { 0.8, 0.6, 60, 0.9f }, { 1.3, 0.4, 53, 0.9f } };
        p.lfo[1].shape = (int) LfoShape::Lorenz; p.mod[1] = { (int) ModSource::Lfo2, (int) ModDest::Cutoff, 0.2f };
        run ("legato_lorenz", p, line, 2.5, samples);
    }
    {
        auto p = base; p.osc[0].unison = 5; p.osc[1].on = true; p.osc[1].mode = (int) OscMode::Granular;
        p.osc[2].on = true; p.osc[2].table = 5; p.voice.polyphony = 16;
        p.fx.distOn = p.fx.chorusOn = p.fx.delayOn = p.fx.reverbOn = true;
        std::vector<Note> many;
        for (int i = 0; i < 24; ++i) many.push_back ({ 0.05 * i, 1.2, 36 + i * 2, 0.8f });
        p.lfo[2].shape = (int) LfoShape::Rossler; p.lfo[2].sync = true;
        p.mod[2] = { (int) ModSource::Lfo3, (int) ModDest::Osc3Pos, 0.6f };
        run ("stress_fx", p, many, 4.0, samples, 140.0, true);
    }
    {
        auto p = base; p.filter.type = 1; p.filter.resonance = 1.0f; p.filter.drive = 1.0f;
        p.mod[1] = { (int) ModSource::Lfo1, (int) ModDest::Cutoff, 1.0f }; p.lfo[0].rateHz = 8.0f;
        run ("filter_extreme", p, chord, 2.0, samples);
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
