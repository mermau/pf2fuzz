#include "PluginEditor.h"

namespace
{
    const juce::Colour bodyTop   (0xff7d1f24);
    const juce::Colour bodyBot   (0xff4a1013);
    const juce::Colour cream     (0xfff1e6cc);
    const juce::Colour stripCol  (0xff1b1b1d);
    const juce::Colour learnCol  (0xffffb020);

    juce::Font font (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    const char* displayNames[] = { "FUZZ", "VOLUME", "FOOTSWITCH", "INPUT", "OUTPUT" };

    void fillChrome (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> b, float darken = 0.0f)
    {
        juce::ColourGradient cg (juce::Colour (0xffeeeeee).darker (darken), b.getX(), b.getY(),
                                 juce::Colour (0xff6d6d70).darker (darken), b.getRight(), b.getBottom(), false);
        cg.addColour (0.45, juce::Colour (0xffb9b9bc).darker (darken));
        g.setGradientFill (cg);
        g.fillPath (p);
    }
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff222224));
    setColour (juce::PopupMenu::textColourId, cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, bodyTop);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee111111));
    setColour (juce::BubbleComponent::outlineColourId, cream.withAlpha (0.4f));
    setColour (juce::TooltipWindow::textColourId, cream);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    using namespace juce;
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.49f;

    // scala serigrafata
    g.setColour (cream.withAlpha (0.85f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float inner = (i % 5 == 0) ? 0.80f : 0.86f;
        g.drawLine (Line<float> (c.getPointOnCircumference (R * inner, a), c.getPointOnCircumference (R * 0.95f, a)),
                    i % 5 == 0 ? 2.0f : 1.2f);
    }

    const float kr = R * 0.70f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // ombra
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (Rectangle<float> (kr * 2.1f, kr * 2.1f).withCentre (c.translated (0, kr * 0.14f)));

    // gonna zigrinata
    ColourGradient skirt (Colour (0xff3d3d40), c.x, c.y - kr, Colour (0xff0d0d0e), c.x, c.y + kr, false);
    g.setGradientFill (skirt);
    g.fillEllipse (Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.09f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = angle + (float) i * MathConstants<float>::twoPi / 36.0f;
        g.drawLine (Line<float> (c.getPointOnCircumference (kr * 0.82f, a), c.getPointOnCircumference (kr * 0.99f, a)), 1.0f);
    }

    // cappello
    const float cr = kr * 0.74f;
    ColourGradient cap (Colour (0xff57575b), c.x - cr * 0.35f, c.y - cr * 0.45f,
                        Colour (0xff111112), c.x + cr, c.y + cr, true);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c));
    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawEllipse (Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (c), 1.0f);

    // indice
    g.setColour (cream);
    Path p;
    p.addLineSegment (Line<float> (c.getPointOnCircumference (cr * 0.20f, angle), c.getPointOnCircumference (kr * 0.96f, angle)), 0.0f);
    g.strokePath (p, PathStrokeType (jmax (2.0f, kr * 0.11f), PathStrokeType::curved, PathStrokeType::rounded));
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

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = b.getCentre().translated (0.0f, down ? 1.5f : 0.0f);
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (b.getCentre().translated (0, R * 0.08f)));

    Path nut; nut.addPolygon (c, 6, R * 0.97f, MathConstants<float>::pi / 6.0f);
    fillChrome (g, nut, nut.getBounds(), down ? 0.25f : 0.0f);

    Path ring; ring.addEllipse (Rectangle<float> (R * 1.45f, R * 1.45f).withCentre (c));
    fillChrome (g, ring, ring.getBounds().translated (0, R * 0.3f), 0.35f);

    Path cap; cap.addEllipse (Rectangle<float> (R * 1.18f, R * 1.18f).withCentre (c));
    fillChrome (g, cap, cap.getBounds(), down ? 0.3f : (highlighted ? -0.08f : 0.05f));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawEllipse (Rectangle<float> (R * 1.18f, R * 1.18f).withCentre (c), 1.0f);
}

