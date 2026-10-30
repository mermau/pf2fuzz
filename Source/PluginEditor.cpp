#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstdint>

namespace
{
    // palette della proposta A: nero martellato, acciaio spazzolato, targhetta rossa
    const juce::Colour ink       (0xff18191b);   // serigrafia sull'acciaio
    const juce::Colour cream     (0xffe4e5e7);   // testi chiari della barra di servizio
    const juce::Colour stripCol  (0xff0e0e10);
    const juce::Colour lineCol   (0xff3a3c40);
    const juce::Colour learnCol  (0xffffb020);

    juce::Font uiFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    const char* displayNames[] = { "FUZZ", "VOLUME", "FOOTSWITCH", "INPUT", "OUTPUT",
                                   "ALIMENTAZIONE", "CARICA", "hFE Q1", "hFE Q2" };

    // classe di guadagno del 2SC536 (datasheet NEC) per un valore di hFE
    const char* hfeClass (float h)
    {
        return h < 100.0f ? "D" : (h < 160.0f ? "E" : (h <= 320.0f ? "F" : (h <= 560.0f ? "G" : "H")));
    }

    // testo con linea di base, come nella grafica di riferimento
    void drawBaseline (juce::Graphics& g, const juce::Font& f, const juce::String& s,
                       float x, float baseline, juce::Justification j)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, s);
        if (j.testFlags (juce::Justification::horizontallyCentred)) x -= w * 0.5f;
        else if (j.testFlags (juce::Justification::right)) x -= w;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x, baseline);
        ga.draw (g);
    }

    // gradiente radiale "cromo" alla maniera degli SVG della proposta
    juce::ColourGradient chrome (juce::Point<float> c, float r, float darken = 0.0f)
    {
        const float d = 2.0f * r;
        juce::ColourGradient cg (juce::Colours::white.darker (darken), c.x - 0.15f * d, c.y - 0.2f * d,
                                 juce::Colour (0xff55595e).darker (darken), c.x - 0.15f * d + 0.85f * d, c.y - 0.2f * d, true);
        cg.addColour (0.35, juce::Colour (0xffd6d8db).darker (darken));
        cg.addColour (0.75, juce::Colour (0xff868a90).darker (darken));
        return cg;
    }

    // rumore a valori (per la vernice martellata e l'acciaio spazzolato)
    float hash2 (int x, int y)
    {
        uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffffu) / 16777215.0f;
    }

    float vnoise (float x, float y)
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        float fx = x - (float) xi, fy = y - (float) yi;
        fx = fx * fx * (3.0f - 2.0f * fx); fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash2 (xi, yi), b = hash2 (xi + 1, yi), c = hash2 (xi, yi + 1), d = hash2 (xi + 1, yi + 1);
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
    }

    float crinkleHeight (float u, float v)
    {
        return 0.55f * vnoise (u * 0.55f, v * 0.55f) + 0.3f * vnoise (u * 1.3f + 17.0f, v * 1.3f)
             + 0.15f * vnoise (u * 3.1f, v * 3.1f + 31.0f);
    }

    // vernice nera martellata: rilievo illuminato da sinistra in alto
    juce::Image makeCrinkle (int w, int h, float scale)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float Lx = -0.491f, Ly = -0.491f, Lz = 0.719f, e = 0.35f, depth = 2.6f;
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const float u = (float) x / scale, v = (float) y / scale;
                const float h0 = crinkleHeight (u, v);
                const float dx = (crinkleHeight (u + e, v) - h0) / e * depth;
                const float dy = (crinkleHeight (u, v + e) - h0) / e * depth;
                const float n = 1.0f / std::sqrt (dx * dx + dy * dy + 1.0f);
                const float lam = juce::jmax (0.0f, (-dx * Lx - dy * Ly + Lz) * n);
                const auto c = (juce::uint8) juce::jlimit (0.0f, 255.0f, 12.0f + 50.0f * lam * lam);
                bd.setPixelColour (x, y, juce::Colour (c, c, (juce::uint8) juce::jmin (255, c + 1)));
            }
        return img;
    }

    // acciaio spazzolato: gradiente diagonale e righe orizzontali
    juce::Image makeSteel (int w, int h, float scale, float bw, float bh)
    {
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float st[4][2] = { { 0.0f, 236.0f }, { 0.38f, 187.0f }, { 0.6f, 222.0f }, { 1.0f, 160.0f } };
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const float u = (float) x / scale, v = (float) y / scale;
                const float t = juce::jlimit (0.0f, 1.0f, 0.5f * (u / bw + v / bh));
                float base = st[3][1];
                for (int i = 0; i < 3; ++i)
                    if (t <= st[i + 1][0])
                    {
                        const float k = (t - st[i][0]) / (st[i + 1][0] - st[i][0]);
                        base = st[i][1] + k * (st[i + 1][1] - st[i][1]);
                        break;
                    }
                const float n = 0.6f * vnoise (u * 0.015f, v * 0.9f) + 0.4f * vnoise (u * 0.006f + 50.0f, v * 2.4f);
                const float val = base * (1.0f - 0.20f * n);
                bd.setPixelColour (x, y, juce::Colour ((juce::uint8) val, (juce::uint8) juce::jmin (255.0f, val + 2.0f),
                                                       (juce::uint8) juce::jmin (255.0f, val + 5.0f)));
            }
        return img;
    }
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    using namespace juce;
    setColour (PopupMenu::backgroundColourId, Colour (0xff1a1b1d));
    setColour (PopupMenu::textColourId, cream);
    setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff8c1218));
    setColour (BubbleComponent::backgroundColourId, Colour (0xee111111));
    setColour (BubbleComponent::outlineColourId, cream.withAlpha (0.4f));
    setColour (TooltipWindow::textColourId, cream);
    setColour (ComboBox::backgroundColourId, Colour (0xff1a1b1d));
    setColour (ComboBox::outlineColourId, lineCol);
    setColour (ComboBox::textColourId, cream);
    setColour (ComboBox::arrowColourId, cream.withAlpha (0.7f));
    setColour (TextButton::buttonColourId, Colour (0xff1a1b1d));
    setColour (TextButton::textColourOffId, cream);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto style = slider.getProperties()["style"].toString();

    if (style == "trimmer")   // trimmer multigiro blu da circuito stampato
    {
        const float side = jmin (b.getWidth(), b.getHeight()) * 0.62f, r = side * 0.36f;
        const auto sq = Rectangle<float> (side, side).withCentre (c);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (sq.translated (0, 2.0f), side * 0.08f);
        g.setGradientFill (ColourGradient (Colour (0xff3d6fd1), sq.getX(), sq.getY(), Colour (0xff1c3c86), sq.getRight(), sq.getBottom(), false));
        g.fillRoundedRectangle (sq, side * 0.08f);
        g.setColour (Colour (0xffd9dde4));
        g.fillEllipse (Rectangle<float> (2 * r, 2 * r).withCentre (c));
        Path slot; slot.addRoundedRectangle (-r * 0.18f, -r * 0.9f, r * 0.36f, r * 1.8f, r * 0.1f);
        g.setColour (Colour (0xff3a3d42));
        g.fillPath (slot, AffineTransform::rotation (angle).translated (c));
        // scala attorno al trimmer
        g.setColour (Colour (0xffc9ccd0).withAlpha (0.6f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (c.getPointOnCircumference (side * 0.74f, a)));
        }
        return;
    }

    if (style == "charge")    // carica della batteria: scala verde / ambra / rossa
    {
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const float vNorm = (float) ((Battery::emptyV - Battery::minV) / (Battery::fullV - Battery::minV));
        auto arc = [&] (float from, float to, Colour col)
        {
            Path a; a.addCentredArc (c.x, c.y, R * 0.86f, R * 0.86f, 0.0f,
                                     startAngle + from * (endAngle - startAngle), startAngle + to * (endAngle - startAngle), true);
            g.setColour (col);
            g.strokePath (a, PathStrokeType (R * 0.1f, PathStrokeType::curved, PathStrokeType::butt));
        };
        const bool on = slider.isEnabled();
        arc (0.0f, vNorm, Colour (0xffc8302b).withAlpha (on ? 1.0f : 0.35f));
        arc (vNorm, vNorm + 0.18f, Colour (0xffd9a32c).withAlpha (on ? 1.0f : 0.35f));
        arc (vNorm + 0.18f, 1.0f, Colour (0xff6fae55).withAlpha (on ? 1.0f : 0.35f));
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillEllipse (Rectangle<float> (R * 1.36f, R * 1.36f).withCentre (c.translated (0, R * 0.06f)));
        ColourGradient body (Colour (0xff3b3b3e), c.x - R * 0.2f, c.y - R * 0.3f, Colour (0xff050505), c.x + R * 0.4f, c.y + R * 0.5f, true);
        g.setGradientFill (body);
        g.fillEllipse (Rectangle<float> (R * 1.3f, R * 1.3f).withCentre (c));
        Path p; p.addRoundedRectangle (-R * 0.05f, -R * 0.62f, R * 0.1f, R * 0.36f, R * 0.05f);
        g.setColour (Colour (0xfff1f2f3).withAlpha (on ? 1.0f : 0.4f));
        g.fillPath (p, AffineTransform::rotation (angle).translated (c));
        return;
    }

    if (w < 80)   // manopole di servizio (INPUT / OUTPUT)
    {
        const float k = jmin (b.getWidth(), b.getHeight()) / 34.0f;
        g.setColour (Colour (0xff1c1c1e)); g.fillEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c));
        g.setColour (Colour (0xff4a4d52)); g.drawEllipse (Rectangle<float> (30 * k, 30 * k).withCentre (c), 1.0f);
        g.setColour (Colour (0xff2b2c2f)); g.fillEllipse (Rectangle<float> (18 * k, 18 * k).withCentre (c));
        Path p; p.addRoundedRectangle (-1.2f * k, -14.0f * k, 2.4f * k, 8.0f * k, 1.2f * k);
        g.setColour (Colour (0xffd9dbde));
        g.fillPath (p, AffineTransform::rotation (angle).translated (c));
        return;
    }

    const float k = jmin (b.getWidth(), b.getHeight()) / 124.0f;   // unità: SVG della proposta

    // scala a punti serigrafata
    g.setColour (Colour (0xff1b1c1e));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        g.fillEllipse (Rectangle<float> (5.2f * k, 5.2f * k).withCentre (c.getPointOnCircumference (56.0f * k, a)));
    }

    // ombra e corpo nero
    g.setColour (Colours::black.withAlpha (0.3f));
    g.fillEllipse (Rectangle<float> (80 * k, 80 * k).withCentre (c.translated (0, 4 * k)));
    ColourGradient body (Colour (0xff3b3b3e), c.x - 7.6f * k, c.y - 15.2f * k,
                         Colour (0xff050505), c.x - 7.6f * k + 60.8f * k, c.y - 15.2f * k, true);
    body.addColour (0.7, Colour (0xff141416));
    g.setGradientFill (body);
    g.fillEllipse (Rectangle<float> (76 * k, 76 * k).withCentre (c));

    // zigrinatura (gira con la manopola)
    g.setColour (Colour (0xff2f3033));
    for (int i = 0; i < 56; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 56.0f;
        g.drawLine (Line<float> (c.getPointOnCircumference (34.4f * k, a), c.getPointOnCircumference (37.6f * k, a)), 2.0f * k);
    }

    // cappuccio cromato
    g.setGradientFill (chrome (c, 22.0f * k));
    g.fillEllipse (Rectangle<float> (44 * k, 44 * k).withCentre (c));

    // indice bianco sulla gonna
    Path p; p.addRoundedRectangle (-2.0f * k, -37.0f * k, 4.0f * k, 14.0f * k, 2.0f * k);
    g.setColour (Colour (0xfff1f2f3));
    g.fillPath (p, AffineTransform::rotation (angle).translated (c));
}

