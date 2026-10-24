#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff0f1b2d);
const juce::Colour kPanel (0xff17263d);
const juce::Colour kTeal (0xff2ec4b6);
const juce::Colour kYellow (0xffffd166);
const juce::Colour kOrange (0xfff4a259);
const juce::Colour kText (0xffe8eef7);
const juce::Colour kMuted (0xff8fa3bf);

juce::String T (const char* s) { return juce::String::fromUTF8 (s); }
juce::String T (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
} // namespace

// ============================================================================ JackFace
void JackFace::drawArm (juce::Graphics& g, int side, juce::Rectangle<float> body, float scale)
{
    // side: 0 = sağ, 1 = sol
    const float s = side == 0 ? 1.0f : -1.0f;
    const float sh = static_cast<float> (side) * 1.7f;
    const float t = static_cast<float> (timeMs);
    float tgTh = 0.2f, tgB = 0.1f;
    switch (mood)
    {
        case jack::Mood::Sleepy:      tgTh = 0.16f; tgB = 0.08f; break;
        case jack::Mood::Relaxed:     tgTh = 0.32f + std::sin (t * 0.0016f + sh) * 0.05f + level * 0.15f; tgB = 0.25f; break;
        case jack::Mood::Working:     tgTh = 0.75f + level * 0.55f + std::sin (t * 0.006f + sh) * 0.08f; tgB = 0.7f + level * 0.4f; break;
        case jack::Mood::Strained:    tgTh = 2.45f + std::sin (t * 0.05f + sh) * 0.04f; tgB = 1.1f; break;
        case jack::Mood::Overwhelmed: tgTh = 2.2f + std::sin (t * 0.02f + sh * 2.0f) * 0.75f; tgB = std::sin (t * 0.03f + sh) * 0.9f; break;
        case jack::Mood::Worried:     tgTh = 1.25f; tgB = 2.3f; break;
    }
    auto& a = arms[side];
    a.th += (tgTh - a.th) * 0.14f;
    a.bend += (tgB - a.bend) * 0.14f;

    const float U = 30.0f * scale, F = 26.0f * scale;
    const juce::Point<float> shoulder (body.getCentreX() + s * (body.getWidth() * 0.5f - 3.0f), body.getY() + body.getHeight() * 0.5f);
    const juce::Point<float> elbow (shoulder.x + s * std::sin (a.th) * U, shoulder.y + std::cos (a.th) * U);
    const float a2 = a.th + a.bend;
    const juce::Point<float> hand (elbow.x + s * std::sin (a2) * F, elbow.y + std::cos (a2) * F);

    const juce::Colour dark (0xff0e5a63), mid (0xff1b8f9a), lite (0xff7fe0e6);
    auto limb = [&] (juce::Point<float> p0, juce::Point<float> p1, float w)
    {
        juce::Path p;
        p.startNewSubPath (p0);
        p.lineTo (p1);
        const auto st = juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (dark);
        g.strokePath (p, st);
        g.setColour (mid);
        g.strokePath (p, juce::PathStrokeType (w * 0.72f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-1.0f, -1.0f));
        g.setColour (lite.withAlpha (0.5f));
        g.strokePath (p, juce::PathStrokeType (w * 0.22f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-w * 0.18f, -w * 0.2f));
    };
    limb (shoulder, elbow, 11.0f * scale);
    limb (elbow, hand, 10.0f * scale);

    const float hr = 9.5f * scale;
    juce::ColourGradient hg (lite, hand.x - hr * 0.4f, hand.y - hr * 0.5f, dark, hand.x + hr, hand.y + hr, true);
    g.setGradientFill (hg);
    g.fillEllipse (hand.x - hr, hand.y - hr, hr * 2.0f, hr * 2.0f);
}

