// Zarya Synth - effects chain: distortion, chorus, stereo / ping-pong delay, Freeverb-style reverb.
#pragma once

#include "Common.h"
#include <vector>

namespace zarya
{
struct Distortion
{
    // drive 0..1 maps to 0..36 dB of gain into tanh, with makeup 1/g^0.25.
    static inline void process (float* l, float* r, int n, float drive, float mix) noexcept
    {
        const float g = dbToGain (36.0f * clamp (drive, 0.0f, 1.0f));
        const float makeup = 1.0f / std::pow (g, 0.25f);
        for (int i = 0; i < n; ++i)
        {
            const float wl = std::tanh (g * l[i]) * makeup;
            const float wr = std::tanh (g * r[i]) * makeup;
            l[i] += (wl - l[i]) * mix;
            r[i] += (wr - r[i]) * mix;
        }
    }
};

class Chorus
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (sr * 0.05) + 4;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        write = 0;
        phase = 0.0;
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        phase = 0.0;
    }

    void process (float* l, float* r, int n, float rateHz, float depth, float mix) noexcept
    {
        if (size <= 4) return;
        const double inc = (double) rateHz / sampleRate;
        const float base = (float) (0.008 * sampleRate);
        const float span = (float) (0.006 * sampleRate) * clamp (depth, 0.0f, 1.0f);
        for (int i = 0; i < n; ++i)
        {
            buf[0][(size_t) write] = l[i];
            buf[1][(size_t) write] = r[i];
            const float m0 = 0.5f + 0.5f * (float) std::sin (2.0 * kPiD * phase);
            const float m1 = 0.5f + 0.5f * (float) std::sin (2.0 * kPiD * phase + 0.5 * kPiD);
            const float wetL = read (0, base + span * m0);
            const float wetR = read (1, base + span * m1);
            const float norm = 1.0f / (1.0f + 0.35f * mix);
            l[i] = (l[i] + mix * wetL * 0.7f) * norm;
            r[i] = (r[i] + mix * wetR * 0.7f) * norm;
            if (++write >= size) write = 0;
            phase += inc;
            if (phase >= 1.0) phase -= 1.0;
        }
    }

private:
    float read (int ch, float delaySamples) const noexcept
    {
        float pos = (float) write - delaySamples;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos % size;
        const int i1 = (i0 + 1) % size;
        const float t = pos - std::floor (pos);
        return buf[ch][(size_t) i0] + (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]) * t;
    }

    double sampleRate = 48000.0;
    std::vector<float> buf[2];
    int size = 0, write = 0;
    double phase = 0.0;
};

class StereoDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (sr * 4.0) + 4;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        write = 0;
        smoothedDelay = -1.0f;
        lpL = lpR = 0.0f;
    }
    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        smoothedDelay = -1.0f;
        lpL = lpR = 0.0f;
    }

    void process (float* l, float* r, int n, float delaySeconds, float feedback, float mix, bool pingPong) noexcept
    {
        if (size <= 4) return;
        const float target = clamp (delaySeconds * (float) sampleRate, 1.0f, (float) (size - 3));
        if (smoothedDelay < 0.0f) smoothedDelay = target;
        const float smooth = 1.0f - std::exp (-1.0f / (0.05f * (float) sampleRate));
        const float fb = clamp (feedback, 0.0f, 0.95f);
        const float lpCoef = 0.45f; // gentle darkening of repeats
        for (int i = 0; i < n; ++i)
        {
            smoothedDelay += (target - smoothedDelay) * smooth;
            const float dl = read (0, smoothedDelay);
            const float dr = read (1, smoothedDelay);
            lpL += (dl - lpL) * lpCoef;
            lpR += (dr - lpR) * lpCoef;
            if (pingPong)
            {
                buf[0][(size_t) write] = 0.5f * (l[i] + r[i]) + fb * lpR;
                buf[1][(size_t) write] = fb * lpL;
            }
            else
            {
                buf[0][(size_t) write] = l[i] + fb * lpL;
                buf[1][(size_t) write] = r[i] + fb * lpR;
            }
            l[i] += mix * dl;
            r[i] += mix * dr;
            if (++write >= size) write = 0;
        }
    }

private:
    float read (int ch, float delaySamples) const noexcept
    {
        float pos = (float) write - delaySamples;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos % size;
        const int i1 = (i0 + 1) % size;
        const float t = pos - std::floor (pos);
        return buf[ch][(size_t) i0] + (buf[ch][(size_t) i1] - buf[ch][(size_t) i0]) * t;
    }

    double sampleRate = 48000.0;
    std::vector<float> buf[2];
    int size = 0, write = 0;
    float smoothedDelay = -1.0f;
    float lpL = 0.0f, lpR = 0.0f;
};

// Freeverb-style reverb (8 combs + 4 allpasses per channel).
class Reverb
{
public:
    void prepare (double sr)
    {
        static const int combTuning[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apTuning[4] = { 556, 441, 341, 225 };
        const double scale = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            const int spread = c == 0 ? 0 : 23;
            for (int i = 0; i < 8; ++i)
            {
                combs[c][i].buf.assign ((size_t) std::max (8, (int) ((combTuning[i] + spread) * scale)), 0.0f);
                combs[c][i].idx = 0;
                combs[c][i].store = 0.0f;
            }
            for (int i = 0; i < 4; ++i)
            {
                aps[c][i].buf.assign ((size_t) std::max (8, (int) ((apTuning[i] + spread) * scale)), 0.0f);
                aps[c][i].idx = 0;
            }
        }
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& cb : combs[c]) { std::fill (cb.buf.begin(), cb.buf.end(), 0.0f); cb.store = 0.0f; }
            for (auto& ap : aps[c]) std::fill (ap.buf.begin(), ap.buf.end(), 0.0f);
        }
    }

    void process (float* l, float* r, int n, float sizeParam, float damping, float mix) noexcept
    {
        const float feedback = 0.7f + 0.28f * clamp (sizeParam, 0.0f, 1.0f);
        const float damp1 = 0.4f * clamp (damping, 0.0f, 1.0f);
        const float damp2 = 1.0f - damp1;
        const float m = clamp (mix, 0.0f, 1.0f);
        const float dry = std::cos (m * 0.5f * kPi);
        const float wet = std::sin (m * 0.5f * kPi) * 3.0f;
        for (int i = 0; i < n; ++i)
        {
            const float input = (l[i] + r[i]) * 0.015f;
            float out[2] = { 0.0f, 0.0f };
            for (int c = 0; c < 2; ++c)
            {
                float acc = 0.0f;
                for (auto& cb : combs[c])
                {
                    const float y = cb.buf[(size_t) cb.idx];
                    cb.store = y * damp2 + cb.store * damp1;
                    cb.buf[(size_t) cb.idx] = input + cb.store * feedback;
                    if (++cb.idx >= (int) cb.buf.size()) cb.idx = 0;
                    acc += y;
                }
                for (auto& ap : aps[c])
                {
                    const float b = ap.buf[(size_t) ap.idx];
                    const float y = -acc + b;
                    ap.buf[(size_t) ap.idx] = acc + b * 0.5f;
                    if (++ap.idx >= (int) ap.buf.size()) ap.idx = 0;
                    acc = y;
                }
                out[c] = acc;
            }
            l[i] = l[i] * dry + out[0] * wet;
            r[i] = r[i] * dry + out[1] * wet;
        }
    }

private:
    struct Comb { std::vector<float> buf; int idx = 0; float store = 0.0f; };
    struct AllPass { std::vector<float> buf; int idx = 0; };
    Comb combs[2][8];
    AllPass aps[2][4];
};
} // namespace zarya