//==============================================================================
void LearnableSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Slider::mouseDown (e);
}

//==============================================================================
void Footswitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Button::mouseDown (e);
}

// interruttore a pedale rotondo come sull'originale: ghiera zigrinata e pulsante cromato
void Footswitch::paintButton (juce::Graphics& g, bool, bool down)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    const float k = jmin (b.getWidth(), b.getHeight()) / 84.0f;
    const auto c = b.getCentre();

    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (62 * k, 62 * k).withCentre (c.translated (0, 4 * k)));

    // rondella cromata
    g.setGradientFill (chrome (c, 29.0f * k, 0.15f));
    g.fillEllipse (Rectangle<float> (58 * k, 58 * k).withCentre (c));
    g.setColour (Colour (0xff3b3d41));
    g.drawEllipse (Rectangle<float> (58 * k, 58 * k).withCentre (c), 1.0f);

    // ghiera zigrinata
    g.setColour (Colour (0xff6e7278));
    g.fillEllipse (Rectangle<float> (46 * k, 46 * k).withCentre (c));
    g.setColour (Colour (0xff3f4246));
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 48.0f;
        g.drawLine (Line<float> (c.getPointOnCircumference (20.5f * k, a), c.getPointOnCircumference (23.0f * k, a)), 1.4f * k);
    }

    // pulsante: premuto, affonda nella ghiera (vista dall'alto appare più piccolo e
    // lascia scoperta la sede scura), senza ombre né cambi di luminosità
    const float capD = (down ? 30.0f : 34.0f) * k;
    if (down)
    {
        g.setColour (Colour (0xff1c1d20));
        g.fillEllipse (Rectangle<float> (35 * k, 35 * k).withCentre (c));
    }
    g.setGradientFill (chrome (c, capD * 0.5f));
    g.fillEllipse (Rectangle<float> (capD, capD).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (capD, capD).withCentre (c), 0.8f);
}

