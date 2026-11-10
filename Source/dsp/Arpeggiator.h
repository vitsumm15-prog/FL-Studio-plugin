// Zarya Synth - JUCE-free arpeggiator: turns held notes into timed note on/off events.
#pragma once

#include "SynthParams.h"
#include <vector>

namespace zarya
{
class Arpeggiator
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; reset(); }

    void reset() noexcept
    {
        numHeld = 0;
        running = false;
        curNote = -1;
        step = 0;
        tStep = 0.0;
        tOff = 0.0;
        rng.seed (0xA5A5F00Du);
    }

    // Seed the held-note list (used when the arp is switched on while keys are down).
    void setHeld (const bool* keys, const float* vels) noexcept
    {
        numHeld = 0;
        for (int n = 0; n < 128; ++n)
            if (keys[n])
                held[numHeld++] = { n, vels[n], orderCounter++ };
        running = numHeld > 0;
        tStep = 0.0;
        step = 0;
    }

    // Stops the arp, returns the note that is still sounding (or -1).
    int stop() noexcept
    {
        const int n = curNote;
        reset();
        return n;
    }

    // in: sorted events of this block. out: cleared and filled with the transformed events.
    void process (const MidiEvent* in, int numIn, int numSamples, double bpm,
                  const ArpParams& p, std::vector<MidiEvent>& out)
    {
        out.clear();
        const double stepLen = std::max (16.0, divisionBeats (p.division) * 60.0 / std::max (20.0, bpm) * sampleRate);
        const double gate = clamp ((double) p.gate, 0.05, 1.0);
        const double inf = 1.0e30;
        int ei = 0;

        for (int guard = 0; guard < 100000; ++guard)
        {
            const double te = ei < numIn ? (double) in[ei].sample : inf;
            const double to = curNote >= 0 ? tOff : inf;
            const double ts = (running && numHeld > 0) ? tStep : inf;
            const double t = std::min (te, std::min (to, ts));
            if (t >= (double) numSamples)
                break;
            const int si = clamp ((int) t, 0, numSamples - 1);

            if (to <= t) // note-off first so legato steps (gate 100%) re-trigger cleanly
            {
                push (out, si, MidiEvent::NoteOff, curNote, 0.0f);
                curNote = -1;
                continue;
            }

            if (te <= t)
            {
                const MidiEvent& e = in[ei++];
                if (e.type == MidiEvent::NoteOn)
                {
                    addHeld (e.note, e.value);
                    if (! running) { running = true; tStep = (double) e.sample; step = 0; }
                }
                else if (e.type == MidiEvent::NoteOff)
                {
                    removeHeld (e.note);
                    if (numHeld == 0) running = false;
                }
                else if (e.type == MidiEvent::AllNotesOff)
                {
                    numHeld = 0;
                    running = false;
                    out.push_back (e);
                }
                else
                {
                    out.push_back (e);
                }
                continue;
            }

            // Arp step.
            if (curNote >= 0)
            {
                push (out, si, MidiEvent::NoteOff, curNote, 0.0f);
                curNote = -1;
            }
            float vel = 0.8f;
            const int note = nextNote (p, vel);
            if (note >= 0)
            {
                push (out, si, MidiEvent::NoteOn, note, vel);
                curNote = note;
                tOff = t + std::max (1.0, gate * stepLen);
            }
            tStep = t + stepLen;
        }

        tStep -= (double) numSamples;
        tOff  -= (double) numSamples;
    }

    bool isRunning() const noexcept { return running; }

private:
    struct Held { int note; float vel; uint32_t order; };

    static void push (std::vector<MidiEvent>& out, int sample, MidiEvent::Type type, int note, float value)
    {
        MidiEvent e;
        e.sample = sample;
        e.type = type;
        e.note = note;
        e.value = value;
        out.push_back (e);
    }

    void addHeld (int note, float vel) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
            if (held[i].note == note) { held[i].vel = vel; return; }
        if (numHeld < 128)
            held[numHeld++] = { note, vel, orderCounter++ };
    }

    void removeHeld (int note) noexcept
    {
        for (int i = 0; i < numHeld; ++i)
        {
            if (held[i].note == note)
            {
                for (int j = i; j < numHeld - 1; ++j) held[j] = held[j + 1];
                --numHeld;
                return;
            }
        }
    }

    int nextNote (const ArpParams& p, float& vel) noexcept
    {
        if (numHeld == 0) return -1;

        Held base[128];
        for (int i = 0; i < numHeld; ++i) base[i] = held[i];
        const bool byPitch = p.mode != (int) ArpMode::AsPlayed;
        // insertion sort: by pitch, or by press order for "As Played"
        for (int i = 1; i < numHeld; ++i)
        {
            Held key = base[i];
            int j = i - 1;
            while (j >= 0 && (byPitch ? base[j].note > key.note : base[j].order > key.order))
            {
                base[j + 1] = base[j];
                --j;
            }
            base[j + 1] = key;
        }

        const int octaves = clamp (p.octaves, 1, 4);
        int asc[512]; float ascVel[512]; int len = 0;
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < numHeld; ++i)
            {
                const int n = base[i].note + 12 * o;
                if (n <= 127) { asc[len] = n; ascVel[len] = base[i].vel; ++len; }
            }
        if (len == 0) return -1;

        int idx = 0;
        switch ((ArpMode) clamp (p.mode, 0, (int) ArpMode::Count - 1))
        {
            case ArpMode::Up:
            case ArpMode::AsPlayed:
                idx = step % len; break;
            case ArpMode::Down:
                idx = len - 1 - (step % len); break;
            case ArpMode::UpDown:
            {
                const int cycle = len > 1 ? 2 * len - 2 : 1;
                const int s = step % cycle;
                idx = s < len ? s : cycle - s;
                break;
            }
            case ArpMode::Random:
                idx = (int) (rng.next() % (uint32_t) len); break;
            case ArpMode::Count:
            default: idx = 0; break;
        }
        ++step;
        if (step > 1000000) step = 0;
        vel = ascVel[idx];
        return asc[idx];
    }

    double sampleRate = 48000.0;
    Held held[128] {};
    int numHeld = 0;
    uint32_t orderCounter = 0;
    bool running = false;
    int curNote = -1;
    int step = 0;
    double tStep = 0.0, tOff = 0.0;
    Random rng;
};
} // namespace zarya