void JackFace::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const float groundY = b.getBottom() - 8.0f;
    const float squish = juce::jlimit (0.0f, 1.0f, grDb / 15.0f);
    const float bodyW = b.getHeight() * 0.64f * (1.0f + 0.30f * squish);
    const float bodyH = b.getHeight() * 0.62f * (1.0f - 0.40f * squish);
    const juce::Rectangle<float> body (cx - bodyW * 0.5f, groundY - bodyH, bodyW, bodyH);
    const float corner = bodyH * 0.42f;

    // gölge
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (cx - bodyW * 0.48f, groundY - 4.0f, bodyW * 0.96f, 10.0f);

    const bool armsFront = (mood == jack::Mood::Strained || mood == jack::Mood::Worried);
    if (! armsFront)
    {
        drawArm (g, 1, body, 1.0f);
        drawArm (g, 0, body, 1.0f);
    }

    // anten: dalga biçimli
    {
        const float topY = body.getY();
        juce::Path stem;
        stem.startNewSubPath (cx, topY + 2.0f);
        stem.lineTo (cx, topY - 14.0f);
        g.setColour (kMuted);
        g.strokePath (stem, juce::PathStrokeType (3.0f));

        juce::Path wave;
        for (int i = 0; i <= 24; ++i)
        {
            const float x = cx - 20.0f + 40.0f * static_cast<float> (i) / 24.0f;
            const float y = topY - 22.0f - 6.0f * std::sin (static_cast<float> (i) / 24.0f * 6.2831853f * 1.5f);
            if (i == 0)
                wave.startNewSubPath (x, y);
            else
                wave.lineTo (x, y);
        }
        g.setColour (kTeal);
        g.strokePath (wave, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // kulaklık
    {
        const float earW = bodyW * 0.09f, earH = bodyH * 0.38f;
        g.setColour (kYellow.darker (0.35f));
        g.fillRoundedRectangle (body.getX() - earW * 0.65f, body.getCentreY() - earH * 0.5f, earW, earH, 5.0f);
        g.fillRoundedRectangle (body.getRight() - earW * 0.35f, body.getCentreY() - earH * 0.5f, earW, earH, 5.0f);
    }

    // gövde
    juce::ColourGradient grad (juce::Colour (0xff7fe0e6), body.getX() + bodyW * 0.3f, body.getY() + bodyH * 0.18f,
                               juce::Colour (0xff0e5a63), body.getRight(), body.getBottom(), true);
    grad.addColour (0.35, juce::Colour (0xff1b8f9a));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (body, corner);
    // parlak yansıma
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillEllipse (body.getX() + bodyW * 0.14f, body.getY() + bodyH * 0.07f, bodyW * 0.3f, bodyH * 0.14f);
    g.setColour (kTeal.withAlpha (0.8f));
    g.drawRoundedRectangle (body, corner, 2.0f);

    // gözlük (visor)
    const float eyeY = body.getY() + bodyH * 0.40f;
    const juce::Rectangle<float> visor (body.getX() + bodyW * 0.12f, eyeY - bodyH * 0.22f, bodyW * 0.76f, bodyH * 0.44f);
    g.setColour (juce::Colour (0xff0b1422));
    g.fillRoundedRectangle (visor, visor.getHeight() * 0.45f);

    const bool worried = (mood == jack::Mood::Worried);
    const bool sleepy = (mood == jack::Mood::Sleepy);
    const juce::Colour eyeCol = worried ? kOrange : kYellow;
    const float dx = bodyW * 0.20f;
    const float eyeW = bodyW * 0.16f;
    float eyeH = bodyH * 0.28f;
    if (mood == jack::Mood::Strained) eyeH *= 0.45f;
    if (mood == jack::Mood::Overwhelmed) eyeH *= 1.15f;

    for (int side = -1; side <= 1; side += 2)
    {
        const float ex = cx + static_cast<float> (side) * dx;
        if (sleepy)
        {
            g.setColour (eyeCol);
            g.drawLine (ex - eyeW * 0.5f, eyeY, ex + eyeW * 0.5f, eyeY, 3.0f);
        }
        else
        {
            g.setColour (eyeCol);
            g.fillEllipse (ex - eyeW * 0.5f, eyeY - eyeH * 0.5f, eyeW, eyeH);
            g.setColour (juce::Colour (0xff0b1422));
            const float pr = juce::jmin (eyeW, eyeH) * 0.32f;
            g.fillEllipse (ex - pr * 0.5f, eyeY - pr * 0.5f, pr, pr);
        }

        if (worried)
        {
            // kaşlar: iç uçlar yukarıda
            g.setColour (kOrange);
            const float by = eyeY - eyeH * 0.5f - 7.0f;
            const float outerX = ex - static_cast<float> (side) * eyeW * 0.5f;
            const float innerX = ex + static_cast<float> (side) * eyeW * 0.5f;
            g.drawLine (outerX, by + 4.0f, innerX, by - 4.0f, 3.0f);
        }
    }

    // ağız
    const float mouthY = body.getY() + bodyH * 0.80f;
    const float mw = bodyW * 0.11f;
    juce::Path mouth;
    g.setColour (kText);
    switch (mood)
    {
        case jack::Mood::Sleepy:
            mouth.startNewSubPath (cx - mw * 0.4f, mouthY);
            mouth.lineTo (cx + mw * 0.4f, mouthY);
            break;
        case jack::Mood::Relaxed:
            mouth.startNewSubPath (cx - mw, mouthY - 1.0f);
            mouth.quadraticTo (cx, mouthY + 7.0f, cx + mw, mouthY - 1.0f);
            break;
        case jack::Mood::Working:
            if (level > 0.25f)
                mouth.addEllipse (cx - mw * 0.6f, mouthY - 3.0f, mw * 1.2f, 4.0f + level * 12.0f);  // şarkı söyler gibi
            else
            {
                mouth.startNewSubPath (cx - mw * 1.3f, mouthY - 2.0f);
                mouth.quadraticTo (cx, mouthY + 12.0f, cx + mw * 1.3f, mouthY - 2.0f);
            }
            break;
        case jack::Mood::Strained:
        case jack::Mood::Worried:
            mouth.startNewSubPath (cx - mw, mouthY);
            mouth.quadraticTo (cx - mw * 0.5f, mouthY - 4.0f, cx, mouthY);
            mouth.quadraticTo (cx + mw * 0.5f, mouthY + 4.0f, cx + mw, mouthY);
            break;
        case jack::Mood::Overwhelmed:
            mouth.addEllipse (cx - mw * 0.35f, mouthY - 3.0f, mw * 0.7f, 12.0f);
            break;
    }
    g.strokePath (mouth, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ter damlası
    if (mood == jack::Mood::Strained || mood == jack::Mood::Overwhelmed)
    {
        g.setColour (juce::Colour (0xff7ec8ff).withAlpha (0.9f));
        g.fillEllipse (body.getRight() - bodyW * 0.14f, body.getY() + bodyH * 0.10f, 9.0f, 13.0f);
    }

    // uyku
    if (sleepy)
    {
        g.setColour (kMuted);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText ("z z z", juce::Rectangle<float> (body.getRight() - 6.0f, body.getY() - 8.0f, 50.0f, 22.0f),
                    juce::Justification::centredLeft);
    }

    // eller yüzün önünde ya da başın üstünde
    if (armsFront)
    {
        drawArm (g, 1, body, 1.0f);
        drawArm (g, 0, body, 1.0f);
    }
}

// ============================================================================ Editor
void JackAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix,
                                          int decimals)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (decimals);
    s.setColour (juce::Slider::rotarySliderFillColourId, kTeal);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kPanel.brighter (0.25f));
    s.setColour (juce::Slider::thumbColourId, kYellow);
    s.setColour (juce::Slider::textBoxTextColourId, kText);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, kMuted);
    l.attachToComponent (&s, false);
    addAndMakeVisible (l);
}