//==============================================================================
void SupplySwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { if (onLearnMenu) onLearnMenu(); return; }
    Button::mouseDown (e);
}

void SupplySwitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool dc = getToggleState();
    g.setColour (Colour (0xff0b0b0c));
    g.fillRoundedRectangle (b, b.getHeight() * 0.25f);
    g.setColour (Colour (0xff3a3c40));
    g.drawRoundedRectangle (b, b.getHeight() * 0.25f, 1.0f);
    const auto half = b.withWidth (b.getWidth() * 0.5f).reduced (3.0f);
    const auto knob = dc ? half.withX (b.getCentreX() + 0.0f) : half;
    g.setGradientFill (ColourGradient (Colour (0xffeceef0), knob.getX(), knob.getY(), Colour (0xff8a8e94), knob.getX(), knob.getBottom(), false));
    g.fillRoundedRectangle (knob, knob.getHeight() * 0.2f);
    if (highlighted) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (knob, knob.getHeight() * 0.2f); }
    g.setFont (Font (FontOptions (b.getHeight() * 0.34f, Font::bold)));
    g.setColour (dc ? Colour (0xff9a9da2) : Colour (0xff18191b));
    g.drawText ("BATT", half, Justification::centred);
    g.setColour (dc ? Colour (0xff18191b) : Colour (0xff9a9da2));
    g.drawText ("DC", half.withX (b.getCentreX()), Justification::centred);
}

//==============================================================================
InteriorPanel::InteriorPanel (PF2FuzzProcessor& p, juce::Typeface::Ptr panelFace,
                              std::function<void (int, juce::Component*)> learnMenu)
    : proc (p), face (panelFace)
{
    using namespace juce;
    auto setupKnob = [&] (LearnableSlider& s, const char* id, const char* style)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.getProperties().set ("style", style);
        s.setPopupDisplayEnabled (true, true, this);
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [learnMenu, idx, &s] { learnMenu (idx, &s); };
        s.setTooltip ("Clic destro: MIDI Learn. Doppio clic: valore originale");
        addAndMakeVisible (s);
    };
    setupKnob (charge, ParamIDs::battery, "charge");
    setupKnob (hfe1,   ParamIDs::hfeQ1,   "trimmer");
    setupKnob (hfe2,   ParamIDs::hfeQ2,   "trimmer");
    chargeA = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::battery, charge);
    hfe1A   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::hfeQ1, hfe1);
    hfe2A   = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::hfeQ2, hfe2);
    charge.setDoubleClickReturnValue (true, Battery::nominalV);
    hfe1.setDoubleClickReturnValue (true, 226.0);
    hfe2.setDoubleClickReturnValue (true, 226.0);

    const int supplyIdx = proc.getLearnIndex (ParamIDs::supply);
    supply.onLearnMenu = [learnMenu, supplyIdx, this] { learnMenu (supplyIdx, &supply); };
    supply.setTooltip ("Batteria (tensione regolabile) o alimentatore stabilizzato da 9 V. Clic destro: MIDI Learn");
    addAndMakeVisible (supply);
    supplyA = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::supply, supply);

    resetBtn.setTooltip ("Batteria a 9 V e transistor al centro della classe F (hFE 226)");
    resetBtn.onClick = [this] { resetToOriginal(); };
    closeBtn.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (resetBtn);
    addAndMakeVisible (closeBtn);
    timerCallback();   // valori correnti subito, senza aspettare il primo tick
    startTimerHz (15);
}

juce::Font InteriorPanel::font (float pt, float tracking) const
{
    return juce::Font (juce::FontOptions (face).withPointHeight (pt * k)).withExtraKerningFactor (tracking / pt);
}

juce::Rectangle<int> InteriorPanel::area (float x, float y, float w, float h) const
{
    return juce::Rectangle<float> (x * k, y * k, w * k, h * k).toNearestInt();
}

void InteriorPanel::resized()
{
    k = (float) getWidth() / baseW;
    supply.setBounds (area (214, 118, 132, 34));
    charge.setBounds (area (230, 186, 100, 100));
    hfe1.setBounds   (area (60, 404, 100, 100));
    hfe2.setBounds   (area (220, 404, 100, 100));
    resetBtn.setBounds (area (44, 594, 180, 32));
    closeBtn.setBounds (area (240, 594, 96, 32));
}

juce::Component* InteriorPanel::controlFor (int idx)
{
    switch (idx) { case 5: return &supply; case 6: return &charge; case 7: return &hfe1; case 8: return &hfe2; default: return nullptr; }
}

void InteriorPanel::resetToOriginal()
{
    auto set = [this] (const char* id, float v)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture();
    };
    set (ParamIDs::supply, 0.0f);
    set (ParamIDs::battery, (float) Battery::nominalV);
    set (ParamIDs::hfeQ1, 226.0f);
    set (ParamIDs::hfeQ2, 226.0f);
}

void InteriorPanel::timerCallback()
{
    const bool dc = proc.apvts.getRawParameterValue (ParamIDs::supply)->load() > 0.5f;
    const float v = proc.apvts.getRawParameterValue (ParamIDs::battery)->load();
    const float h1 = proc.apvts.getRawParameterValue (ParamIDs::hfeQ1)->load();
    const float h2 = proc.apvts.getRawParameterValue (ParamIDs::hfeQ2)->load();
    if (dc != lastDC || std::abs (v - lastV) > 1e-4f || std::abs (h1 - lastH1) > 1e-3f || std::abs (h2 - lastH2) > 1e-3f)
    {
        lastDC = dc; lastV = v; lastH1 = h1; lastH2 = h2;
        charge.setEnabled (! dc);
        repaint();
    }
}

void InteriorPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    g.addTransform (AffineTransform::scale (k));
    const float W = baseW, H = baseH;
    auto f = [this] (float pt, float tr = 0.0f) { return font (pt, tr).withPointHeight (pt); };   // già scalato dalla trasformazione

    // fondo del pedale aperto
    g.setColour (Colour (0xff141416));
    g.fillRoundedRectangle (0, 0, W, H, 14.0f);
    g.setColour (Colour (0xff3a3c40));
    g.drawRoundedRectangle (0.5f, 0.5f, W - 1.0f, H - 1.0f, 14.0f, 1.0f);

    g.setColour (cream);
    g.setFont (f (18.0f, 4.0f));
    g.drawText ("INTERNO", Rectangle<float> (0, 22, W, 24), Justification::centred);
    g.setColour (Colour (0xff8a8d92));
    g.setFont (uiFont (10.0f, false));
    g.drawText ("alimentazione e transistor", Rectangle<float> (0, 46, W, 14), Justification::centred);

    // --- alimentazione ---
    const bool dc = lastDC;
    const float v = dc ? (float) Battery::dcV : lastV;
    const double ch = Battery::chargeFromVolts (v);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (f (11.0f, 2.0f));
    g.drawText ("ALIMENTAZIONE", Rectangle<float> (30, 82, W - 60, 16), Justification::centredLeft);

    // batteria 9 V disegnata, con il livello di carica
    const Rectangle<float> bat (54, 122, 104, 168);
    g.setColour (Colour (0xff2a2b2e));
    g.fillRoundedRectangle (bat.getX() + 18, bat.getY() - 14, 22, 16, 3.0f);
    g.fillRoundedRectangle (bat.getRight() - 40, bat.getY() - 14, 22, 16, 3.0f);
    g.setColour (Colour (0xff9a9da2));
    g.drawEllipse (bat.getX() + 22, bat.getY() - 11, 14, 10, 1.6f);
    g.drawRect (bat.getRight() - 36.0f, bat.getY() - 11.0f, 14.0f, 10.0f, 1.6f);
    g.setColour (Colour (0xff202124));
    g.fillRoundedRectangle (bat, 8.0f);
    g.setColour (Colour (0xff4a4d52));
    g.drawRoundedRectangle (bat, 8.0f, 1.5f);
    if (dc)
    {
        g.setColour (Colour (0xff8a8d92));
        g.setFont (f (14.0f, 1.0f));
        g.drawText ("DC", bat, Justification::centred);
    }
    else
    {
        const float lvl = (float) jlimit (0.0, 1.0, ch);
        const Colour col = ch <= 0.0 ? Colour (0xffc8302b) : (ch < 0.25 ? Colour (0xffd9a32c) : Colour (0xff6fae55));
        const auto inner = bat.reduced (8.0f);
        const auto fill = inner.withTrimmedTop (inner.getHeight() * (1.0f - lvl));
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (fill.withHeight (jmax (fill.getHeight(), ch <= 0.0 ? 4.0f : 0.0f)).withBottomY (inner.getBottom()), 4.0f);
        g.setColour (Colours::white.withAlpha (0.85f));
        g.setFont (f (16.0f, 1.0f));
        g.drawText ("9V", bat.withTrimmedTop (16).withHeight (24), Justification::centred);
    }
    g.setColour (cream);
    g.setFont (uiFont (13.0f));
    g.drawText (String (v, 2) + " V", Rectangle<float> (34, 298, 144, 18), Justification::centred);
    g.setColour (Colour (0xff9a9da2));
    g.setFont (uiFont (10.0f, false));
    const String state = dc ? String ("alimentatore stabilizzato")
                       : (ch < 0.0 ? String ("oltre la scarica")
                       : String (roundToInt (ch * 100.0)) + "% di carica");
    g.drawText (state, Rectangle<float> (34, 316, 144, 14), Justification::centred);

    g.setColour (Colour (0xff9a9da2));
    g.setFont (uiFont (9.0f));
    g.drawText ("BATTERIA", Rectangle<float> (214, 154, 66, 12), Justification::centred);
    g.drawText ("ALIMENTATORE", Rectangle<float> (280, 154, 66, 12), Justification::centred);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (f (10.0f, 1.5f));
    g.drawText ("CARICA", Rectangle<float> (230, 290, 100, 14), Justification::centred);
    g.setColour (Colour (0xff6c6f74));
    g.setFont (uiFont (8.0f, false));
    g.drawText ("4.5 V", Rectangle<float> (214, 270, 40, 10), Justification::centredLeft);
    g.drawText ("9.6 V", Rectangle<float> (306, 270, 40, 10), Justification::centredRight);

    // --- transistor su circuito stampato ---
    const Rectangle<float> pcb (24, 350, W - 48, 222);
    g.setColour (Colour (0xff1d3a28));
    g.fillRoundedRectangle (pcb, 6.0f);
    g.setColour (Colour (0xffb07a3a).withAlpha (0.35f));
    for (float yy : { 380.0f, 520.0f }) g.drawLine (pcb.getX() + 14, yy, pcb.getRight() - 14, yy, 2.0f);
    g.drawLine (190.0f, 380.0f, 190.0f, 520.0f, 2.0f);
    g.setColour (Colour (0xffe8e2c8));
    g.setFont (f (11.0f, 2.0f));
    g.drawText ("TRANSISTOR 2SC536", Rectangle<float> (pcb.getX(), 358, pcb.getWidth(), 16), Justification::centred);
    for (auto [cx, label, hv] : { std::tuple<float, const char*, float> { 110.0f, "Q1", lastH1 }, { 270.0f, "Q2", lastH2 } })
    {
        g.setColour (Colour (0xffe8e2c8));
        g.setFont (f (12.0f, 1.0f));
        g.drawText (label, Rectangle<float> (cx - 50, 386, 100, 16), Justification::centred);
        g.setFont (uiFont (11.0f));
        g.drawText ("hFE " + String (roundToInt (hv)) + "  " + hfeClass (hv), Rectangle<float> (cx - 60, 506, 120, 14), Justification::centred);
    }
    g.setColour (Colour (0xffb9b39a));
    g.setFont (uiFont (9.0f, false));
    g.drawText (String::fromUTF8 ("originale: classe F, hFE 160\xe2\x80\x93" "320"), Rectangle<float> (pcb.getX(), 548, pcb.getWidth(), 14), Justification::centred);
}

