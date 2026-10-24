#pragma once

#include <map>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// Jack'in yüzü: gövdesi gain reduction ile gerçekten sıkışır, ifadesi ruh haline göre değişir.
class JackFace : public juce::Component
{
public:
    // level: giriş seviyesi 0..1 (kollar ve ağız sese tepki verir), timeMs: animasyon saati
    void setState (float gr, jack::Mood m, float lvl, double nowMs)
    {
        grDb = gr;
        mood = m;
        level = lvl;
        timeMs = nowMs;
        repaint();
    }
    void paint (juce::Graphics& g) override;

private:
    struct Arm { float th = 0.2f, bend = 0.1f; };  // th: omuz açısı (0 aşağı, pi yukarı), bend: dirsek
    void drawArm (juce::Graphics& g, int side, juce::Rectangle<float> body, float scale);

    float grDb = 0.0f;
    float level = 0.0f;
    double timeMs = 0.0;
    Arm arms[2];
    jack::Mood mood = jack::Mood::Sleepy;
};

class JackAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JackAudioProcessorEditor (JackAudioProcessor&);
    ~JackAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kWidth = 540;
    static constexpr int kHeightBasic = 478;
    static constexpr int kHeightAdvanced = 664;

    void timerCallback() override;
    void refreshAdvice (const jack::Params& p, juce::int64 now);
    juce::String bubbleText (juce::int64 now) const;
    void setFeedback (const juce::String& text, int ms);
    void setupKnob (juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& suffix, int decimals);
    void styleChoiceButton (juce::TextButton& b);
    void updateAdvancedVisibility();

    JackAudioProcessor& proc;

    JackFace face;

    juce::TextButton sourceBtns[static_cast<int> (jack::Source::Count)];
    juce::TextButton feelBtns[3];
    juce::Label feelLabel;
    juce::TextButton applyBtn, noBtn;
    juce::ToggleButton advancedToggle, autoMakeupToggle, progRelToggle;

    juce::Slider amountSlider, mixSlider, trimSlider;
    juce::Slider thresholdSlider, ratioSlider, attackSlider, releaseSlider, kneeSlider, hpfSlider, makeupSlider;
    juce::Label amountLabel, mixLabel, trimLabel;
    juce::Label thresholdLabel, ratioLabel, attackLabel, releaseLabel, kneeLabel, hpfLabel, makeupLabel;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> amountAtt, mixAtt, trimAtt, thresholdAtt, ratioAtt, attackAtt, releaseAtt, kneeAtt, hpfAtt,
        makeupAtt;
    std::unique_ptr<ButtonAtt> advancedAtt, autoMakeupAtt, progRelAtt;

    juce::Rectangle<int> bubbleArea, meterArea;

    // Jack'in durumu
    std::optional<jack::Advice> current;
    std::map<int, juce::int64> mutedUntil;
    bool allGood = false;
    bool advancedShown = false;
    jack::Mood mood = jack::Mood::Sleepy;
    float grDisp = 0.0f;
    int frame = 0;
    juce::String feedbackText, sourceText;
    juce::int64 feedbackUntil = 0, sourceUntil = 0, greetUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JackAudioProcessorEditor)
};