void JackAudioProcessorEditor::styleChoiceButton (juce::TextButton& b)
{
    b.setClickingTogglesState (false);
    b.setColour (juce::TextButton::buttonColourId, kPanel);
    b.setColour (juce::TextButton::buttonOnColourId, kTeal);
    b.setColour (juce::TextButton::textColourOffId, kText);
    b.setColour (juce::TextButton::textColourOnId, kBg);
    addAndMakeVisible (b);
}

JackAudioProcessorEditor::JackAudioProcessorEditor (JackAudioProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (kWidth, kHeightBasic);
    setResizable (false, false);

    addAndMakeVisible (face);

    // kaynak düğmeleri
    for (int i = 0; i < static_cast<int> (jack::Source::Count); ++i)
    {
        auto& b = sourceBtns[i];
        b.setButtonText (T (jack::profile (static_cast<jack::Source> (i)).name));
        styleChoiceButton (b);
        b.onClick = [this, i]
        {
            proc.setParamReal ("source", static_cast<float> (i));
            const auto now = juce::Time::currentTimeMillis();
            sourceText = T (jack::sourceLine (static_cast<jack::Source> (i)));
            sourceUntil = now + 5000;
            mutedUntil.clear();  // kaynak değişince önceki "reddedilen" öneriler geçersiz
        };
    }

    // his düğmeleri
    const char* feelNames[3] = { "Yumuşak", "Dengeli", "Vurucu" };
    for (int i = 0; i < 3; ++i)
    {
        feelBtns[i].setButtonText (T (feelNames[i]));
        styleChoiceButton (feelBtns[i]);
        feelBtns[i].onClick = [this, i] { proc.setParamReal ("feel", static_cast<float> (i)); };
    }
    feelLabel.setText (T ("HİS"), juce::dontSendNotification);
    feelLabel.setColour (juce::Label::textColourId, kMuted);
    feelLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (feelLabel);

    // ana düğmeler
    setupKnob (amountSlider, amountLabel, T ("SIKIŞTIR"), " %", 0);
    setupKnob (mixSlider, mixLabel, T ("KARIŞIM"), " %", 0);
    setupKnob (trimSlider, trimLabel, T ("ÇIKIŞ"), " dB", 1);
    amountSlider.setColour (juce::Slider::rotarySliderFillColourId, kYellow);

    // gelişmiş düğmeler
    setupKnob (thresholdSlider, thresholdLabel, "THRESHOLD", " dB", 1);
    setupKnob (ratioSlider, ratioLabel, "RATIO", ":1", 1);
    setupKnob (attackSlider, attackLabel, "ATTACK", " ms", 1);
    setupKnob (releaseSlider, releaseLabel, "RELEASE", " ms", 0);
    setupKnob (kneeSlider, kneeLabel, "KNEE", " dB", 1);
    setupKnob (hpfSlider, hpfLabel, "SC HPF", " Hz", 0);
    setupKnob (makeupSlider, makeupLabel, "MAKEUP", " dB", 1);

    auto styleToggle = [this] (juce::ToggleButton& t, const juce::String& text)
    {
        t.setButtonText (text);
        t.setColour (juce::ToggleButton::textColourId, kText);
        t.setColour (juce::ToggleButton::tickColourId, kTeal);
        t.setColour (juce::ToggleButton::tickDisabledColourId, kMuted);
        addAndMakeVisible (t);
    };
    styleToggle (advancedToggle, T ("Gelişmiş"));
    styleToggle (autoMakeupToggle, T ("Otomatik makeup (adil karşılaştırma)"));
    styleToggle (progRelToggle, T ("Program release (sıkıştıkça yavaş bırak)"));

    // parametre bağlantıları (düğmeler yukarıda oluşturulduktan sonra)
    amountAtt = std::make_unique<SliderAtt> (proc.apvts, "amount", amountSlider);
    mixAtt = std::make_unique<SliderAtt> (proc.apvts, "mix", mixSlider);
    trimAtt = std::make_unique<SliderAtt> (proc.apvts, "trim", trimSlider);
    thresholdAtt = std::make_unique<SliderAtt> (proc.apvts, "threshold", thresholdSlider);
    ratioAtt = std::make_unique<SliderAtt> (proc.apvts, "ratio", ratioSlider);
    attackAtt = std::make_unique<SliderAtt> (proc.apvts, "attack", attackSlider);
    releaseAtt = std::make_unique<SliderAtt> (proc.apvts, "release", releaseSlider);
    kneeAtt = std::make_unique<SliderAtt> (proc.apvts, "knee", kneeSlider);
    hpfAtt = std::make_unique<SliderAtt> (proc.apvts, "hpf", hpfSlider);
    makeupAtt = std::make_unique<SliderAtt> (proc.apvts, "makeup", makeupSlider);
    advancedAtt = std::make_unique<ButtonAtt> (proc.apvts, "advanced", advancedToggle);
    autoMakeupAtt = std::make_unique<ButtonAtt> (proc.apvts, "automakeup", autoMakeupToggle);
    progRelAtt = std::make_unique<ButtonAtt> (proc.apvts, "progrel", progRelToggle);

    // öneri düğmeleri
    applyBtn.setColour (juce::TextButton::buttonColourId, kTeal);
    applyBtn.setColour (juce::TextButton::textColourOffId, kBg);
    noBtn.setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.2f));
    noBtn.setColour (juce::TextButton::textColourOffId, kText);
    noBtn.setButtonText (T ("Böyle kalsın"));
    addChildComponent (applyBtn);
    addChildComponent (noBtn);

    applyBtn.onClick = [this]
    {
        if (! current)
            return;
        const auto now = juce::Time::currentTimeMillis();
        proc.applyPatchToParams (current->patch);
        mutedUntil[current->ruleId] = now + 6000;  // yeni ölçüm gelene kadar tekrar sorma
        setFeedback (T (jack::appliedLine()), 3500);
        current.reset();
    };
    noBtn.onClick = [this]
    {
        if (! current)
            return;
        const auto now = juce::Time::currentTimeMillis();
        mutedUntil[current->ruleId] = now + 30000;  // 30 sn sus
        setFeedback (T (jack::declinedLine()), 3500);
        current.reset();
    };

    greetUntil = juce::Time::currentTimeMillis() + 7000;
    advancedShown = proc.readParams().advanced;
    updateAdvancedVisibility();
    setSize (kWidth, advancedShown ? kHeightAdvanced : kHeightBasic);
    startTimerHz (30);
}

