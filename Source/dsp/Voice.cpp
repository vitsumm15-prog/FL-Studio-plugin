// Zarya Synth - voice implementation.
#include "Voice.h"

namespace zarya
{
namespace
{
constexpr float kPitchModRange  = 24.0f; // semitones at amount = 1
constexpr float kCutoffModRange = 60.0f; // semitones (5 octaves) at amount = 1
constexpr float kMaxDetuneSemis = 1.0f;  // unison spread at detune = 1

inline float unisonOffset (int u, int count) noexcept
{
    return count <= 1 ? 0.0f : (float) u / (float) (count - 1) * 2.0f - 1.0f;
}

// Hann window lookup (1024 points + guard).
struct HannTable
{
    float v[1025];
    HannTable()
    {
        for (int i = 0; i <= 1024; ++i)
            v[i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPiD * (double) i / 1024.0);
    }
    float operator() (float x01) const noexcept
    {
        const float p = clamp (x01, 0.0f, 1.0f) * 1024.0f;
        const int i = std::min (1023, (int) p);
        return v[i] + (v[i + 1] - v[i]) * (p - (float) i);
    }
};
const HannTable& hann()
{
    static const HannTable t;
    return t;
}
} // namespace

void Voice::prepare (double sampleRate)
{
    sr = sampleRate;
    ampEnv.setSampleRate (sr);
    for (auto& e : modEnv) e.setSampleRate (sr);
    hann();
    reset();
}

void Voice::reset() noexcept
{
    active = false;
    released = false;
    sustained = false;
    ampEnv.reset();
    for (auto& e : modEnv) e.reset();
    filter.reset();
    for (auto& o : osc)
    {
        for (auto& g : o.grains) g.active = false;
        o.first = true;
    }
    firstBlock = true;
}

void Voice::start (int newNote, float vel, bool legato, float glideFrom, const SynthParams& p,
                   const SampleData* const* samples, uint32_t seed)
{
    note = newNote;
    targetNote = (float) newNote;
    sustained = false;

    if (legato && active && ! released)
        return; // envelopes keep running, pitch glides to targetNote

    released = false;
    const bool wasSounding = active && ampEnv.getLevel() > 1.0e-4f;
    rng.seed (seed);
    velocity = clamp (vel, 0.0f, 1.0f);
    voiceRandom = rng.bi();
    currentNote = (p.voice.glide > 0.0005f && glideFrom >= 0.0f) ? glideFrom : targetNote;

    for (int o = 0; o < kNumOsc; ++o)
    {
        auto& st = osc[o];
        const auto& op = p.osc[o];
        const SampleData* s = samples != nullptr ? samples[o] : nullptr;
        const int len = s != nullptr ? s->length() : 0;
        st.loopStart = len > 0 ? clamp ((double) op.position, 0.0, 0.999) * (double) len : 0.0;
        for (int u = 0; u < kMaxUnison; ++u)
        {
            if (! wasSounding)
                st.phase[u] = op.unison > 1 ? (double) rng.uni() : 0.0;
            st.playPos[u] = st.loopStart;
            st.playing[u] = true;
        }
        st.grainTimer = 0.0;
        st.scanOffset = 0.0;
        if (! wasSounding)
        {
            for (auto& g : st.grains) g.active = false;
            st.first = true;
        }
    }

    if (! wasSounding)
    {
        filter.reset();
        firstBlock = true;
    }
    ampEnv.noteOn();
    for (auto& e : modEnv) e.noteOn();
    active = true;
}

void Voice::release() noexcept
{
    if (! active) return;
    released = true;
    ampEnv.noteOff();
    for (auto& e : modEnv) e.noteOff();
}

void Voice::computeControl (const SynthParams& p, const GlobalMod& g, int n)
{
    ampEnv.setParams (p.env[0].attack, p.env[0].decay, p.env[0].sustain, p.env[0].release);
    for (int e = 1; e < kNumEnv; ++e)
        modEnv[e - 1].setParams (p.env[e].attack, p.env[e].decay, p.env[e].sustain, p.env[e].release);

    float src[(int) ModSource::Count] = {};
    for (int e = 0; e < kNumEnv - 1; ++e)
        src[(int) ModSource::Env2 + e] = modEnv[e].advance (n);
    for (int l = 0; l < kNumLfo; ++l)
        src[(int) ModSource::Lfo1 + l] = g.lfo[l];
    src[(int) ModSource::Velocity] = velocity;
    src[(int) ModSource::ModWheel] = g.modWheel;
    src[(int) ModSource::Note]     = ((float) note - 60.0f) / 60.0f;
    src[(int) ModSource::Random]   = voiceRandom;

    float d[(int) ModDest::Count] = {};
    for (const auto& slot : p.mod)
    {
        if (slot.source <= 0 || slot.source >= (int) ModSource::Count) continue;
        if (slot.dest <= 0 || slot.dest >= (int) ModDest::Count) continue;
        d[slot.dest] += src[slot.source] * slot.amount;
    }

    if (p.voice.glide > 0.0005f)
    {
        const float c = 1.0f - std::exp (-(float) n / (p.voice.glide * (float) sr));
        currentNote += (targetNote - currentNote) * c;
        if (std::abs (targetNote - currentNote) < 0.001f) currentNote = targetNote;
    }
    else
    {
        currentNote = targetNote;
    }

    const float basePitch = currentNote + g.bendSemis + d[(int) ModDest::PitchAll] * kPitchModRange;
    for (int o = 0; o < kNumOsc; ++o)
    {
        const auto& op = p.osc[o];
        const float semis = basePitch + 12.0f * (float) op.octave + (float) op.semitone + 0.01f * op.fine
                          + d[(int) ModDest::Osc1Pitch + o] * kPitchModRange;
        oscHz[o]    = clamp (midiToHz (semis), 0.5f, (float) (0.45 * sr));
        oscLevel[o] = clamp (op.level + d[(int) ModDest::Osc1Level + o], 0.0f, 1.0f);
        oscPos[o]   = clamp (op.position + d[(int) ModDest::Osc1Pos + o], 0.0f, 1.0f);
        oscPan[o]   = clamp (op.pan + d[(int) ModDest::Osc1Pan + o], -1.0f, 1.0f);
    }

    const float cutSemis = d[(int) ModDest::Cutoff] * kCutoffModRange + p.filter.keytrack * (currentNote - 60.0f);
    prevFilterG = filterG;
    filterG = StereoFilter::cutoffToG (p.filter.cutoff * semisToRatio (cutSemis), sr);
    filterK = StereoFilter::resonanceToK (p.filter.resonance + d[(int) ModDest::Resonance]);
    filterDrive = clamp (p.filter.drive + d[(int) ModDest::Drive], 0.0f, 1.0f);

    const float velGain = 1.0f - clamp (p.voice.velSens, 0.0f, 1.0f) * (1.0f - velocity);
    prevAmpGain = ampGain;
    ampGain = velGain * clamp (1.0f + d[(int) ModDest::Amp], 0.0f, 2.0f);

    if (firstBlock)
    {
        prevFilterG = filterG;
        prevAmpGain = ampGain;
        firstBlock = false;
    }
}

void Voice::render (float* outL, float* outR, int numSamples, const SynthParams& p,
                    const GlobalMod& g, const SampleData* const* samples)
{
    if (! active) return;

    float fL[kControlBlock], fR[kControlBlock], dL[kControlBlock], dR[kControlBlock];
    const auto type = (FilterType) clamp (p.filter.type, 0, (int) FilterType::Count - 1);
    int pos = 0;

    while (pos < numSamples && active)
    {
        const int n = std::min (kControlBlock, numSamples - pos);
        computeControl (p, g, n);
        std::fill (fL, fL + n, 0.0f); std::fill (fR, fR + n, 0.0f);
        std::fill (dL, dL + n, 0.0f); std::fill (dR, dR + n, 0.0f);

        for (int o = 0; o < kNumOsc; ++o)
        {
            const auto& op = p.osc[o];
            if (! op.on) continue;
            const bool viaFilter = op.toFilter && p.filter.on;
            renderOsc (o, op, samples != nullptr ? samples[o] : nullptr, n,
                       viaFilter ? fL : dL, viaFilter ? fR : dR);
        }

        if (p.filter.on)
        {
            const float dg = dbToGain (24.0f * filterDrive);
            const float makeup = 1.0f / std::pow (dg, 0.35f);
            const bool drive = filterDrive > 0.001f;
            for (int i = 0; i < n; ++i)
            {
                const float gg = prevFilterG + (filterG - prevFilterG) * (float) (i + 1) / (float) n;
                float xl = fL[i], xr = fR[i];
                if (drive)
                {
                    xl = std::tanh (xl * dg) * makeup;
                    xr = std::tanh (xr * dg) * makeup;
                }
                fL[i] = filter.process (0, xl, type, gg, filterK);
                fR[i] = filter.process (1, xr, type, gg, filterK);
            }
            if (! std::isfinite (filter.s1[0].ic1) || ! std::isfinite (filter.s1[1].ic1)
                || ! std::isfinite (filter.s2[0].ic1) || ! std::isfinite (filter.s2[1].ic1))
                filter.reset();
        }

        for (int i = 0; i < n; ++i)
        {
            const float env = ampEnv.process();
            const float gain = env * (prevAmpGain + (ampGain - prevAmpGain) * (float) (i + 1) / (float) n);
            outL[pos + i] += (fL[i] + dL[i]) * gain;
            outR[pos + i] += (fR[i] + dR[i]) * gain;
        }

        if (! ampEnv.isActive())
        {
            active = false;
            released = false;
            sustained = false;
        }
        pos += n;
    }
}

void Voice::renderOsc (int o, const OscParams& op, const SampleData* s, int n, float* busL, float* busR)
{
    switch ((OscMode) clamp (op.mode, 0, (int) OscMode::Count - 1))
    {
        case OscMode::Sample:   renderSample (o, op, s, n, busL, busR); break;
        case OscMode::Granular: renderGranular (o, op, s, n, busL, busR); break;
        case OscMode::Wavetable:
        case OscMode::Count:
        default:                renderWavetable (o, op, n, busL, busR); break;
    }
    osc[o].prevLevel = oscLevel[o];
    osc[o].prevPos = oscPos[o];
    osc[o].first = false;
}
} // namespace zarya
