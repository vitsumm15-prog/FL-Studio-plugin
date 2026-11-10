// Zarya Synth - engine implementation.
#include "SynthEngine.h"

namespace zarya
{
void SynthEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    for (auto& v : voices) v.prepare (sr);
    arp.prepare (sr);
    arpEvents.reserve ((size_t) std::max (256, maxBlockSize));
    chorus.prepare (sr);
    delay.prepare (sr);
    reverb.prepare (sr);
    WavetableBank::instance(); // make sure tables exist before audio starts
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices) v.reset();
    for (int i = 0; i < kNumLfo; ++i) lfos[i].reset (0xC0FFEEu + (uint32_t) i * 7919u);
    arp.reset();
    arpWasOn = false;
    chorus.reset();
    delay.reset();
    reverb.reset();
    std::fill (std::begin (keyDown), std::end (keyDown), false);
    monoCount = 0;
    sustainDown = false;
    bend = 0.0f;
    gmod = GlobalMod();
    lastNote = -1.0f;
    firstBlock = true;
}

int SynthEngine::activeVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices) n += v.isActive() ? 1 : 0;
    return n;
}

uint32_t SynthEngine::nextSeed() noexcept
{
    seedState = seedState * 1664525u + 1013904223u;
    return seedState ^ (seedState >> 15);
}

Voice* SynthEngine::findVoiceFor (int note, int polyphony)
{
    // 1) same note already sounding -> retrigger it (avoids stacked duplicates)
    for (int i = 0; i < polyphony; ++i)
        if (voices[i].isActive() && voices[i].getNote() == note)
            return &voices[i];
    // 2) free voice
    for (int i = 0; i < polyphony; ++i)
        if (! voices[i].isActive())
            return &voices[i];
    // 3) steal: quietest released voice, otherwise the oldest one
    Voice* best = nullptr;
    for (int i = 0; i < polyphony; ++i)
        if (voices[i].isReleased() && (best == nullptr || voices[i].getLevel() < best->getLevel()))
            best = &voices[i];
    if (best != nullptr) return best;
    for (int i = 0; i < polyphony; ++i)
        if (best == nullptr || voices[i].startOrder < best->startOrder)
            best = &voices[i];
    return best;
}

void SynthEngine::noteOn (int note, float vel, const SynthParams& p, const SampleData* const* samples)
{
    const auto mode = (VoiceMode) clamp (p.voice.mode, 0, (int) VoiceMode::Count - 1);
    if (mode == VoiceMode::Poly)
    {
        const int poly = clamp (p.voice.polyphony, 1, kMaxVoices);
        if (Voice* v = findVoiceFor (note, poly))
        {
            v->startOrder = ++orderCounter;
            v->start (note, vel, false, lastNote, p, samples, nextSeed());
        }
    }
    else
    {
        // remove the note if already on the stack, then push it on top
        for (int i = 0; i < monoCount; ++i)
            if (monoStack[i] == note)
            {
                for (int j = i; j < monoCount - 1; ++j) { monoStack[j] = monoStack[j + 1]; monoVel[j] = monoVel[j + 1]; }
                --monoCount;
                break;
            }
        if (monoCount < 128) { monoStack[monoCount] = note; monoVel[monoCount] = vel; ++monoCount; }

        Voice& v = voices[0];
        const bool legato = mode == VoiceMode::Legato && v.isActive() && ! v.isReleased() && monoCount > 1;
        v.startOrder = ++orderCounter;
        v.start (note, vel, legato, v.isActive() ? (float) v.getNote() : lastNote, p, samples, nextSeed());
        // other voices may still ring after a Poly -> Mono switch
        for (int i = 1; i < kMaxVoices; ++i)
            if (voices[i].isActive() && ! voices[i].isReleased()) voices[i].release();
    }
    lastNote = (float) note;
}

void SynthEngine::releaseVoice (Voice& v)
{
    if (sustainDown) v.sustained = true;
    else v.release();
}

