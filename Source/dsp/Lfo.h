// Zarya Synth - global LFO with classic, random and chaotic (Lorenz / Roessler) shapes.
#pragma once

#include "Common.h"

namespace zarya
{
enum class LfoShape { Sine, Triangle, SawUp, SawDown, Square, SampleHold, SmoothRandom, Lorenz, Rossler, Count };

inline const char* const* lfoShapeNames() noexcept
{
    static const char* const names[(int) LfoShape::Count] = {
        "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Smooth Random", "Lorenz", "Roessler"
    };
    return names;
}

class Lfo
{
public:
    void reset (uint32_t seed) noexcept
    {
        rng.seed (seed);
        phase = 0.0;
        held = rng.bi();
        smoothFrom = rng.bi();
        smoothTo = rng.bi();
        lx = 0.1 + 0.01 * rng.bi(); ly = 0.0; lz = 0.0;
        rx = 0.1 + 0.01 * rng.bi(); ry = 0.0; rz = 0.0;
        // Let the chaotic systems settle onto their attractors.
        for (int i = 0; i < 3000; ++i) { stepLorenz (0.005); stepRossler (0.02); }
        out = 0.0f;
    }

    // Force the phase (tempo sync to host position). phase01 in [0, 1).
    void setPhase (double phase01) noexcept { phase = phase01 - std::floor (phase01); }

    // Advance by dtSeconds at rateHz and return the bipolar output (-1..1).
    float tick (LfoShape shape, float rateHz, double dtSeconds) noexcept
    {
        const double inc = (double) rateHz * dtSeconds;
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = rng.bi();
            smoothFrom = smoothTo;
            smoothTo = rng.bi();
        }

        const double p = phase;
        switch (shape)
        {
            case LfoShape::Sine:       out = (float) std::sin (2.0 * kPiD * p); break;
            case LfoShape::Triangle:   out = (float) (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p); break;
            case LfoShape::SawUp:      out = (float) (2.0 * p - 1.0); break;
            case LfoShape::SawDown:    out = (float) (1.0 - 2.0 * p); break;
            case LfoShape::Square:     out = p < 0.5 ? 1.0f : -1.0f; break;
            case LfoShape::SampleHold: out = held; break;
            case LfoShape::SmoothRandom:
            {
                const float s = (float) (p * p * (3.0 - 2.0 * p));
                out = smoothFrom + (smoothTo - smoothFrom) * s;
                break;
            }
            case LfoShape::Lorenz:
            {
                // ~1 orbit per second at 1 Hz; RK4 substeps of at most 0.01 time units.
                const double total = (double) rateHz * dtSeconds * 1.0;
                const int steps = std::max (1, (int) std::ceil (total / 0.01));
                const double h = total / steps;
                for (int i = 0; i < steps; ++i) stepLorenz (h);
                out = (float) clamp (lx / 20.0, -1.0, 1.0);
                break;
            }
            case LfoShape::Rossler:
            {
                // Period ~ 2*pi time units, so scale time to give ~1 orbit per second at 1 Hz.
                const double total = (double) rateHz * dtSeconds * 6.2832;
                const int steps = std::max (1, (int) std::ceil (total / 0.02));
                const double h = total / steps;
                for (int i = 0; i < steps; ++i) stepRossler (h);
                out = (float) clamp ((rx - 0.5) / 11.0, -1.0, 1.0);
                break;
            }
            case LfoShape::Count:
            default: out = 0.0f; break;
        }
        return out;
    }

    float value() const noexcept { return out; }

private:
    void stepLorenz (double h) noexcept
    {
        auto f = [] (double x, double y, double z, double& dx, double& dy, double& dz)
        {
            constexpr double sigma = 10.0, rho = 28.0, beta = 8.0 / 3.0;
            dx = sigma * (y - x);
            dy = x * (rho - z) - y;
            dz = x * y - beta * z;
        };
        rk4 (f, lx, ly, lz, h);
        if (! std::isfinite (lx) || std::abs (lx) > 1.0e3) { lx = 0.1; ly = 0.0; lz = 0.0; }
    }

    void stepRossler (double h) noexcept
    {
        auto f = [] (double x, double y, double z, double& dx, double& dy, double& dz)
        {
            constexpr double a = 0.2, b = 0.2, c = 5.7;
            dx = -y - z;
            dy = x + a * y;
            dz = b + z * (x - c);
        };
        rk4 (f, rx, ry, rz, h);
        if (! std::isfinite (rx) || std::abs (rx) > 1.0e3) { rx = 0.1; ry = 0.0; rz = 0.0; }
    }

    template <typename F>
    static void rk4 (F f, double& x, double& y, double& z, double h) noexcept
    {
        double k1x, k1y, k1z, k2x, k2y, k2z, k3x, k3y, k3z, k4x, k4y, k4z;
        f (x, y, z, k1x, k1y, k1z);
        f (x + 0.5 * h * k1x, y + 0.5 * h * k1y, z + 0.5 * h * k1z, k2x, k2y, k2z);
        f (x + 0.5 * h * k2x, y + 0.5 * h * k2y, z + 0.5 * h * k2z, k3x, k3y, k3z);
        f (x + h * k3x, y + h * k3y, z + h * k3z, k4x, k4y, k4z);
        x += h / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
        y += h / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);
        z += h / 6.0 * (k1z + 2.0 * k2z + 2.0 * k3z + k4z);
    }

    Random rng;
    double phase = 0.0;
    float held = 0.0f, smoothFrom = 0.0f, smoothTo = 0.0f, out = 0.0f;
    double lx = 0.1, ly = 0.0, lz = 0.0;
    double rx = 0.1, ry = 0.0, rz = 0.0;
};
} // namespace zarya