//==============================================================================
juce::Image PF2FuzzEditor::createAppIcon (int size)
{
    using namespace juce;
    Image img (Image::ARGB, size, size, true);
    Graphics g (img);
    const float u = (float) size / 1024.0f;
    auto R = [u] (float x, float y, float w, float h) { return Rectangle<float> (x * u, y * u, w * u, h * u); };

    // corpo nero martellato (griglia delle icone macOS: 824 px con ombra)
    const auto body = R (100, 100, 824, 824);
    Path bodyPath; bodyPath.addRoundedRectangle (body, 185.0f * u);
    DropShadow (Colours::black.withAlpha (0.55f), roundToInt (28 * u), { 0, roundToInt (12 * u) }).drawForPath (g, bodyPath);
    {
        Graphics::ScopedSaveState st (g);
        g.reduceClipRegion (bodyPath);
        g.drawImage (makeCrinkle (roundToInt (body.getWidth()), roundToInt (body.getHeight()), 4.0f * u), body);
    }
    g.setColour (Colour (0xff3a3c40));
    g.strokePath (bodyPath, PathStrokeType (3.0f * u));

    // pannello in acciaio con VOL e FUZZ
    const auto panel = R (172, 178, 680, 400);
    Path panelPath; panelPath.addRoundedRectangle (panel, 22.0f * u);
    {
        Graphics::ScopedSaveState st (g);
        g.reduceClipRegion (panelPath);
        g.drawImage (makeSteel (roundToInt (panel.getWidth()), roundToInt (panel.getHeight()), 2.0f * u,
                                panel.getWidth() / (2.0f * u), panel.getHeight() / (2.0f * u)), panel);
    }
    g.setColour (Colour (0xff56595e));
    g.strokePath (panelPath, PathStrokeType (3.0f * u));

    PedalLookAndFeel lnf;
    Slider dummy;
    const float start = MathConstants<float>::pi * 1.25f, end = MathConstants<float>::pi * 2.75f;
    for (auto [cx, pos] : { std::pair<float, float> { 352.0f, 0.5f }, { 672.0f, 0.7f } })
    {
        const auto kb = R (cx - 140, 378 - 140, 280, 280).toNearestInt();
        lnf.drawRotarySlider (g, kb.getX(), kb.getY(), kb.getWidth(), kb.getHeight(), pos, start, end, dummy);
    }

    // targhetta rossa con il nome
    const auto badge = R (232, 642, 560, 190);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (badge.translated (0, 8 * u), 18 * u);
    g.setGradientFill (ColourGradient (Colour (0xffd42a30), 0, badge.getY(), Colour (0xff8c1218), 0, badge.getBottom(), false));
    g.fillRoundedRectangle (badge, 18 * u);
    g.setColour (Colour (0xffe6e7ea).withAlpha (0.6f));
    g.drawRoundedRectangle (badge.reduced (12 * u), 10 * u, 3 * u);
    auto face = Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
    const Font f = Font (FontOptions (face).withPointHeight (118.0f * u)).withExtraKerningFactor (0.04f);
    ColourGradient chromeText (Colours::white, 0, badge.getY() + 40 * u, Colour (0xff9a9ea4), 0, badge.getBottom() - 40 * u, false);
    chromeText.addColour (0.55, Colour (0xffd6d9dd));
    g.setGradientFill (chromeText);
    drawBaseline (g, f, "FYN-2", badge.getCentreX(), badge.getCentreY() + 42 * u, Justification::horizontallyCentred);
    return img;
}

//==============================================================================
juce::Font PF2FuzzEditor::scriptFont (float pt) const
{
    return juce::Font (juce::FontOptions (scriptFace).withPointHeight (pt));
}

juce::Font PF2FuzzEditor::panelFont (float pt, float tracking) const
{
    return juce::Font (juce::FontOptions (panelFace).withPointHeight (pt)).withExtraKerningFactor (tracking / pt);
}

//==============================================================================
PF2FuzzEditor::PF2FuzzEditor (PF2FuzzProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    scriptFace = juce::Typeface::createSystemTypefaceFor (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
    panelFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);

    auto setupKnob = [this] (LearnableSlider& s, const char* id, bool small)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, (double) proc.apvts.getParameter (id)->getDefaultValue()
                                               * (small ? 48.0 : 1.0) + (small ? -24.0 : 0.0));
        s.setMouseDragSensitivity (small ? 160 : 240);
        s.setTooltip ("Clic destro: MIDI Learn");
        const int idx = proc.getLearnIndex (id);
        s.onLearnMenu = [this, idx, &s] { showLearnMenu (idx, &s); };
        addAndMakeVisible (s);
    };

    setupKnob (fuzz,    ParamIDs::fuzz,   false);
    setupKnob (volume,  ParamIDs::volume, false);
    setupKnob (inTrim,  ParamIDs::input,  true);
    setupKnob (outTrim, ParamIDs::output, true);

    fuzzA  = std::make_unique<SA> (proc.apvts, ParamIDs::fuzz,   fuzz);
    volA   = std::make_unique<SA> (proc.apvts, ParamIDs::volume, volume);
    inA    = std::make_unique<SA> (proc.apvts, ParamIDs::input,  inTrim);
    outA   = std::make_unique<SA> (proc.apvts, ParamIDs::output, outTrim);

    footswitch.onLearnMenu = [this] { showLearnMenu (2, &footswitch); };
    footswitch.setTooltip ("Bypass. Clic destro: MIDI Learn");
    addAndMakeVisible (footswitch);
    bypassA = std::make_unique<BA> (proc.apvts, ParamIDs::bypass, footswitch);

    // pick-up: preset fissi + chitarre salvate (vedi PickupLibrary)
    pickupBox.setTooltip ("Pick-up della chitarra collegata: il FY-2 ha un ingresso a bassa impedenza che interagisce con il pick-up");
    pickupBox.onChange = [this] { pickupChosen(); };
    addAndMakeVisible (pickupBox);
    pickupMenuBtn.setTooltip ("Salva i valori attuali con il nome della chitarra, o elimina una chitarra salvata");
    pickupMenuBtn.onClick = [this] { showPickupMenu(); };
    addAndMakeVisible (pickupMenuBtn);
    rebuildPickupBox();

    // pannello interno (alimentazione e transistor), aperto dal pulsante INTERNO
    interior = std::make_unique<InteriorPanel> (proc, panelFace, [this] (int idx, juce::Component* c) { showLearnMenu (idx, c); });
    interior->onClose = [this] { interior->setVisible (false); interiorBtn.setToggleState (false, juce::dontSendNotification); };
    addChildComponent (*interior);
    interiorBtn.setClickingTogglesState (true);
    interiorBtn.setTooltip ("Apre l'interno del pedale: batteria o alimentatore, carica, guadagno dei transistor");
    interiorBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8c1218));
    interiorBtn.onClick = [this] { interior->setVisible (interiorBtn.getToggleState()); interior->toFront (false); };
    addAndMakeVisible (interiorBtn);
    for (auto* s : { &pickR, &pickL })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 20);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxTextColourId, cream);
        s->setColour (juce::Slider::trackColourId, juce::Colour (0xff6e7278));
        s->setColour (juce::Slider::thumbColourId, cream);
        addAndMakeVisible (s);
    }
    pickR.setTooltip ("Resistenza in continua del pick-up (dato del costruttore)");
    pickL.setTooltip ("Induttanza del pick-up");
    pickRA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupR, pickR);
    pickLA = std::make_unique<SA> (proc.apvts, ParamIDs::pickupL, pickL);
    pickR.setTextValueSuffix (juce::String::fromUTF8 (" k\xce\xa9"));
    pickL.setTextValueSuffix (" H");
    pickR.setNumDecimalPlacesToDisplay (1);
    pickL.setNumDecimalPlacesToDisplay (2);

    setResizable (true, true);
    setResizeLimits (baseW * 3 / 4, baseH * 3 / 4, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    lastBypass = proc.apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;
    timerCallback();
    startTimerHz (30);
}