JackAudioProcessorEditor::~JackAudioProcessorEditor()
{
    stopTimer();
}

void JackAudioProcessorEditor::setFeedback (const juce::String& text, int ms)
{
    feedbackText = text;
    feedbackUntil = juce::Time::currentTimeMillis() + ms;
}

void JackAudioProcessorEditor::updateAdvancedVisibility()
{
    juce::Slider* advSliders[] = { &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                   &kneeSlider,      &hpfSlider,   &makeupSlider };
    for (auto* s : advSliders)
        s->setVisible (advancedShown);
    autoMakeupToggle.setVisible (advancedShown);
    progRelToggle.setVisible (advancedShown);

    // gelişmişte kullanılmayan basit kontroller soluk görünür
    amountSlider.setEnabled (! advancedShown);
    for (auto& b : feelBtns)
        b.setEnabled (! advancedShown);
}

void JackAudioProcessorEditor::refreshAdvice (const jack::Params& p, juce::int64 now)
{
    const auto list = jack::advise (p, proc.readStats());
    allGood = false;
    current.reset();
    for (const auto& a : list)
    {
        if (a.ruleId == jack::kRuleAllGood)
        {
            allGood = true;
            continue;
        }
        const auto it = mutedUntil.find (a.ruleId);
        if (it != mutedUntil.end() && now < it->second)
            continue;
        if (a.applyLabel.empty())
            continue;
        current = a;
        break;
    }
    if (current)
        applyBtn.setButtonText (T (current->applyLabel));
}

