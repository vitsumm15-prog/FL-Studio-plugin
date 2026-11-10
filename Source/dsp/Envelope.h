// Zarya Synth - exponential ADSR envelope (EarLevel-style target-ratio curves).
#pragma once

#include "Common.h"

namespace zarya
{
class Envelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) noexcept { sampleRate = sr; dirty = true; }

    // Times in seconds, sustain 0..1. Cheap to call every block: recomputes only on change.
    void setParams (float a, float d, float s, float r) noexcept
    {
        a = std::max (a, 0.0005f);
        d = std::max (d, 0.0005f);
        r = std::max (r, 0.0010f);
        s = clamp (s, 0.0f, 1.0f);
        if (! dirty && a == attackTime && d == decayTime && s == sustain && r == releaseTime)
            return;
        attackTime = a; decayTime = d; sustain = s; releaseTime = r;
        dirty = false;

        attackCoef  = coef ((float) (a * sampleRate), ratioA);
        attackBase  = (1.0f + ratioA) * (1.0f - attackCoef);
        decayCoef   = coef ((float) (d * sampleRate), ratioDR);
        decayBase   = (sustain - ratioDR) * (1.0f - decayCoef);
        releaseCoef = coef ((float) (r * sampleRate), ratioDR);
        releaseBase = -ratioDR * (1.0f - releaseCoef);
    }

    void noteOn() noexcept  { stage = Attack; }  // restarts from the current level (no click)
    void noteOff() noexcept { if (stage != Idle) stage = Release; }
    void reset() noexcept   { stage = Idle; level = 0.0f; }

    float process() noexcept
    {
        switch (stage)
        {
            case Attack:
                level = attackBase + level * attackCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;
            case Decay:
                level = decayBase + level * decayCoef;
                if (level <= sustain) { level = sustain; stage = Sustain; }
                break;
            case Sustain:
                level = sustain;
                break;
            case Release:
                level = releaseBase + level * releaseCoef;
                if (level <= 1.0e-5f) { level = 0.0f; stage = Idle; }
                break;
            case Idle:
            default:
                level = 0.0f;
                break;
        }
        return level;
    }

    // Advance n samples, return the final level (used for modulation envelopes).
    float advance (int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            process();
        return level;
    }

    float getLevel() const noexcept { return level; }
    Stage getStage() const noexcept { return stage; }
    bool isActive() const noexcept  { return stage != Idle; }

private:
    static float coef (float rateSamples, float ratio) noexcept
    {
        return rateSamples <= 0.0f ? 0.0f : std::exp (-std::log ((1.0f + ratio) / ratio) / rateSamples);
    }

    double sampleRate = 48000.0;
    Stage stage = Idle;
    float level = 0.0f;
    float attackTime = -1.0f, decayTime = -1.0f, sustain = -1.0f, releaseTime = -1.0f;
    bool dirty = true;
    float attackCoef = 0.0f, attackBase = 1.0f;
    float decayCoef = 0.0f, decayBase = 0.0f;
    float releaseCoef = 0.0f, releaseBase = 0.0f;
    static constexpr float ratioA  = 0.3f;
    static constexpr float ratioDR = 0.0001f;
};
} // namespace zarya