void SynthEngine::noteOff (int note, const SynthParams& p, const SampleData* const* samples)
{
    const auto mode = (VoiceMode) clamp (p.voice.mode, 0, (int) VoiceMode::Count - 1);
    if (mode == VoiceMode::Poly)
    {
        for (auto& v : voices)
            if (v.isActive() && ! v.isReleased() && v.getNote() == note)
                releaseVoice (v);
        return;
    }

    int idx = -1;
    for (int i = 0; i < monoCount; ++i)
        if (monoStack[i] == note) { idx = i; break; }
    if (idx < 0)
    {
        // note may belong to a voice started in Poly mode before switching
        for (auto& v : voices)
            if (v.isActive() && ! v.isReleased() && v.getNote() == note) releaseVoice (v);
        return;
    }
    const bool wasTop = idx == monoCount - 1;
    for (int j = idx; j < monoCount - 1; ++j) { monoStack[j] = monoStack[j + 1]; monoVel[j] = monoVel[j + 1]; }
    --monoCount;

    Voice& v = voices[0];
    if (! wasTop || v.getNote() != note) return;
    if (monoCount > 0)
    {
        // fall back to the previously held key
        const bool legato = mode == VoiceMode::Legato;
        v.start (monoStack[monoCount - 1], monoVel[monoCount - 1], legato, (float) note, p, samples, nextSeed());
        lastNote = (float) monoStack[monoCount - 1];
    }
    else
    {
        releaseVoice (v);
    }
}

void SynthEngine::allNotesOff()
{
    for (auto& v : voices) { v.sustained = false; v.release(); }
    monoCount = 0;
    sustainDown = false;
}

void SynthEngine::handleEvent (const MidiEvent& e, const SynthParams& p, const SampleData* const* samples)
{
    switch (e.type)
    {
        case MidiEvent::NoteOn:
            if (e.value <= 0.0f) noteOff (e.note, p, samples);
            else noteOn (e.note, e.value, p, samples);
            break;
        case MidiEvent::NoteOff:   noteOff (e.note, p, samples); break;
        case MidiEvent::PitchBend: bend = clamp (e.value, -1.0f, 1.0f); break;
        case MidiEvent::ModWheel:  gmod.modWheel = clamp (e.value, 0.0f, 1.0f); break;
        case MidiEvent::Sustain:
        {
            const bool down = e.value >= 0.5f;
            if (sustainDown && ! down)
                for (auto& v : voices)
                    if (v.sustained) { v.sustained = false; v.release(); }
            sustainDown = down;
            break;
        }
        case MidiEvent::AllNotesOff: allNotesOff(); break;
        default: break;
    }
}

void SynthEngine::renderSpan (float* outL, float* outR, int start, int n, const SynthParams& p,
                              const TransportInfo& tr, const SampleData* const* samples)
{
    int pos = start;
    const int end = start + n;
    const double bpm = clamp (tr.bpm, 20.0, 999.0);
    while (pos < end)
    {
        const int len = std::min (kControlBlock, end - pos);
        const double dt = (double) len / sr;
        for (int i = 0; i < kNumLfo; ++i)
        {
            const auto& lp = p.lfo[i];
            const auto shape = (LfoShape) clamp (lp.shape, 0, (int) LfoShape::Count - 1);
            float rate = clamp (lp.rateHz, 0.01f, 50.0f);
            if (lp.sync)
            {
                const double beats = divisionBeats (lp.division);
                rate = (float) (bpm / 60.0 / beats);
                const bool periodic = shape != LfoShape::Lorenz && shape != LfoShape::Rossler;
                if (periodic && tr.isPlaying && tr.hasPpq)
                {
                    const double ppqNow = tr.ppq + (double) (samplesProcessed + pos) / sr * bpm / 60.0;
                    lfos[i].setPhase (ppqNow / beats);
                }
            }
            gmod.lfo[i] = lfos[i].tick (shape, rate, dt);
        }
        gmod.bendSemis = bend * (float) clamp (p.voice.bendRange, 0, 24);

        for (auto& v : voices)
            if (v.isActive())
                v.render (outL + pos, outR + pos, len, p, gmod, samples);
        pos += len;
    }
}

