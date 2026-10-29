#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<const char*, PF2FuzzProcessor::numLearnable> PF2FuzzProcessor::learnableIDs {
    ParamIDs::fuzz, ParamIDs::volume, ParamIDs::bypass, ParamIDs::input, ParamIDs::output
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PF2FuzzProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int) { return String (v * 10.0f, 1); }));
    };
    auto trim = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("dB"));
    };

    layout.add (knob (ParamIDs::fuzz,   "Fuzz",   0.7f));
    layout.add (knob (ParamIDs::volume, "Volume", 0.5f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    layout.add (trim (ParamIDs::input,  "Input Trim"));
    // pick-up della chitarra collegata (interagisce con l'ingresso a bassa impedenza del FY-2)
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::pickup, 1 }, "Pickup",
        StringArray { "Ideale", "Single coil", "P-90", "Humbucker", "Personalizzato" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupR, 1 }, "Pickup R",
        NormalisableRange<float> (2.0f, 20.0f, 0.1f), 8.0f, AudioParameterFloatAttributes().withLabel ("kOhm")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::pickupL, 1 }, "Pickup L",
        NormalisableRange<float> (0.5f, 10.0f, 0.05f), 4.0f, AudioParameterFloatAttributes().withLabel ("H")));
    layout.add (trim (ParamIDs::output, "Output Trim"));
    return layout;
}

PF2FuzzProcessor::PF2FuzzProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    fuzzRaw   = apvts.getRawParameterValue (ParamIDs::fuzz);
    volRaw    = apvts.getRawParameterValue (ParamIDs::volume);
    bypassRaw = apvts.getRawParameterValue (ParamIDs::bypass);
    inRaw     = apvts.getRawParameterValue (ParamIDs::input);
    outRaw    = apvts.getRawParameterValue (ParamIDs::output);
    pickupRaw = apvts.getRawParameterValue (ParamIDs::pickup);
    pickRRaw  = apvts.getRawParameterValue (ParamIDs::pickupR);
    pickLRaw  = apvts.getRawParameterValue (ParamIDs::pickupL);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ParamIDs::bypass));

    for (int i = 0; i < numLearnable; ++i)
        learnParams[(size_t) i] = apvts.getParameter (learnableIDs[(size_t) i]);

    resetDefaultMappings();
}

//==============================================================================
void PF2FuzzProcessor::resetDefaultMappings()
{
    for (auto& m : ccToParam) m.store (-1);
    // CC "liberi" 20-23 per manopole/switch; CC 64 (pedale sustain) per il footswitch
    ccToParam[20].store (0);  // FUZZ
    ccToParam[21].store (1);  // VOLUME
    ccToParam[64].store (2);  // BYPASS
    ++mapVersion;
}

int PF2FuzzProcessor::getLearnIndex (const juce::String& paramID) const
{
    for (int i = 0; i < numLearnable; ++i)
        if (paramID == learnableIDs[(size_t) i]) return i;
    return -1;
}

int PF2FuzzProcessor::getCCForParam (int paramIndex) const
{
    for (int cc = 0; cc < 128; ++cc)
        if (ccToParam[(size_t) cc].load() == paramIndex) return cc;
    return -1;
}

void PF2FuzzProcessor::clearMappingForParam (int paramIndex)
{
    for (auto& m : ccToParam)
        if (m.load() == paramIndex) m.store (-1);
    ++mapVersion;
}

