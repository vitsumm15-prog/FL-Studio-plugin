// Zarya Synth — общие константы и утилиты DSP-ядра (без зависимостей от JUCE).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace zarya
{
constexpr float  kPi       = 3.14159265358979323846f;
constexpr float  kTwoPi    = 6.28318530717958647692f;
constexpr double kPiD      = 3.14159265358979323846;

constexpr int kNumOsc       = 3;
constexpr int kNumEnv       = 4;
constexpr int kNumLfo       = 4;
constexpr int kNumModSlots  = 8;
constexpr int kMaxVoices    = 16;
constexpr int kMaxUnison    = 7;
constexpr int kMaxGrains    = 32;
constexpr int kControlBlock = 32;   // шаг пересчёта модуляции, в сэмплах

// Корневая нота сэмплов (MIDI 60): сэмпл, сыгранный на этой ноте, звучит в оригинальной высоте.
constexpr float kSampleRootNote = 60.0f;

// Быстрый ГПСЧ xorshift32 — детерминированный и безопасный для аудиопотока.
struct Random
{
    uint32_t state = 0x9E3779B9u;

    void seed (uint32_t s) noexcept { state = (s != 0u) ? s : 0x9E3779B9u; }

    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uni() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0, 1)
    float bi() noexcept  { return uni() * 2.0f - 1.0f; }                           // [-1, 1)
};

template <typename T>
inline T clamp (T v, T lo, T hi) noexcept { return std::min (std::max (v, lo), hi); }

// Равномощная панорама: pan в [-1, 1], в центре оба канала = 0.7071.
inline void panGains (float pan, float& l, float& r) noexcept
{
    const float a = (clamp (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    l = std::cos (a);
    r = std::sin (a);
}

inline float midiToHz (float note) noexcept   { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float semisToRatio (float s) noexcept  { return std::exp2 (s / 12.0f); }
inline float dbToGain (float db) noexcept     { return std::pow (10.0f, db / 20.0f); }

// Кубическая интерполяция Эрмита по четырём точкам.
inline float hermite (float xm1, float x0, float x1, float x2, float t) noexcept
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

// Чтение сэмпла с зацикливанием индексов (для гранулярного режима и петли).
inline float readWrapped (const float* d, int len, double pos) noexcept
{
    if (len < 4) return 0.0f;
    double p = std::fmod (pos, (double) len);
    if (p < 0.0) p += (double) len;
    int i = (int) p;
    if (i >= len) i = len - 1;
    const float t = (float) (p - (double) i);
    const int im1 = (i == 0) ? len - 1 : i - 1;
    const int i1  = (i + 1) % len;
    const int i2  = (i + 2) % len;
    return hermite (d[im1], d[i], d[i1], d[i2], t);
}

// Чтение сэмпла с ограничением на краях (для однократного проигрывания).
inline float readClamped (const float* d, int len, double pos) noexcept
{
    if (len < 4 || pos < 0.0 || pos >= (double) (len - 1)) return 0.0f;
    const int i = (int) pos;
    const float t = (float) (pos - (double) i);
    const float xm1 = d[i > 0 ? i - 1 : 0];
    const float x1  = d[i + 1];
    const float x2  = d[i + 2 < len ? i + 2 : len - 1];
    return hermite (xm1, d[i], x1, x2, t);
}

// Музыкальные деления для синхронизации с темпом (в долях четвертной ноты).
constexpr int kNumDivisions = 15;
inline double divisionBeats (int index) noexcept
{
    static const double beats[kNumDivisions] = {
        16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0,
        0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125
    };
    return beats[clamp (index, 0, kNumDivisions - 1)];
}
inline const char* const* divisionNames() noexcept
{
    static const char* const names[kNumDivisions] = {
        "4/1", "2/1", "1/1", "1/2", "1/2T", "1/4.", "1/4", "1/4T",
        "1/8.", "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32"
    };
    return names;
}
} // namespace zarya