PF2FuzzEditor::~PF2FuzzEditor()
{
    proc.cancelLearn();
    setLookAndFeel (nullptr);
}

juce::Rectangle<float> PF2FuzzEditor::base (float x, float y, float w, float h) const
{
    return { x * scale, y * scale, w * scale, h * scale };
}

void PF2FuzzEditor::resized()
{
    scale = (float) getWidth() / (float) baseW;
    auto place = [this] (juce::Component& c, float cx, float cy, float w, float h)
    { c.setBounds (base (cx - w * 0.5f, cy - h * 0.5f, w, h).toNearestInt()); };

    // barra di servizio
    place (inTrim,  40.0f,  25.0f, 34.0f, 34.0f);
    place (outTrim, 380.0f, 25.0f, 34.0f, 34.0f);
    pickupBox.setBounds     (base (62.0f,  64.0f, 168.0f, 24.0f).toNearestInt());
    pickupMenuBtn.setBounds (base (234.0f, 64.0f, 26.0f,  24.0f).toNearestInt());
    pickR.setBounds         (base (264.0f, 64.0f, 76.0f,  24.0f).toNearestInt());
    pickL.setBounds         (base (342.0f, 64.0f, 72.0f,  24.0f).toNearestInt());
    interiorBtn.setBounds   (base (298.0f, 12.0f, 52.0f,  36.0f).toNearestInt());
    if (interior != nullptr)
        interior->setBounds (base (20.0f, pedalY + 14.0f, InteriorPanel::baseW, InteriorPanel::baseH).toNearestInt());

    // pedale (coordinate della proposta A + pedalY)
    place (footswitch, 210.0f, pedalY + 120.0f, 84.0f, 84.0f);
    place (volume,     122.0f, pedalY + 340.0f, 124.0f, 124.0f);
    place (fuzz,       298.0f, pedalY + 340.0f, 124.0f, 124.0f);

    renderBackground();
}

void PF2FuzzEditor::renderBackground()
{
    using namespace juce;
    background = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
    Graphics g (background);

    // texture generate alla risoluzione reale della finestra
    const Rectangle<float> body (20.0f, pedalY + 14.0f, 380.0f, 656.0f);
    const Rectangle<float> panel (40.0f, pedalY + 214.0f, 340.0f, 262.0f);
    // texture generate una sola volta alla risoluzione massima della finestra (2x) e
    // condivise fra tutte le istanze: il ridimensionamento non le ricalcola
    static const Image crinkle = makeCrinkle (roundToInt (body.getWidth() * 2.0f), roundToInt (body.getHeight() * 2.0f), 2.0f);
    static const Image steel = makeSteel (roundToInt (panel.getWidth() * 2.0f), roundToInt (panel.getHeight() * 2.0f), 2.0f,
                                          panel.getWidth(), panel.getHeight());

    g.addTransform (AffineTransform::scale (scale));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.fillAll (Colour (0xff08080a));

    // barra di servizio (non fa parte del pedale)
    g.setColour (stripCol);
    g.fillRect (0.0f, 0.0f, (float) baseW, 96.0f);
    g.setColour (lineCol);
    g.fillRect (0.0f, 95.0f, (float) baseW, 1.0f);
    g.setColour (Colour (0xffc9ccd0));
    g.setFont (uiFont (9.0f));
    g.drawText ("INPUT",  Rectangle<float> (14.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("OUTPUT", Rectangle<float> (354.0f, 42.0f, 52.0f, 12.0f), Justification::centred);
    g.drawText ("PICKUP", Rectangle<float> (14.0f, 64.0f, 46.0f, 24.0f), Justification::centredLeft);

    // scatola nera martellata
    Path bodyPath; bodyPath.addRoundedRectangle (body, 14.0f);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (bodyPath);
        g.drawImage (crinkle, body);
    }
    g.setColour (Colour (0xff2e2f32));
    g.strokePath (bodyPath, PathStrokeType (1.0f));

    // pannello in acciaio spazzolato
    Path panelPath; panelPath.addRoundedRectangle (panel, 5.0f);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (panelPath);
        g.drawImage (steel, panel);
    }
    g.setColour (Colour (0xff56595e));
    g.strokePath (panelPath, PathStrokeType (1.0f));

    // serigrafia del pannello
    g.setColour (ink);
    drawBaseline (g, panelFont (19.0f, 1.0f), "VOL",  122.0f, pedalY + 250.0f, Justification::horizontallyCentred);
    drawBaseline (g, panelFont (19.0f, 1.0f), "FUZZ", 298.0f, pedalY + 250.0f, Justification::horizontallyCentred);
    drawBaseline (g, panelFont (21.0f, 3.0f), "FUZZ BOX", 210.0f, pedalY + 452.0f, Justification::horizontallyCentred);
    drawBaseline (g, panelFont (11.0f, 1.0f), "FYN-2", 358.0f, pedalY + 452.0f, Justification::right);

    // targhetta rossa Collateral Effects
    const Point<float> bc (210.0f, pedalY + 580.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bc.x - 104.0f, bc.y - 38.0f, 208.0f, 80.0f, 6.0f);
    g.setGradientFill (ColourGradient (Colour (0xffd42a30), bc.x, bc.y - 42.0f, Colour (0xff8c1218), bc.x, bc.y + 38.0f, false));
    g.fillRoundedRectangle (bc.x - 104.0f, bc.y - 42.0f, 208.0f, 80.0f, 6.0f);
    g.setColour (Colour (0xffe6e7ea).withAlpha (0.55f));
    g.drawRoundedRectangle (bc.x - 99.0f, bc.y - 37.0f, 198.0f, 70.0f, 4.0f, 1.2f);
    ColourGradient chromeText (Colours::white, bc.x, bc.y - 26.0f, Colour (0xff8e9298), bc.x, bc.y + 6.0f, false);
    chromeText.addColour (0.55, Colour (0xffcfd2d6));
    g.setGradientFill (chromeText);
    drawBaseline (g, scriptFont (34.0f), "Collateral", bc.x, bc.y, Justification::horizontallyCentred);
    g.setColour (Colour (0xffeef0f2));
    drawBaseline (g, panelFont (11.0f, 5.0f), "EFFECTS", bc.x + 2.5f, bc.y + 22.0f, Justification::horizontallyCentred);
}

