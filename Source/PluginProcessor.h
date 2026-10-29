#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include "FuzzCircuit.h"

namespace ParamIDs
{
    inline constexpr const char* fuzz   = "fuzz";
    inline constexpr const char* volume = "volume";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* input  = "input";
    inline constexpr const char* output = "output";
    inline constexpr const char* pickup = "pickup";
    inline constexpr const char* pickupR = "pickupR";
    inline constexpr const char* pickupL = "pickupL";
}

//==============================================================================
class PF2FuzzProcessor : public juce::AudioProcessor
{
public:
    PF2FuzzProcessor();
    ~PF2FuzzProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // MIDI CC: mappa e MIDI Learn (thread-safe, senza lock)
    static constexpr int numLearnable = 5;
    static const std::array<const char*, numLearnable> learnableIDs;

    int  getLearnIndex (const juce::String& paramID) const;
    void startLearn (int paramIndex)  { learningParam.store (paramIndex); }
    void cancelLearn()                { learningParam.store (-1); }
    int  getLearningParam() const     { return learningParam.load(); }
    int  getCCForParam (int paramIndex) const;
    void clearMappingForParam (int paramIndex);
    void resetDefaultMappings();
    int  getMapVersion() const        { return mapVersion.load(); }

    // ultima attività MIDI (per il display)
    std::atomic<int> lastCC { -1 }, lastCCValue { 0 }, ccCounter { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleCC (int cc, int value);

    pf2::FuzzCircuit circuit;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DryWetMixer<float> mixer { 256 };

    juce::SmoothedValue<float> fuzzSm, volSm, inGainSm, outGainSm;

    std::atomic<float>* fuzzRaw   = nullptr;
    std::atomic<float>* volRaw    = nullptr;
    std::atomic<float>* bypassRaw = nullptr;
    std::atomic<float>* inRaw     = nullptr;
    std::atomic<float>* outRaw    = nullptr;
    std::atomic<float>* pickupRaw = nullptr;
    std::atomic<float>* pickRRaw  = nullptr;
    std::atomic<float>* pickLRaw  = nullptr;
    pf2::Pickup currentPickup() const;
    juce::AudioParameterBool* bypassParam = nullptr;

    std::array<juce::RangedAudioParameter*, numLearnable> learnParams {};
    std::array<std::atomic<int>, 128> ccToParam;
    std::array<bool, 128> ccHigh {};
    std::atomic<int> learningParam { -1 }, mapVersion { 0 };

    int maxBlock = 512, osChannels = 2;
    int bypassFadeSamples = 4000, bypassFadeLeft = 0;
    bool lastBypassState = false, circuitIdle = false;

    static constexpr int controlBlock = 32;       // campioni (frequenza base)
    static constexpr float voltsPerFS = 1.0f;     // 0 dBFS = 1 V di picco
    // il FY-2 esce molto basso (≈ 30 mV rms): OUTPUT 0 dB = livello reale +18 dB puliti
    static constexpr float fsPerVolt  = 8.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PF2FuzzProcessor)
};
