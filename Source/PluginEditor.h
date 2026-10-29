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
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.34f), juce::Font::bold));
    }
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

// Deviatore a slitta BATTERIA / ALIMENTATORE del pannello interno
class SupplySwitch : public juce::Button
{
public:
    SupplySwitch() : juce::Button ("Supply") { setClickingTogglesState (true); }
    std::function<void()> onLearnMenu;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Button::mouseUp (e); }
};

//==============================================================================
// Pannello "interno" (come aprire il fondo del pedale): alimentazione e transistor
class InteriorPanel : public juce::Component, private juce::Timer
{
public:
    InteriorPanel (PF2FuzzProcessor&, juce::Typeface::Ptr panelFace,
                   std::function<void (int, juce::Component*)> learnMenu);
    ~InteriorPanel() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Component* controlFor (int paramIndex);
    std::function<void()> onClose;

    static constexpr float baseW = 380.0f, baseH = 656.0f;

private:
    void timerCallback() override;
    juce::Font font (float pt, float tracking = 0.0f) const;
    juce::Rectangle<int> area (float x, float y, float w, float h) const;
    void resetToOriginal();

    PF2FuzzProcessor& proc;
    juce::Typeface::Ptr face;
    SupplySwitch supply;
    LearnableSlider charge, hfe1, hfe2;
    juce::TextButton resetBtn { "VALORI ORIGINALI" }, closeBtn { "CHIUDI" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> supplyA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chargeA, hfe1A, hfe2A;
    float k = 1.0f, lastV = -1.0f, lastH1 = -1.0f, lastH2 = -1.0f;
    bool lastDC = false;
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

    // coordinate di progetto (proposta grafica A): barra di servizio 0-96, pedale sotto
    static constexpr int baseW = 420, baseH = 780;
    static constexpr float pedalY = 96.0f;

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
    juce::TextButton interiorBtn { "INTERNO" };
    std::unique_ptr<InteriorPanel> interior;
    PickupLibrary library;
    std::unique_ptr<juce::AlertWindow> nameDialog;
    int lastLibVersion = -1;
    std::unique_ptr<SA> pickRA, pickLA;
    int lastPickup = -1;

    juce::Typeface::Ptr scriptFace, panelFace;
    juce::Font scriptFont (float pt) const;
    juce::Font panelFont (float pt, float tracking = 0.0f) const;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Image background;
    float scale = 1.0f;
    int lastLearn = -2, lastMapVersion = -1, lastCCCount = -1;
    bool lastBypass = false;
    float pulse = 0.0f;
};