//==============================================================================
void PF2FuzzEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImageAt (background, 0, 0);
    g.addTransform (AffineTransform::scale (scale));

    // LED di stato accanto all'interruttore
    const Point<float> led (296.0f, pedalY + 120.0f);
    const bool active = ! lastBypass;
    if (active)
    {
        ColourGradient glow (Colour (0xffff3b30).withAlpha (0.5f), led.x, led.y,
                             Colour (0xffff3b30).withAlpha (0.0f), led.x + 20.0f, led.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (40.0f, 40.0f).withCentre (led));
    }
    g.setGradientFill (chrome (led, 8.0f));
    g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (led));
    g.setColour (Colour (0xff3d4044));
    g.drawEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (led), 0.8f);
    g.setGradientFill (active ? ColourGradient (Colour (0xffffb3a8), led.x - 1.0f, led.y - 1.5f, Colour (0xff7a0d08), led.x + 5.0f, led.y, true)
                              : ColourGradient (Colour (0xff5a1c18), led.x - 1.0f, led.y - 1.5f, Colour (0xff200605), led.x + 5.0f, led.y, true));
    g.fillEllipse (Rectangle<float> (10.0f, 10.0f).withCentre (led));

    // display MIDI
    const Rectangle<float> disp (76.0f, 8.0f, 216.0f, 44.0f);
    g.setColour (Colour (0xff0a110b));
    g.fillRoundedRectangle (disp, 4.0f);
    g.setColour (Colour (0xff23262a));
    g.drawRoundedRectangle (disp, 4.0f, 1.0f);

    const Colour lcd (0xff9be08a);
    g.setColour (lcd);
    g.setFont (uiFont (10.0f));
    String line1 = "MIDI", line2;
    const int learn = proc.getLearningParam();
    if (learn >= 0)
    {
        line1 = String ("LEARN: ") + displayNames[learn];
        line2 = "muovi un controllo sulla tastiera";
    }
    else if (const int cc = proc.lastCC.load(); cc >= 0)
    {
        int idx = -1;
        for (int i = 0; i < PF2FuzzProcessor::numLearnable; ++i)
            if (proc.getCCForParam (i) == cc) { idx = i; break; }
        line2 = "CC " + String (cc) + " = " + String (proc.lastCCValue.load())
                + (idx >= 0 ? String ("  -> ") + displayNames[idx] : String ("  (non assegnato)"));
    }
    else
        line2 = "clic destro su un comando: Learn";
    g.drawText (line1, disp.reduced (10.0f, 5.0f).removeFromTop (16.0f), Justification::centredLeft);
    g.setFont (uiFont (10.0f, false));
    g.setColour (Colour (0xff8fcf80));
    g.drawText (line2, disp.reduced (10.0f, 5.0f).removeFromBottom (16.0f), Justification::centredLeft);
}

void PF2FuzzEditor::paintOverChildren (juce::Graphics& g)
{
    const int learn = proc.getLearningParam();
    if (auto* c = controlFor (learn))
    {
        const float a = 0.45f + 0.45f * std::sin (pulse);
        g.setColour (learnCol.withAlpha (a));
        g.drawRoundedRectangle (getLocalArea (c, c->getLocalBounds()).toFloat().expanded (3.0f * scale), 8.0f * scale, 2.0f * scale);
    }
}

juce::Component* PF2FuzzEditor::controlFor (int idx)
{
    switch (idx)
    {
        case 0: return &fuzz;   case 1: return &volume; case 2: return &footswitch;
        case 3: return &inTrim; case 4: return &outTrim;
        case 5: case 6: case 7: case 8: return interior != nullptr && interior->isVisible() ? interior->controlFor (idx) : nullptr;
        default: return nullptr;
    }
}

void PF2FuzzEditor::timerCallback()
{
    const int learn = proc.getLearningParam();
    const int mv = proc.getMapVersion();
    const int cnt = proc.ccCounter.load();
    const bool byp = proc.apvts.getRawParameterValue (ParamIDs::bypass)->load() > 0.5f;

    if (learn >= 0) pulse += 0.35f;
    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    syncPickupBox();
    if (pu != lastPickup)
    {
        lastPickup = pu;
        // R e L si regolano solo in "Personalizzato": con i preset i valori sono nel nome
        const bool custom = pu == 4;
        pickR.setVisible (custom); pickL.setVisible (custom);
    }
    if (learn >= 0 || learn != lastLearn || mv != lastMapVersion || cnt != lastCCCount || byp != lastBypass)
    {
        lastLearn = learn; lastMapVersion = mv; lastCCCount = cnt; lastBypass = byp;
        repaint();
    }
}

void PF2FuzzEditor::showLearnMenu (int idx, juce::Component* target)
{
    juce::PopupMenu m;
    const int cc = proc.getCCForParam (idx);
    const bool learning = proc.getLearningParam() == idx;

    m.addSectionHeader (displayNames[idx]);
    m.addItem (1, learning ? "Annulla MIDI Learn" : "MIDI Learn (poi muovi un controllo)");
    m.addItem (2, cc >= 0 ? "Rimuovi assegnazione CC " + juce::String (cc) : "Nessun CC assegnato", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Ripristina mappatura predefinita");

    juce::Component::SafePointer<PF2FuzzEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                     [safe, idx, learning] (int r)
                     {
                         if (safe == nullptr) return;
                         auto& p = safe->proc;
                         if (r == 1) { if (learning) p.cancelLearn(); else p.startLearn (idx); }
                         else if (r == 2) p.clearMappingForParam (idx);
                         else if (r == 3) p.resetDefaultMappings();
                         safe->repaint();
                     });
}