juce::String JackAudioProcessorEditor::bubbleText (juce::int64 now) const
{
    if (current)
        return T (current->message);
    if (now < feedbackUntil)
        return feedbackText;
    if (now < sourceUntil)
        return sourceText;
    if (now < greetUntil)
        return T (jack::greetingLine());
    if (allGood)
        return T ("Ayarlar harika görünüyor. Aynen devam!");
    return T (jack::moodLine (mood));
}

void JackAudioProcessorEditor::timerCallback()
{
    const auto p = proc.readParams();
    const auto now = juce::Time::currentTimeMillis();

    for (int i = 0; i < static_cast<int> (jack::Source::Count); ++i)
        sourceBtns[i].setToggleState (i == static_cast<int> (p.source), juce::dontSendNotification);
    for (int i = 0; i < 3; ++i)
        feelBtns[i].setToggleState (i == static_cast<int> (p.feel), juce::dontSendNotification);

    if (p.advanced != advancedShown)
    {
        advancedShown = p.advanced;
        updateAdvancedVisibility();
        setSize (kWidth, advancedShown ? kHeightAdvanced : kHeightBasic);
    }

    grDisp += (proc.getGrDb() - grDisp) * 0.4f;
    if ((++frame % 15) == 0)
        refreshAdvice (p, now);

    mood = jack::moodFor (grDisp, proc.signalPresent(), current.has_value());
    face.setState (grDisp, mood, juce::jlimit (0.0f, 1.0f, juce::Decibels::decibelsToGain (proc.getInDb()) * 1.1f),
                   static_cast<double> (juce::Time::getMillisecondCounterHiRes()));

    applyBtn.setVisible (current.has_value());
    noBtn.setVisible (current.has_value());
    repaint (bubbleArea.getUnion (meterArea));
}

void JackAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    const auto now = juce::Time::currentTimeMillis();

    // konuşma balonu
    {
        const auto r = bubbleArea.toFloat();
        g.setColour (kPanel.brighter (0.12f));
        g.fillRoundedRectangle (r, 14.0f);
        juce::Path tail;
        tail.addTriangle (r.getX() - 10.0f, r.getY() + 46.0f, r.getX() + 2.0f, r.getY() + 36.0f, r.getX() + 2.0f, r.getY() + 58.0f);
        g.fillPath (tail);
        g.setColour (current ? kOrange : kTeal);
        g.drawRoundedRectangle (r, 14.0f, 1.5f);

        auto textArea = bubbleArea.reduced (14, 10);
        if (applyBtn.isVisible())
            textArea.removeFromBottom (40);
        g.setColour (kText);
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (bubbleText (now), textArea, juce::Justification::centredLeft, 5);
    }

    // GR göstergesi
    {
        const auto r = meterArea.toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 6.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, grDisp / 20.0f);
        const auto col = grDisp < 6.0f ? kTeal : (grDisp < 12.0f ? kYellow : kOrange);
        g.setColour (col);
        g.fillRoundedRectangle (r.withTrimmedRight (r.getWidth() * (1.0f - frac)).reduced (2.0f), 5.0f);
        g.setColour (kText);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String::formatted ("GR  %.1f dB", grDisp), meterArea.reduced (10, 0), juce::Justification::centredLeft);
    }

    g.setColour (kMuted.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Wavejackers  |  Jack Compressor", juce::Rectangle<int> (20, kHeightBasic - 22, kWidth - 40, 16),
                juce::Justification::centredRight);
}

void JackAudioProcessorEditor::resized()
{
    face.setBounds (14, 8, 200, 168);
    bubbleArea = { 226, 16, 300, 138 };
    applyBtn.setBounds (bubbleArea.getX() + 12, bubbleArea.getBottom() - 40, 150, 28);
    noBtn.setBounds (bubbleArea.getX() + 172, bubbleArea.getBottom() - 40, 116, 28);

    // kaynak seçimi: 4 + 3 düğme
    for (int i = 0; i < 4; ++i)
        sourceBtns[i].setBounds (20 + i * 126, 186, 120, 28);
    for (int i = 4; i < static_cast<int> (jack::Source::Count); ++i)
        sourceBtns[i].setBounds (20 + (i - 4) * 126, 220, 120, 28);

    // ana kontroller
    amountSlider.setBounds (40, 276, 190, 142);
    feelLabel.setBounds (270, 254, 250, 18);
    for (int i = 0; i < 3; ++i)
        feelBtns[i].setBounds (270 + i * 86, 276, 80, 28);
    mixSlider.setBounds (270, 336, 110, 84);
    trimSlider.setBounds (400, 336, 110, 84);

    meterArea = { 20, 428, 330, 24 };
    advancedToggle.setBounds (370, 426, 150, 28);

    // gelişmiş panel
    juce::Slider* advSliders[] = { &thresholdSlider, &ratioSlider, &attackSlider, &releaseSlider,
                                   &kneeSlider,      &hpfSlider,   &makeupSlider };
    for (int i = 0; i < 7; ++i)
        advSliders[i]->setBounds (14 + i * 73, kHeightBasic + 40, 72, 84);
    autoMakeupToggle.setBounds (20, kHeightBasic + 128, 500, 24);
    progRelToggle.setBounds (20, kHeightBasic + 154, 500, 24);
}