void PF2FuzzProcessor::handleCC (int cc, int value)
{
    lastCC.store (cc); lastCCValue.store (value); ++ccCounter;

    int learn = learningParam.load();
    if (learn >= 0 && learningParam.compare_exchange_strong (learn, -1))
    {
        for (auto& m : ccToParam)
            if (m.load() == learn) m.store (-1);
        ccToParam[(size_t) cc].store (learn);
        ccHigh[(size_t) cc] = value >= 64;
        ++mapVersion;
        return; // il messaggio usato per l'apprendimento non modifica il parametro
    }

    const int idx = ccToParam[(size_t) cc].load();
    if (idx < 0) return;
    auto* p = learnParams[(size_t) idx];

    auto setNorm = [p] (float v)
    {
        if (std::abs (p->getValue() - v) < 1.0e-6f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    };

    if (idx == 2) // BYPASS: commuta alla pressione (pedale o pulsante "gate")
    {
        const bool high = value >= 64;
        if (high && ! ccHigh[(size_t) cc]) setNorm (p->getValue() > 0.5f ? 0.0f : 1.0f);
        ccHigh[(size_t) cc] = high;
    }
    else
    {
        setNorm ((float) value / 127.0f);
    }
}

pf2::Pickup PF2FuzzProcessor::currentPickup() const
{
    switch ((int) pickupRaw->load())
    {
        case 0:  return pf2::Pickup::ideal();
        case 1:  return pf2::Pickup::singleCoil();
        case 2:  return pf2::Pickup::p90();
        case 3:  return pf2::Pickup::humbucker();
        default: return { pickRRaw->load() * 1000.0, (double) pickLRaw->load(), 150e-12, 250e3 };   // C e pot tipici
    }
}

//==============================================================================
bool PF2FuzzProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

void PF2FuzzProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numCh = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    // il pedale è mono: si elabora un solo canale e lo si copia sugli altri
    maxBlock = juce::jmax (1, samplesPerBlock);
    osChannels = (int) numCh;

    // il circuito gira sempre a circa 350-390 kHz: 8x a 44.1/48 kHz, 4x a 88.2/96 kHz,
    // 2x a 176.4/192 kHz (aliasing delle armoniche del fuzz sotto i -55 dB)
    const int oversamplingOrder = sampleRate > 300000.0 ? 0 : (sampleRate > 120000.0 ? 1 : (sampleRate > 60000.0 ? 2 : 3));
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, (size_t) oversamplingOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const double osRate = sampleRate * (double) oversampling->getOversamplingFactor();
    circuit.setPickup (currentPickup());
    circuit.prepare (osRate);
    circuit.setControls (fuzzRaw->load(), volRaw->load());

    const auto latency = oversampling->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetMixProportion (bypassRaw->load() > 0.5f ? 0.0f : 1.0f);
    mixer.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    mixer.setWetLatency (latency);

    // bypass: dopo la dissolvenza del mixer (50 ms) il circuito non viene più calcolato
    bypassFadeSamples = juce::roundToInt (0.08 * sampleRate);
    lastBypassState = bypassRaw->load() > 0.5f;
    bypassFadeLeft = 0;
    circuitIdle = lastBypassState;

    auto initSm = [sampleRate] (juce::SmoothedValue<float>& s, float v, double sec)
    { s.reset (sampleRate, sec); s.setCurrentAndTargetValue (v); };
    initSm (fuzzSm,  fuzzRaw->load(),  0.03);
    initSm (volSm,   volRaw->load(),   0.03);
    initSm (inGainSm,  juce::Decibels::decibelsToGain (inRaw->load()),  0.03);
    initSm (outGainSm, juce::Decibels::decibelsToGain (outRaw->load()), 0.03);
}

