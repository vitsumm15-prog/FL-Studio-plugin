// Zarya Synth - TPT state-variable filter (Zavalishin / Simper), 12 and 24 dB modes.
#pragma once

#include "Common.h"

namespace zarya
{
enum class FilterType { LP12, LP24, HP12, HP24, BP, Notch, Count };

inline const char* const* filterTypeNames() noexcept
{
    static const char* const names[(int) FilterType::Count] = { "LP 12", "LP 24", "HP 12", "HP 24", "Band Pass", "Notch" };
    return names;
}

struct SvfStage
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    // Returns low, band and high outputs for gain g = tan(pi*fc/fs) and damping k.
    inline void process (float v0, float g, float k, float& low, float& band, float& high) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        band = v1;
        high = v0 - k * v1 - v2;
    }
};

// Stereo filter: one resonant stage, plus a Butterworth stage for the 24 dB modes.
struct StereoFilter
{
    SvfStage s1[2], s2[2];

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) { s1[c].reset(); s2[c].reset(); }
    }

    static float cutoffToG (float hz, double sr) noexcept
    {
        const float fc = clamp (hz, 16.0f, (float) (0.45 * sr));
        return std::tan (kPi * fc / (float) sr);
    }

    static float resonanceToK (float res) noexcept { return 2.0f - 1.96f * clamp (res, 0.0f, 1.0f); }

    inline float process (int ch, float x, FilterType type, float g, float k) noexcept
    {
        float lo, bp, hp;
        s1[ch].process (x, g, k, lo, bp, hp);
        switch (type)
        {
            case FilterType::LP12: return lo;
            case FilterType::HP12: return hp;
            case FilterType::BP:   return bp * k; // unity gain at the centre frequency
            case FilterType::Notch: return lo + hp;
            case FilterType::LP24:
            {
                float lo2, bp2, hp2;
                s2[ch].process (lo, g, 1.41421356f, lo2, bp2, hp2);
                return lo2;
            }
            case FilterType::HP24:
            {
                float lo2, bp2, hp2;
                s2[ch].process (hp, g, 1.41421356f, lo2, bp2, hp2);
                return hp2;
            }
            case FilterType::Count:
            default: return lo;
        }
    }
};
} // namespace zarya
