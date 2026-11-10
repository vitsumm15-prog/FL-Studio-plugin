// Zarya Synth - procedural wavetable bank.
#include "Wavetable.h"

#include <complex>

namespace zarya
{
namespace
{
using cd = std::complex<double>;

// Iterative radix-2 FFT (size must be a power of two).
void fft (std::vector<cd>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * kPiD / (double) len * (inverse ? 1.0 : -1.0);
        const size_t half = len / 2;
        for (size_t i = 0; i < n; i += len)
        {
            for (size_t j = 0; j < half; ++j)
            {
                const cd w (std::cos (ang * (double) j), std::sin (ang * (double) j));
                const cd u = a[i + j];
                const cd v = a[i + j + half] * w;
                a[i + j]        = u + v;
                a[i + j + half] = u - v;
            }
        }
    }

    if (inverse)
        for (auto& x : a)
            x /= (double) n;
}

double frac (double x) { return x - std::floor (x); }

double shapeSine (double x) { return std::sin (2.0 * kPiD * x); }
double shapeTri (double x)
{
    x = frac (x);
    if (x < 0.25) return 4.0 * x;
    if (x < 0.75) return 2.0 - 4.0 * x;
    return 4.0 * x - 4.0;
}
double shapeSaw (double x)    { return 2.0 * frac (x + 0.5) - 1.0; }
double shapeSquare (double x) { return frac (x) < 0.5 ? 1.0 : -1.0; }

void additive (double* out, const std::vector<double>& amps)
{
    const int n = WavetableBank::kTableSize;
    for (int i = 0; i < n; ++i)
        out[i] = 0.0;
    for (size_t k = 1; k < amps.size(); ++k)
    {
        const double a = amps[k];
        if (std::abs (a) < 1.0e-9)
            continue;
        for (int i = 0; i < n; ++i)
            out[i] += a * std::sin (2.0 * kPiD * (double) k * (double) i / (double) n);
    }
}

struct Formant { double f1, f2, f3; };

// One frame in the time domain. t is the frame position 0..1.
void generateFrame (int family, double t, double* out)
{
    const int n = WavetableBank::kTableSize;

    switch (family)
    {
        case 0: // Basic: sine -> triangle -> saw -> square
        {
            const double s = t * 3.0;
            const int seg = std::min (2, (int) s);
            const double m = s - (double) seg;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / (double) n;
                double a = 0.0, b = 0.0;
                if (seg == 0)      { a = shapeSine (x); b = shapeTri (x); }
                else if (seg == 1) { a = shapeTri (x);  b = shapeSaw (x); }
                else               { a = shapeSaw (x);  b = shapeSquare (x); }
                out[i] = a + (b - a) * m;
            }
            break;
        }
        case 1: // PWM: pulse width 50% -> 5%
        {
            const double w = 0.5 - 0.45 * t;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / (double) n;
                out[i] = x < w ? 1.0 : -1.0;
            }
            break;
        }
        case 2: // Harmonic sweep: saw spectrum with growing harmonic count (1..256)
        {
            const double count = std::pow (2.0, t * 8.0);
            std::vector<double> amps (258, 0.0);
            for (int k = 1; k <= 256; ++k)
            {
                const double fade = clamp (count - (double) k + 1.0, 0.0, 1.0);
                amps[(size_t) k] = fade / (double) k;
            }
            additive (out, amps);
            break;
        }
        case 3: // Vowels: formant spectra a - e - i - o - u over a 110 Hz fundamental
        {
            static const Formant vowels[5] = {
                { 730.0, 1090.0, 2440.0 }, { 530.0, 1840.0, 2480.0 }, { 270.0, 2290.0, 3010.0 },
                { 570.0,  840.0, 2410.0 }, { 300.0,  870.0, 2240.0 }
            };
            const double s = t * 4.0;
            const int seg = std::min (3, (int) s);
            const double m = s - (double) seg;
            const Formant& a = vowels[seg];
            const Formant& b = vowels[seg + 1];
            const double f1 = a.f1 + (b.f1 - a.f1) * m;
            const double f2 = a.f2 + (b.f2 - a.f2) * m;
            const double f3 = a.f3 + (b.f3 - a.f3) * m;
            std::vector<double> amps (201, 0.0);
            for (int k = 1; k <= 200; ++k)
            {
                const double f = 110.0 * (double) k;
                const double g = std::exp (-std::pow ((f - f1) / 90.0, 2.0))
                               + 0.5 * std::exp (-std::pow ((f - f2) / 110.0, 2.0))
                               + 0.25 * std::exp (-std::pow ((f - f3) / 140.0, 2.0));
                amps[(size_t) k] = (g + 0.002) / std::sqrt ((double) k);
            }
            additive (out, amps);
            break;
        }
        case 4: // Sync: hard-synced saw, slave ratio 1 -> 8
        {
            const double r = 1.0 + 7.0 * t;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / (double) n;
                out[i] = 2.0 * frac (x * r) - 1.0;
            }
            break;
        }
        case 5: // FM: 1:1 phase modulation, index 0 -> 8
        {
            const double index = 8.0 * t;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / (double) n;
                out[i] = std::sin (2.0 * kPiD * x + index * std::sin (2.0 * kPiD * x));
            }
            break;
        }
        case 6: // Fold: sine wavefolder, gain 1 -> 8
        {
            const double g = 1.0 + 7.0 * t;
            for (int i = 0; i < n; ++i)
            {
                const double x = (double) i / (double) n;
                out[i] = std::sin (0.5 * kPiD * g * std::sin (2.0 * kPiD * x));
            }
            break;
        }
        default: // Organ: drawbar registrations morphing across frames
        {
            static const int harm[9] = { 1, 2, 3, 4, 5, 6, 8, 10, 16 };
            static const double reg[4][9] = {
                { 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
                { 1.0, 0.8, 0.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
                { 1.0, 0.5, 0.7, 0.6, 0.0, 0.4, 0.3, 0.0, 0.0 },
                { 1.0, 0.8, 0.8, 0.8, 0.5, 0.7, 0.7, 0.5, 0.6 }
            };
            const double s = t * 3.0;
            const int seg = std::min (2, (int) s);
            const double m = s - (double) seg;
            std::vector<double> amps (17, 0.0);
            for (int j = 0; j < 9; ++j)
                amps[(size_t) harm[j]] = reg[seg][j] + (reg[seg + 1][j] - reg[seg][j]) * m;
            additive (out, amps);
            break;
        }
    }
}

