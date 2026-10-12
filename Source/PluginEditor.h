#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PickupLibrary.h"

//==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// Slider con menu MIDI Learn sul clic destro
class LearnableSlider : public juce::Slider
{
public:
    std::function<void()> onLearnMenu;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp   (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

// Interruttore a pedale (true bypass)
class Footswitch : public juce::Button
{
public:
    Footswitch() : juce::Button ("Footswitch") { setClickingTogglesState (true); }
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
};

//==============================================================================
class PF2FuzzEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PF2FuzzEditor (PF2FuzzProcessor&);
    ~PF2FuzzEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 380, baseH = 680;
    static constexpr float dy = 40.0f;   // spostamento del pedale sotto la barra superiore

private:
    void timerCallback() override;
    void showLearnMenu (int paramIndex, juce::Component* target);
    void renderBackground();
    juce::Component* controlFor (int paramIndex);
    juce::Rectangle<float> base (float x, float y, float w, float h) const;

    // pick-up: preset fissi + chitarre salvate dall'utente
    void rebuildPickupBox();
    void syncPickupBox();
    void pickupChosen();
    void showPickupMenu();
    void askNameAndSave();
    void setParam (const char* id, float realValue);
    float currentR() const;
    float currentL() const;

    PF2FuzzProcessor& proc;
    PedalLookAndFeel lnf;

    LearnableSlider fuzz, volume, inTrim, outTrim;
    Footswitch footswitch;
    juce::ComboBox pickupBox;
    juce::Slider pickR, pickL;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> fuzzA, volA, inA, outA;
    std::unique_ptr<BA> bypassA;
    juce::TextButton pickupMenuBtn { juce::String::fromUTF8 ("\xe2\x80\xa6") };
    PickupLibrary library;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    int lastLibVersion = -1;
    std::unique_ptr<SA> pickRA, pickLA;
    int lastPickup = -1;

    juce::TooltipWindow tooltips { this, 700 };
    juce::Image background;
    float scale = 1.0f;
    int lastLearn = -2, lastMapVersion = -1, lastCCCount = -1;
    bool lastBypass = false;
    float pulse = 0.0f;
};