//==============================================================================
PF2FuzzEditor::PF2FuzzEditor (PF2FuzzProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (LearnableSlider& s, const char* id, bool small)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setPopupDisplayEnabled (true, true, this);
        s.setDoubleClickReturnValue (true, (double) proc.apvts.getParameter (id)->getDefaultValue()
                                               * (small ? 48.0 : 1.0) + (small ? -24.0 : 0.0));
        s.setMouseDragSensitivity (small ? 160 : 220);
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
    pickupMenuBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2d));
    pickupMenuBtn.setColour (juce::TextButton::textColourOffId, cream);
    pickupMenuBtn.onClick = [this] { showPickupMenu(); };
    addAndMakeVisible (pickupMenuBtn);
    rebuildPickupBox();
    for (auto* s : { &pickR, &pickL })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 46, 20);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxTextColourId, cream);
        s->setColour (juce::Slider::trackColourId, bodyTop);
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

    place (inTrim,  40.0f,  36.0f, 52.0f, 52.0f);
    place (outTrim, 340.0f, 36.0f, 52.0f, 52.0f);
    place (fuzz,    118.0f, 204.0f + dy, 116.0f, 116.0f);
    place (volume,  262.0f, 204.0f + dy, 116.0f, 116.0f);
    place (footswitch, 190.0f, 560.0f + dy, 82.0f, 82.0f);
    pickupBox.setBounds (base (56.0f, 84.0f, 146.0f, 22.0f).toNearestInt());
    pickupMenuBtn.setBounds (base (204.0f, 84.0f, 22.0f, 22.0f).toNearestInt());
    pickR.setBounds (base (230.0f, 84.0f, 74.0f, 22.0f).toNearestInt());
    pickL.setBounds (base (306.0f, 84.0f, 70.0f, 22.0f).toNearestInt());

    renderBackground();
}

