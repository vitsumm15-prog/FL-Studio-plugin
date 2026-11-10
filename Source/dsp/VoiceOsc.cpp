// Zarya Synth - oscillator renderers of a voice: wavetable, sample and granular.
#include "Voice.h"

namespace zarya
{
namespace
{
constexpr float kMaxDetuneSemis = 1.0f; // unison spread at detune = 1
constexpr float kUnisonWidth    = 0.8f; // stereo spread of unison voices

inline float unisonOffset (int u, int count) noexcept
{
    return count <= 1 ? 0.0f : (float) u / (float) (count - 1) * 2.0f - 1.0f;
}

struct HannLut
{
    float v[1025];
    HannLut()
    {
        for (int i = 0; i <= 1024; ++i)
            v[i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPiD * (double) i / 1024.0);
    }
    float at (float x01) const noexcept
    {
        const float p = clamp (x01, 0.0f, 1.0f) * 1024.0f;
        const int i = std::min (1023, (int) p);
        return v[i] + (v[i + 1] - v[i]) * (p - (float) i);
    }
};
const HannLut& hannLut()
{
    static const HannLut t;
    return t;
}

struct UnisonSetup
{
    int count = 1;
    float ratio[kMaxUnison];
    float gl[kMaxUnison], gr[kMaxUnison];
    float norm = 1.0f;
    float maxRatio = 1.0f;

    UnisonSetup (const OscParams& op, float pan)
    {
        count = clamp (op.unison, 1, kMaxUnison);
        const float spread = clamp (op.detune, 0.0f, 1.0f) * kMaxDetuneSemis;
        for (int u = 0; u < count; ++u)
        {
            const float off = unisonOffset (u, count);
            ratio[u] = semisToRatio (off * spread);
            maxRatio = std::max (maxRatio, ratio[u]);
            panGains (clamp (pan + off * kUnisonWidth, -1.0f, 1.0f), gl[u], gr[u]);
        }
        norm = 1.0f / std::sqrt ((float) count);
    }
};
} // namespace

void Voice::renderWavetable (int o, const OscParams& op, int n, float* busL, float* busR)
{
    auto& st = osc[o];
    const auto& tab = WavetableBank::instance().table (op.table);
    const UnisonSetup us (op, oscPan[o]);
    double inc[kMaxUnison];
    for (int u = 0; u < us.count; ++u)
        inc[u] = (double) oscHz[o] * (double) us.ratio[u] / sr;

    const int level = WavetableBank::levelForFrequency (oscHz[o] * us.maxRatio, (float) sr);
    const float lv0 = st.first ? oscLevel[o] : st.prevLevel;
    const float ps0 = st.first ? oscPos[o] : st.prevPos;
    constexpr int N = WavetableBank::kTableSize;
    constexpr int lastFrame = WavetableBank::kFrames - 1;

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) / (float) n;
        const float lv = (lv0 + (oscLevel[o] - lv0) * t) * us.norm;
        const float fpos = (ps0 + (oscPos[o] - ps0) * t) * (float) lastFrame;
        const int f0 = clamp ((int) fpos, 0, lastFrame);
        const int f1 = std::min (f0 + 1, lastFrame);
        const float ft = fpos - (float) f0;
        const float* d0 = tab.get (f0, level);
        const float* d1 = tab.get (f1, level);

        float sl = 0.0f, sr2 = 0.0f;
        for (int u = 0; u < us.count; ++u)
        {
            double ph = st.phase[u];
            const double idx = ph * (double) N;
            const int i0 = std::min (N - 1, (int) idx);
            const float fr = (float) (idx - (double) i0);
            const float a = d0[i0] + (d0[i0 + 1] - d0[i0]) * fr;
            const float b = d1[i0] + (d1[i0 + 1] - d1[i0]) * fr;
            const float s = a + (b - a) * ft;
            sl += s * us.gl[u];
            sr2 += s * us.gr[u];
            ph += inc[u];
            if (ph >= 1.0) ph -= std::floor (ph);
            st.phase[u] = ph;
        }
        busL[i] += sl * lv;
        busR[i] += sr2 * lv;
    }
}