const char* const kTableIds[WavetableBank::kNumTables] = {
    "Basic", "PWM", "Harmonic Sweep", "Vowels", "Sync", "FM", "Fold", "Organ"
};
} // namespace

const char* const* WavetableBank::tableNames() noexcept { return kTableIds; }

const WavetableBank& WavetableBank::instance()
{
    static const WavetableBank bank;
    return bank;
}

int WavetableBank::levelForFrequency (float hz, float sampleRate) noexcept
{
    if (hz <= 1.0f)
        return 0;
    const float limit = std::max (0.5f * sampleRate, sampleRate - 18000.0f);
    const float maxHarmonics = limit / hz;
    for (int level = 0; level < kLevels; ++level)
        if ((float) (1024 >> level) <= maxHarmonics)
            return level;
    return kLevels - 1;
}

WavetableBank::WavetableBank()
{
    const int n = kTableSize;
    std::vector<double> frame ((size_t) n);
    std::vector<cd> spectrum ((size_t) n), work ((size_t) n);

    tables.resize (kNumTables);
    for (int ti = 0; ti < kNumTables; ++ti)
    {
        Table& tab = tables[(size_t) ti];
        tab.name = kTableIds[ti];
        tab.data.assign ((size_t) kFrames * kLevels * kStride, 0.0f);

        for (int f = 0; f < kFrames; ++f)
        {
            generateFrame (ti, (double) f / (double) (kFrames - 1), frame.data());

            for (int i = 0; i < n; ++i)
                spectrum[(size_t) i] = cd (frame[(size_t) i], 0.0);
            fft (spectrum, false);
            spectrum[0] = cd (0.0, 0.0);              // remove DC
            spectrum[(size_t) n / 2] = cd (0.0, 0.0); // remove Nyquist

            float scale = 1.0f;
            for (int level = 0; level < kLevels; ++level)
            {
                const int maxHarm = std::min (1023, 1024 >> level);
                for (int k = 0; k < n; ++k)
                {
                    const int h = (k <= n / 2) ? k : n - k;
                    work[(size_t) k] = (h >= 1 && h <= maxHarm) ? spectrum[(size_t) k] : cd (0.0, 0.0);
                }
                fft (work, true);

                if (level == 0)
                {
                    double peak = 0.0;
                    for (int i = 0; i < n; ++i)
                        peak = std::max (peak, std::abs (work[(size_t) i].real()));
                    scale = peak > 1.0e-9 ? (float) (0.95 / peak) : 0.0f;
                }

                float* dst = tab.data.data() + ((size_t) f * kLevels + (size_t) level) * kStride;
                for (int i = 0; i < n; ++i)
                    dst[i] = (float) work[(size_t) i].real() * scale;
                dst[n] = dst[0];
            }
        }
    }
}
} // namespace zarya
