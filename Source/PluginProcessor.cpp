// Zarya Synth - JUCE audio processor implementation.
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace zarya;

namespace
{
const juce::Identifier kSamplesNode ("SAMPLES");
juce::String samplePathKey (int osc) { return "path" + juce::String (osc); }
constexpr double kMaxSampleSeconds = 60.0;
} // namespace

ZaryaProcessor::ZaryaProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ZARYA", createLayout())
{
    formats.registerBasicFormats();
    WavetableBank::instance(); // build tables now, not on the audio thread
    defaultSample = SampleData::makeDefault (48000.0);
    for (int o = 0; o < kNumOsc; ++o)
    {
        samples[o] = defaultSample;
        audioSamples[o] = defaultSample;
    }
    events.reserve (4096);
    bindParams();
    startTimerHz (2);
}

ZaryaProcessor::~ZaryaProcessor() { stopTimer(); }

bool ZaryaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ZaryaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void ZaryaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() <= 0) return;

    // Notes from the on-screen keyboard are merged into the host MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    readParams();

    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked())
            for (int o = 0; o < kNumOsc; ++o)
                if (audioSamples[o] != samples[o]) audioSamples[o] = samples[o]; // old one stays alive in graveyard
    }
    const SampleData* raw[kNumOsc];
    for (int o = 0; o < kNumOsc; ++o) raw[o] = audioSamples[o].get();

    events.clear();
    for (const auto meta : midi)
    {
        if (events.size() >= events.capacity()) break; // never allocate on the audio thread
        const auto& m = meta.getMessage();
        MidiEvent e;
        e.sample = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        if (m.isNoteOn())                     { e.type = MidiEvent::NoteOn;  e.note = m.getNoteNumber(); e.value = m.getFloatVelocity(); }
        else if (m.isNoteOff())               { e.type = MidiEvent::NoteOff; e.note = m.getNoteNumber(); }
        else if (m.isPitchWheel())            { e.type = MidiEvent::PitchBend; e.value = (float) (m.getPitchWheelValue() - 8192) / 8192.0f; }
        else if (m.isControllerOfType (1))    { e.type = MidiEvent::ModWheel; e.value = (float) m.getControllerValue() / 127.0f; }
        else if (m.isSustainPedalOn())        { e.type = MidiEvent::Sustain; e.value = 1.0f; }
        else if (m.isSustainPedalOff())       { e.type = MidiEvent::Sustain; e.value = 0.0f; }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { e.type = MidiEvent::AllNotesOff; }
        else continue;
        events.push_back (e);
    }

    TransportInfo tr;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm()) tr.bpm = *bpm;
            if (const auto ppq = pos->getPpqPosition()) { tr.ppq = *ppq; tr.hasPpq = true; }
            tr.isPlaying = pos->getIsPlaying();
        }
    }

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (right == nullptr)
    {
        // Mono output: render stereo into a stack buffer chunk-wise and sum.
        float tmpL[512], tmpR[512];
        int done = 0;
        while (done < numSamples)
        {
            const int n = std::min (512, numSamples - done);
            int first = 0, count = 0;
            for (size_t i = 0; i < events.size(); ++i)
                if (events[i].sample < done) ++first;
                else if (events[i].sample < done + n) ++count;
            std::vector<MidiEvent>::iterator it = events.begin() + first;
            for (auto e = it; e != it + count; ++e) e->sample -= done;
            engine.process (tmpL, tmpR, n, params, count > 0 ? &*it : nullptr, count, tr, raw);
            for (int i = 0; i < n; ++i) left[done + i] = 0.5f * (tmpL[i] + tmpR[i]);
            done += n;
        }
    }
    else
    {
        engine.process (left, right, numSamples, params, events.data(), (int) events.size(), tr, raw);
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);

    {
        int w = scopeWrite.load (std::memory_order_relaxed);
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            peak = std::max (peak, std::abs (x));
            scope[w].store (x, std::memory_order_relaxed);
            w = (w + 1) & (kScopeSize - 1);
        }
        scopeWrite.store (w, std::memory_order_release);
        outputPeak.store (std::max (peak, outputPeak.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    }

    activeVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
    for (int l = 0; l < kNumLfo; ++l) lfoDisplay[l].store (engine.lfoValue (l), std::memory_order_relaxed);
    midi.clear();
}

juce::AudioProcessorEditor* ZaryaProcessor::createEditor() { return new ZaryaEditor (*this); }

void ZaryaProcessor::setSampleInternal (int osc, std::shared_ptr<const SampleData> s)
{
    osc = juce::jlimit (0, kNumOsc - 1, osc);
    std::shared_ptr<const SampleData> old;
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        old = samples[osc];
        samples[osc] = std::move (s);
    }
    if (old != nullptr && old != defaultSample) graveyard.push_back (std::move (old));
}

void ZaryaProcessor::timerCallback()
{
    // A replaced sample is freed here once the audio thread no longer references it.
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const std::shared_ptr<const SampleData>& p) { return p.use_count() <= 1; }),
                     graveyard.end());
}

std::shared_ptr<const SampleData> ZaryaProcessor::getSample (int osc) const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return samples[juce::jlimit (0, kNumOsc - 1, osc)];
}

void ZaryaProcessor::resetSampleToDefault (int osc) { setSampleInternal (osc, defaultSample); }

juce::String ZaryaProcessor::loadSample (int osc, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return "Unsupported or unreadable audio file";
    if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0) return "The file contains no audio";

    const auto maxLen = (juce::int64) (reader->sampleRate * kMaxSampleSeconds);
    const int len = (int) std::min (reader->lengthInSamples, maxLen);
    const int channels = (int) juce::jlimit (1u, 2u, reader->numChannels);
    juce::AudioBuffer<float> buf (channels, len);
    reader->read (&buf, 0, len, 0, true, channels > 1);

    auto s = std::make_shared<SampleData>();
    s->sampleRate = reader->sampleRate;
    s->name = file.getFileName().toStdString();
    s->path = file.getFullPathName().toStdString();
    s->data.resize ((size_t) len);
    float peak = 0.0f;
    for (int i = 0; i < len; ++i)
    {
        float x = buf.getSample (0, i);
        if (channels > 1) x = 0.5f * (x + buf.getSample (1, i));
        s->data[(size_t) i] = x;
        peak = std::max (peak, std::abs (x));
    }
    if (peak > 1.0e-6f)
        for (auto& x : s->data) x *= 0.9f / peak;
    s->computeOverview();
    setSampleInternal (osc, std::move (s));
    return {};
}

void ZaryaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName (kSamplesNode), nullptr);
    juce::ValueTree node (kSamplesNode);
    for (int o = 0; o < kNumOsc; ++o)
    {
        const auto s = getSample (o);
        node.setProperty (samplePathKey (o), s != nullptr ? juce::String (s->path) : juce::String(), nullptr);
    }
    state.appendChild (node, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ZaryaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    const auto node = state.getChildWithName (kSamplesNode);
    for (int o = 0; o < kNumOsc; ++o)
    {
        const juce::String path = node.isValid() ? node[samplePathKey (o)].toString() : juce::String();
        const juce::File f (path);
        if (path.isNotEmpty() && f.existsAsFile()) loadSample (o, f);
        else resetSampleToDefault (o);
    }
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ZaryaProcessor(); }