//==============================================================================
// Pick-up: ID del menu 1-4 = preset fissi, 5 = Personalizzato, 100+ = chitarre salvate
namespace { const juce::Identifier pickupNameProp ("pickupName"); }

float PF2FuzzEditor::currentR() const
{
    switch ((int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load())
    {
        case 0: return (float) (pf2::Pickup::ideal().R / 1000.0);
        case 1: return (float) (pf2::Pickup::singleCoil().R / 1000.0);
        case 2: return (float) (pf2::Pickup::p90().R / 1000.0);
        case 3: return (float) (pf2::Pickup::humbucker().R / 1000.0);
        default: return proc.apvts.getRawParameterValue (ParamIDs::pickupR)->load();
    }
}

float PF2FuzzEditor::currentL() const
{
    switch ((int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load())
    {
        case 0: return 0.0f;
        case 1: return (float) pf2::Pickup::singleCoil().L;
        case 2: return (float) pf2::Pickup::p90().L;
        case 3: return (float) pf2::Pickup::humbucker().L;
        default: return proc.apvts.getRawParameterValue (ParamIDs::pickupL)->load();
    }
}

void PF2FuzzEditor::setParam (const char* id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void PF2FuzzEditor::rebuildPickupBox()
{
    pickupBox.clear (juce::dontSendNotification);
    pickupBox.addItem (juce::String::fromUTF8 ("Ideale (10 k\xce\xa9)"), 1);
    pickupBox.addItem (juce::String::fromUTF8 ("Single coil (6 k\xce\xa9, 2.5 H)"), 2);
    pickupBox.addItem (juce::String::fromUTF8 ("P-90 (8 k\xce\xa9, 6 H)"), 3);
    pickupBox.addItem (juce::String::fromUTF8 ("Humbucker (10 k\xce\xa9, 5 H)"), 4);
    pickupBox.addItem ("Personalizzato", 5);

    const auto& entries = library.getEntries();
    if (! entries.empty())
    {
        pickupBox.addSectionHeading ("Le mie chitarre");
        for (size_t i = 0; i < entries.size(); ++i)
            pickupBox.addItem (entries[i].name, 100 + (int) i);
    }
    lastLibVersion = library.getVersion();
    syncPickupBox();
}

// mostra nel menu la voce che corrisponde allo stato dei parametri
void PF2FuzzEditor::syncPickupBox()
{
    if (library.getVersion() != lastLibVersion) { rebuildPickupBox(); return; }

    const int pu = (int) proc.apvts.getRawParameterValue (ParamIDs::pickup)->load();
    int id = juce::jlimit (1, 5, pu + 1);
    if (pu == 4)
    {
        const auto name = proc.apvts.state.getProperty (pickupNameProp).toString();
        const int idx = library.indexOf (name);
        if (idx >= 0)
        {
            const auto& e = library.getEntries()[(size_t) idx];
            if (std::abs (e.rKohm - currentR()) < 0.051f && std::abs (e.lHenry - currentL()) < 0.026f)
                id = 100 + idx;
        }
    }
    if (pickupBox.getSelectedId() != id) pickupBox.setSelectedId (id, juce::dontSendNotification);
}

void PF2FuzzEditor::pickupChosen()
{
    const int id = pickupBox.getSelectedId();
    if (id >= 1 && id <= 5)
    {
        proc.apvts.state.setProperty (pickupNameProp, juce::String(), nullptr);
        setParam (ParamIDs::pickup, (float) (id - 1));
    }
    else if (id >= 100)
    {
        const auto& entries = library.getEntries();
        const auto i = (size_t) (id - 100);
        if (i >= entries.size()) return;
        setParam (ParamIDs::pickupR, entries[i].rKohm);
        setParam (ParamIDs::pickupL, entries[i].lHenry);
        setParam (ParamIDs::pickup, 4.0f);
        proc.apvts.state.setProperty (pickupNameProp, entries[i].name, nullptr);
    }
    syncPickupBox();
}

void PF2FuzzEditor::showPickupMenu()
{
    library.reload();   // un'altra istanza potrebbe aver salvato nel frattempo
    juce::PopupMenu m, del;
    m.addItem (1, "Salva i valori attuali come chitarra...");
    const auto& entries = library.getEntries();
    for (size_t i = 0; i < entries.size(); ++i) del.addItem (1000 + (int) i, entries[i].name);
    m.addSubMenu ("Elimina chitarra", del, ! entries.empty());

    juce::Component::SafePointer<PF2FuzzEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&pickupMenuBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1) { safe->askNameAndSave(); return; }
        const auto& e = safe->library.getEntries();
        const auto i = (size_t) (r - 1000);
        if (i < e.size()) safe->library.remove (e[i].name);
        safe->rebuildPickupBox();
    });
}

void PF2FuzzEditor::askNameAndSave()
{
    const float r = currentR(), l = currentL();
    const auto current = proc.apvts.state.getProperty (pickupNameProp).toString();

    nameDialog = std::make_unique<juce::AlertWindow> (
        "Salva pick-up",
        juce::String::fromUTF8 ("Valori attuali: ") + juce::String (r, 1) + juce::String::fromUTF8 (" k\xce\xa9, ")
            + juce::String (l, 2) + juce::String::fromUTF8 (" H.\nNome della chitarra (se esiste gi\xc3\xa0, viene aggiornata):"),
        juce::MessageBoxIconType::NoIcon, this);
    nameDialog->addTextEditor ("name", current, "Nome");
    nameDialog->addButton ("Salva", 1, juce::KeyPress (juce::KeyPress::returnKey));
    nameDialog->addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PF2FuzzEditor> safe (this);
    nameDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safe, r, l] (int result)
    {
        if (safe == nullptr) return;
        const auto name = safe->nameDialog != nullptr ? safe->nameDialog->getTextEditorContents ("name").trim() : juce::String();
        if (result == 1 && name.isNotEmpty())
        {
            safe->library.save (name, r, l);
            // la chitarra salvata diventa la selezione corrente
            safe->setParam (ParamIDs::pickupR, r);
            safe->setParam (ParamIDs::pickupL, l);
            safe->setParam (ParamIDs::pickup, 4.0f);
            safe->proc.apvts.state.setProperty (pickupNameProp, name, nullptr);
            safe->rebuildPickupBox();
        }
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->nameDialog.reset(); });
    }), false);
}