void Voice::renderSample (int o, const OscParams& op, const SampleData* s, int n, float* busL, float* busR)
{
    auto& st = osc[o];
    if (s == nullptr || s->length() < 4) return;
    const int len = s->length();
    const float* data = s->data.data();
    const UnisonSetup us (op, oscPan[o]);
    const double base = (double) oscHz[o] / (double) midiToHz (kSampleRootNote) * s->sampleRate / sr;
    const float lv0 = st.first ? oscLevel[o] : st.prevLevel;

    if (st.loopStart >= (double) (len - 8)) st.loopStart = 0.0;
    const double loopLen = (double) len - 1.0 - st.loopStart;
    // Short crossfade at the loop point to avoid clicks: the loop tail fades into the
    // material just before loopStart, so the wrap lands on a continuous signal.
    const double xfade = std::min (std::min (loopLen * 0.25, 0.01 * s->sampleRate), st.loopStart);

    for (int i = 0; i < n; ++i)
    {
        const float t = (float) (i + 1) / (float) n;
        const float lv = (lv0 + (oscLevel[o] - lv0) * t) * us.norm;
        float sl = 0.0f, sr2 = 0.0f;
        for (int u = 0; u < us.count; ++u)
        {
            if (! st.playing[u]) continue;
            double p = st.playPos[u];
            float v = 0.0f;
            if (op.loop && loopLen > 8.0)
            {
                const double loopEnd = st.loopStart + loopLen;
                while (p >= loopEnd) p -= loopLen;
                v = readClamped (data, len, p);
                const double toEnd = loopEnd - p;
                if (xfade > 1.0 && toEnd < xfade)
                {
                    const float w = (float) (toEnd / xfade); // 1 -> 0 approaching the end
                    const float head = readClamped (data, len, st.loopStart - toEnd);
                    v = v * w + head * (1.0f - w);
                }
            }
            else
            {
                if (p >= (double) (len - 2)) { st.playing[u] = false; continue; }
                v = readClamped (data, len, p);
            }
            sl += v * us.gl[u];
            sr2 += v * us.gr[u];
            p += base * (double) us.ratio[u];
            st.playPos[u] = p;
        }
        busL[i] += sl * lv;
        busR[i] += sr2 * lv;
    }
}

void Voice::spawnGrain (int o, const OscParams& op, const SampleData* s)
{
    auto& st = osc[o];
    Grain* g = nullptr;
    for (auto& cand : st.grains)
        if (! cand.active) { g = &cand; break; }
    if (g == nullptr) return;

    const int len = s->length();
    const float spray = clamp (op.grainSpray, 0.0f, 1.0f);
    double center = (double) oscPos[o] + st.scanOffset + 0.25 * (double) spray * (double) rng.bi();
    center -= std::floor (center);

    const int count = clamp (op.unison, 1, kMaxUnison);
    const float off = count > 1 ? unisonOffset ((int) (rng.next() % (uint32_t) count), count) : 0.0f;
    const float detune = semisToRatio (off * clamp (op.detune, 0.0f, 1.0f) * kMaxDetuneSemis);

    g->inc = (double) oscHz[o] / (double) midiToHz (kSampleRootNote) * (double) detune * s->sampleRate / sr;
    g->length = std::max (16, (int) (clamp (op.grainSizeMs, 5.0f, 1000.0f) * 0.001f * (float) sr));
    g->pos = center * (double) len;
    g->age = 0;
    const float pan = clamp (oscPan[o] + 0.7f * spray * rng.bi() + 0.5f * off, -1.0f, 1.0f);
    panGains (pan, g->gl, g->gr);
    g->active = true;
}

void Voice::renderGranular (int o, const OscParams& op, const SampleData* s, int n, float* busL, float* busR)
{
    auto& st = osc[o];
    if (s == nullptr || s->length() < 4) return;
    const int len = s->length();
    const float* data = s->data.data();
    const auto& win = hannLut();

    const float density = clamp (op.grainDensity, 1.0f, 200.0f);
    const float overlap = density * clamp (op.grainSizeMs, 5.0f, 1000.0f) * 0.001f;
    const float norm = 1.0f / std::sqrt (std::max (1.0f, overlap));
    const double interval = sr / (double) density;
    const double lenSec = (double) len / s->sampleRate;
    const double scanInc = (double) op.grainScan / (lenSec * sr);
    const float lv0 = st.first ? oscLevel[o] : st.prevLevel;
    const float spray = clamp (op.grainSpray, 0.0f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        st.grainTimer -= 1.0;
        if (st.grainTimer <= 0.0)
        {
            spawnGrain (o, op, s);
            st.grainTimer += interval * (1.0 + 0.3 * (double) spray * (double) rng.bi());
            if (st.grainTimer < 1.0) st.grainTimer = 1.0;
        }
        st.scanOffset += scanInc;
        st.scanOffset -= std::floor (st.scanOffset);

        float sl = 0.0f, sr2 = 0.0f;
        for (auto& g : st.grains)
        {
            if (! g.active) continue;
            const float w = win.at ((float) g.age / (float) g.length);
            const float v = readWrapped (data, len, g.pos) * w;
            sl += v * g.gl;
            sr2 += v * g.gr;
            g.pos += g.inc;
            if (++g.age >= g.length) g.active = false;
        }
        const float t = (float) (i + 1) / (float) n;
        const float lv = (lv0 + (oscLevel[o] - lv0) * t) * norm;
        busL[i] += sl * lv;
        busR[i] += sr2 * lv;
    }
}
} // namespace zarya