void PF2FuzzProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isController()) handleCC (m.getControllerNumber(), m.getControllerValue());
    }
    midi.clear();

    const int numCh = juce::jmin (getTotalNumInputChannels(), osChannels);
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh < 1) return;

    circuit.setPickup (currentPickup());
    fuzzSm.setTargetValue (fuzzRaw->load());
    volSm.setTargetValue (volRaw->load());
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (inRaw->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (outRaw->load()));
    const bool bypassed = bypassRaw->load() > 0.5f;
    mixer.setWetMixProportion (bypassed ? 0.0f : 1.0f);

    // true bypass: a dissolvenza finita il circuito resta fermo (come il pedale, il cui
    // ingresso è scollegato) e non consuma CPU; al reinserimento riparte dal punto di riposo
    if (bypassed && ! lastBypassState) bypassFadeLeft = bypassFadeSamples;
    if (! bypassed && circuitIdle)
    {
        circuit.reset();
        oversampling->reset();
        circuitIdle = false;
    }
    lastBypassState = bypassed;
    if (bypassed && ! circuitIdle)
    {
        bypassFadeLeft -= buffer.getNumSamples();
        if (bypassFadeLeft < -buffer.getNumSamples()) circuitIdle = true;   // ancora un blocco di margine
    }

    juce::dsp::AudioBlock<float> full (buffer);
    full = full.getSubsetChannelBlock (0, (size_t) numCh);
    const int maxChunk = juce::jmax (1, maxBlock);
    const int factor = (int) oversampling->getOversamplingFactor();

    // l'host può inviare blocchi più grandi di quanto dichiarato: si lavora a pezzi
    for (int offset = 0; offset < buffer.getNumSamples(); offset += maxChunk)
    {
        const int n = juce::jmin (maxChunk, buffer.getNumSamples() - offset);
        auto block = full.getSubBlock ((size_t) offset, (size_t) n);
        auto mono  = block.getSingleChannelBlock (0);   // ingresso: canale sinistro / mono

        mixer.pushDrySamples (block);
        if (circuitIdle) { mixer.mixWetSamples (block); continue; }   // solo il ramo diretto

        auto* x = mono.getChannelPointer (0);

        // la chitarra può arrivare su uno qualsiasi dei due ingressi (es. ingresso 2 Hi-Z):
        // se i canali sono copie identiche si usa il primo, altrimenti si sommano
        if (numCh > 1)
        {
            const auto* r = block.getChannelPointer (1);
            bool identical = true;
            for (int i = 0; i < n && identical; ++i) identical = std::abs (x[i] - r[i]) < 1.0e-7f;
            if (! identical) juce::FloatVectorOperations::add (x, r, n);
        }

        for (int i = 0; i < n; ++i) x[i] *= inGainSm.getNextValue() * voltsPerFS;

        auto up = oversampling->processSamplesUp (mono);
        auto* d = up.getChannelPointer (0);

        for (int start = 0; start < n; start += controlBlock)
        {
            const int len = juce::jmin (controlBlock, n - start);
            circuit.setControls (fuzzSm.skip (len), volSm.skip (len));
            for (int i = start * factor, end = (start + len) * factor; i < end; ++i)
                d[i] = (float) circuit.process ((double) d[i]);
        }

        oversampling->processSamplesDown (mono);

        for (int i = 0; i < n; ++i) x[i] *= outGainSm.getNextValue() * fsPerVolt;
        for (int ch = 1; ch < numCh; ++ch)
            juce::FloatVectorOperations::copy (block.getChannelPointer ((size_t) ch), x, n);

        mixer.mixWetSamples (block);
    }
}

//==============================================================================
void PF2FuzzProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement root ("PF2State");
    if (auto params = apvts.copyState().createXml()) root.addChildElement (params.release());

    auto* map = root.createNewChildElement ("MIDIMAP");
    for (int cc = 0; cc < 128; ++cc)
        if (const int idx = ccToParam[(size_t) cc].load(); idx >= 0)
        {
            auto* e = map->createNewChildElement ("CC");
            e->setAttribute ("num", cc);
            e->setAttribute ("param", learnableIDs[(size_t) idx]);
        }
    copyXmlToBinary (root, destData);
}

void PF2FuzzProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName ("PF2State")) return;

    if (auto* params = xml->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*params));

    if (auto* map = xml->getChildByName ("MIDIMAP"))
    {
        for (auto& m : ccToParam) m.store (-1);
        for (auto* e : map->getChildWithTagNameIterator ("CC"))
        {
            const int cc = e->getIntAttribute ("num", -1);
            const int idx = getLearnIndex (e->getStringAttribute ("param"));
            if (juce::isPositiveAndBelow (cc, 128) && idx >= 0) ccToParam[(size_t) cc].store (idx);
        }
        ++mapVersion;
    }
}

juce::AudioProcessorEditor* PF2FuzzProcessor::createEditor() { return new PF2FuzzEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PF2FuzzProcessor(); }
