// Zarya Synth - mono sample container used by the Sample and Granular oscillator modes.
#pragma once

#include "Common.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace zarya
{
struct SampleData
{
    std::vector<float> data;      // mono, normalised to about -1..1
    double sampleRate = 48000.0;
    std::string name;             // UTF-8 display name
    std::string path;             // UTF-8 absolute path (empty for the built-in sample)
    std::array<float, 512> overviewMin {};
    std::array<float, 512> overviewMax {};

    int length() const noexcept { return (int) data.size(); }

    void computeOverview()
    {
        const int len = length();
        for (int b = 0; b < 512; ++b)
        {
            const int s0 = (int) ((long long) b * len / 512);
            const int s1 = std::max (s0 + 1, (int) ((long long) (b + 1) * len / 512));
            float lo = 0.0f, hi = 0.0f;
            for (int i = s0; i < s1 && i < len; ++i)
            {
                lo = std::min (lo, data[(size_t) i]);
                hi = std::max (hi, data[(size_t) i]);
            }
            overviewMin[(size_t) b] = lo;
            overviewMax[(size_t) b] = hi;
        }
    }

    // Built-in evolving pad (root MIDI 60) so Sample/Granular modes sound out of the box.
    static std::shared_ptr<SampleData> makeDefault (double sr = 48000.0)
    {
        auto s = std::make_shared<SampleData>();
        s->sampleRate = sr;
        s->name = "Zarya Pad (built-in)";
        const int len = (int) (sr * 2.5);
        s->data.assign ((size_t) len, 0.0f);

        const double root = 261.6255653; // C, MIDI 60
        const double partners[3] = { 1.0, 1.4983, 2.0 }; // root, fifth, octave
        const double partnerGain[3] = { 1.0, 0.35, 0.45 };
        Random rng;
        rng.seed (12345u);
        float noiseLp = 0.0f;

        for (int i = 0; i < len; ++i)
        {
            const double t = (double) i / sr;
            const double sweep = 0.5 + 0.5 * std::sin (2.0 * kPiD * 0.4 * t - 1.2);
            double v = 0.0;
            for (int p = 0; p < 3; ++p)
            {
                const double f0 = root * partners[p];
                for (int k = 1; k <= 12; ++k)
                {
                    const double f = f0 * (double) k;
                    if (f > 0.45 * sr) break;
                    const double bright = std::exp (-(double) (k - 1) * (1.4 - 1.1 * sweep));
                    const double wobble = 1.0 + 0.3 * std::sin (2.0 * kPiD * (0.13 * (double) k + 0.07 * p) * t);
                    v += partnerGain[p] * bright * wobble * std::sin (2.0 * kPiD * f * t + 0.7 * (double) (k * (p + 1))) / (double) k;
                }
            }
            noiseLp += 0.05f * (rng.bi() - noiseLp);
            v += 0.6 * (double) noiseLp * sweep;

            const double fadeIn  = std::min (1.0, t / 0.05);
            const double fadeOut = std::min (1.0, (2.5 - t) / 0.25);
            s->data[(size_t) i] = (float) (v * fadeIn * std::max (0.0, fadeOut));
        }

        float peak = 0.0f;
        for (float x : s->data) peak = std::max (peak, std::abs (x));
        if (peak > 0.0f)
            for (float& x : s->data) x *= 0.9f / peak;

        s->computeOverview();
        return s;
    }
};
} // namespace zarya