void PF2FuzzEditor::renderBackground()
{
    using namespace juce;
    background = Image (Image::ARGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    g.fillAll (Colour (0xff101011));

    // barra superiore (controlli di servizio, non presenti sul pedale)
    g.setColour (stripCol);
    g.fillRect (0.0f, 0.0f, (float) baseW, 76.0f + dy);
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawLine (0.0f, 76.0f + dy, (float) baseW, 76.0f + dy, 1.0f);
    g.drawLine (10.0f, 78.0f, (float) baseW - 10.0f, 78.0f, 1.0f);
    g.setColour (cream.withAlpha (0.6f));
    g.setFont (font (9.0f));
    g.drawText ("INPUT",  Rectangle<float> (10.0f, 62.0f, 60.0f, 12.0f), Justification::centred);
    g.drawText ("OUTPUT", Rectangle<float> (310.0f, 62.0f, 60.0f, 12.0f), Justification::centred);
    g.drawText ("PICKUP", Rectangle<float> (8.0f, 84.0f, 50.0f, 22.0f), Justification::centredLeft);

    g.addTransform (AffineTransform::translation (0.0f, dy));   // da qui in giù: il pedale

    // jack laterali
    for (auto [x, label] : { std::pair<float, const char*> { 4.0f, "OUT" }, { 362.0f, "IN" } })
    {
        Path jack; jack.addRoundedRectangle (x, 220.0f, 14.0f, 34.0f, 3.0f);
        fillChrome (g, jack, jack.getBounds(), 0.2f);
        g.setColour (cream.withAlpha (0.55f));
        g.setFont (font (8.0f));
        g.drawText (label, Rectangle<float> (x < 100.0f ? 24.0f : 330.0f, 232.0f, 26.0f, 10.0f), Justification::centred);
    }

    // scatola
    const Rectangle<float> body (16.0f, 92.0f, 348.0f, 534.0f);
    DropShadow (Colours::black.withAlpha (0.7f), 18, { 0, 6 }).drawForRectangle (g, body.toNearestInt());
    ColourGradient bg (bodyTop, body.getX(), body.getY(), bodyBot, body.getRight(), body.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (body, 20.0f);

    // vernice metallizzata
    Random rnd (2026);
    for (int i = 0; i < 900; ++i)
    {
        const float px = body.getX() + rnd.nextFloat() * body.getWidth();
        const float py = body.getY() + rnd.nextFloat() * body.getHeight();
        g.setColour (Colours::white.withAlpha (0.03f + rnd.nextFloat() * 0.07f));
        g.fillRect (px, py, 1.0f, 1.0f);
    }
    ColourGradient sheen (Colours::white.withAlpha (0.10f), body.getX(), body.getY(),
                          Colours::transparentWhite, body.getX(), body.getY() + 160.0f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (body, 20.0f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (body, 20.0f, 1.5f);

    // viti
    for (auto pt : { Point<float> (34, 110), Point<float> (346, 110), Point<float> (34, 608), Point<float> (346, 608) })
    {
        Path s; s.addEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (pt));
        fillChrome (g, s, s.getBounds(), 0.1f);
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawLine (Line<float> (pt.translated (-3.5f, -2.0f), pt.translated (3.5f, 2.0f)), 1.3f);
    }

    // serigrafia
    g.setColour (cream);
    g.setFont (font (13.0f));
    for (auto [x, label] : { std::pair<float, const char*> { 118.0f, "FUZZ" }, { 262.0f, "VOLUME" } })
        g.drawText (label, Rectangle<float> (x - 50.0f, 264.0f, 100.0f, 16.0f), Justification::centred);

    g.drawLine (60.0f, 392.0f, 320.0f, 392.0f, 1.0f);
    g.setFont (font (58.0f));
    g.drawText ("PF-2", Rectangle<float> (40.0f, 396.0f, 300.0f, 62.0f), Justification::centred);
    g.setFont (font (15.0f));
    g.drawText ("F  U  Z  Z", Rectangle<float> (40.0f, 456.0f, 300.0f, 18.0f), Justification::centred);
    g.drawLine (60.0f, 480.0f, 320.0f, 480.0f, 1.0f);
    g.setFont (font (8.0f, false));
    g.setColour (cream.withAlpha (0.7f));
    g.drawText ("PARMA", Rectangle<float> (40.0f, 484.0f, 300.0f, 11.0f), Justification::centred);
}

//==============================================================================
void PF2FuzzEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.drawImageAt (background, 0, 0);
    g.addTransform (AffineTransform::scale (scale));

    // LED
    g.saveState();
    g.addTransform (AffineTransform::translation (0.0f, dy));
    const Point<float> led (190.0f, 340.0f);
    const bool active = ! lastBypass;
    if (active)
    {
        ColourGradient glow (Colour (0xffff3b30).withAlpha (0.55f), led.x, led.y,
                             Colour (0xffff3b30).withAlpha (0.0f), led.x + 22.0f, led.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (Rectangle<float> (44.0f, 44.0f).withCentre (led));
    }
    g.setColour (Colour (0xff1a1a1a));
    g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (led));
    g.setColour (active ? Colour (0xffff4a3d) : Colour (0xff4a1a1a));
    g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (led));
    g.setColour (Colours::white.withAlpha (active ? 0.7f : 0.15f));
    g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (led.translated (-2.0f, -2.0f)));

    // assegnazioni CC
    g.setFont (font (9.0f, false));
    auto ccText = [this] (int idx)
    {
        const int cc = proc.getCCForParam (idx);
        return proc.getLearningParam() == idx ? String ("LEARN...") : (cc >= 0 ? "CC " + String (cc) : String ("--"));
    };
    g.setColour (cream.withAlpha (0.55f));
    const float xs[] = { 118.0f, 262.0f };
    for (int i = 0; i < 2; ++i)
        g.drawText (ccText (i), Rectangle<float> (xs[i] - 40.0f, 280.0f, 80.0f, 12.0f), Justification::centred);
    g.drawText (ccText (2), Rectangle<float> (238.0f, 554.0f, 80.0f, 12.0f), Justification::centredLeft);
    g.restoreState();
    g.setFont (font (9.0f, false));
    g.setColour (cream.withAlpha (0.45f));
    g.drawText (ccText (3), Rectangle<float> (0.0f, 4.0f, 80.0f, 10.0f), Justification::centred);
    g.drawText (ccText (4), Rectangle<float> (300.0f, 4.0f, 80.0f, 10.0f), Justification::centred);

    // display MIDI
    const Rectangle<float> disp (86.0f, 16.0f, 208.0f, 42.0f);
    g.setColour (Colour (0xff0a0f0a));
    g.fillRoundedRectangle (disp, 5.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (disp, 5.0f, 1.0f);

    const Colour lcd (0xff9be08a);
    g.setColour (lcd);
    g.setFont (font (10.0f));
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
    g.drawText (line1, disp.reduced (8.0f, 3.0f).removeFromTop (15.0f), Justification::centredLeft);
    g.setFont (font (9.5f, false));
    g.setColour (lcd.withAlpha (0.85f));
    g.drawText (line2, disp.reduced (8.0f, 3.0f).removeFromBottom (15.0f), Justification::centredLeft);
}

void PF2FuzzEditor::paintOverChildren (juce::Graphics& g)
{
    const int learn = proc.getLearningParam();
    if (auto* c = controlFor (learn))
    {
        const float a = 0.45f + 0.45f * std::sin (pulse);
        g.setColour (learnCol.withAlpha (a));
        g.drawRoundedRectangle (c->getBounds().toFloat().expanded (3.0f * scale), 8.0f * scale, 2.0f * scale);
    }
}

juce::Component* PF2FuzzEditor::controlFor (int idx)
{
    switch (idx)
    {
        case 0: return &fuzz;   case 1: return &volume; case 2: return &footswitch;
        case 3: return &inTrim; case 4: return &outTrim;
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
        const bool custom = pu == 4;
        pickR.setEnabled (custom); pickL.setEnabled (custom);
        pickR.setAlpha (custom ? 1.0f : 0.35f); pickL.setAlpha (custom ? 1.0f : 0.35f);
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