void SynthEngine::process (float* outL, float* outR, int numSamples, const SynthParams& p,
                           const MidiEvent* events, int numEvents, const TransportInfo& tr,
                           const SampleData* const* samples)
{
    std::fill (outL, outL + numSamples, 0.0f);
    std::fill (outR, outR + numSamples, 0.0f);
    samplesProcessed = 0;

    // Track physical keys (needed to seed the arpeggiator when it is switched on).
    for (int i = 0; i < numEvents; ++i)
    {
        const auto& e = events[i];
        if (e.note < 0 || e.note > 127) continue;
        if (e.type == MidiEvent::NoteOn && e.value > 0.0f) { keyDown[e.note] = true; keyVel[e.note] = e.value; }
        else if (e.type == MidiEvent::NoteOn || e.type == MidiEvent::NoteOff) keyDown[e.note] = false;
    }

    const MidiEvent* ev = events;
    int numEv = numEvents;
    if (p.arp.on)
    {
        if (! arpWasOn)
        {
            for (auto& v : voices) if (v.isActive() && ! v.isReleased()) v.release();
            monoCount = 0;
            arp.reset();
        }
        arp.process (events, numEvents, numSamples, tr.bpm, p.arp, arpEvents);
        ev = arpEvents.data();
        numEv = (int) arpEvents.size();
    }
    else if (arpWasOn)
    {
        const int n = arp.stop();
        if (n >= 0) noteOff (n, p, samples);
        // keys still held when the arp is switched off start sounding normally
        for (int k = 0; k < 128; ++k)
            if (keyDown[k]) noteOn (k, keyVel[k], p, samples);
    }
    arpWasOn = p.arp.on;

    int pos = 0;
    for (int i = 0; i < numEv; ++i)
    {
        const int at = clamp (ev[i].sample, 0, numSamples);
        if (at > pos) { renderSpan (outL, outR, pos, at - pos, p, tr, samples); pos = at; }
        handleEvent (ev[i], p, samples);
    }
    if (pos < numSamples) renderSpan (outL, outR, pos, numSamples - pos, p, tr, samples);

    // Effects chain.
    const auto& fx = p.fx;
    if (fx.distOn)   Distortion::process (outL, outR, numSamples, fx.distDrive, fx.distMix);
    if (fx.chorusOn) chorus.process (outL, outR, numSamples, fx.chorusRate, fx.chorusDepth, fx.chorusMix);
    if (fx.delayOn)
    {
        const double secs = divisionBeats (fx.delayDivision) * 60.0 / clamp (tr.bpm, 20.0, 999.0);
        delay.process (outL, outR, numSamples, (float) std::min (secs, 3.9), fx.delayFeedback, fx.delayMix, fx.delayPingPong);
    }
    if (fx.reverbOn) reverb.process (outL, outR, numSamples, fx.reverbSize, fx.reverbDamp, fx.reverbMix);

    // Master gain with smoothing, a soft safety limiter (linear below 0.8, ceiling 1.0) and a NaN guard.
    const float target = dbToGain (clamp (p.masterDb, -60.0f, 6.0f));
    if (firstBlock) { masterGain = target; firstBlock = false; }
    const float step = (target - masterGain) / (float) std::max (1, numSamples);
    auto soft = [] (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.8f) return x;
        const float y = 0.8f + 0.2f * std::tanh ((a - 0.8f) * 5.0f);
        return x < 0.0f ? -y : y;
    };
    bool bad = false;
    for (int i = 0; i < numSamples; ++i)
    {
        masterGain += step;
        outL[i] = soft (outL[i] * masterGain);
        outR[i] = soft (outR[i] * masterGain);
        if (! std::isfinite (outL[i]) || ! std::isfinite (outR[i])) bad = true;
    }
    if (bad)
    {
        std::fill (outL, outL + numSamples, 0.0f);
        std::fill (outR, outR + numSamples, 0.0f);
        for (auto& v : voices) v.reset();
        chorus.reset(); delay.reset(); reverb.reset();
    }
    masterGain = target;
}
} // namespace zarya
