#pragma once

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

#include "JackCore.h"

// Jack Compressor: ses iş parçacığı tarafı.
// DSP ve öneri mantığı JackCore.h içindedir; bu sınıf yalnızca JUCE'a bağlar.
class JackAudioProcessor : public juce::AudioProcessor
{
public:
    JackAudioProcessor();
    ~JackAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Jack Compressor"; }
    bool acceptsMidi() const override { return false; }
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

    // ---- arayüz iş parçacığı için güvenli okumalar
    jack::Params readParams() const;
    jack::Stats readStats() const;
    float getGrDb() const { return grNow.load(); }
    bool signalPresent() const { return inHoldDb.load() > -60.0f; }
    float getInDb() const { return inHoldDb.load(); }

    // arayüz dili (varsayılan İngilizce); son seçim bu bilgisayarda yeni örnekler için hatırlanır
    jack::Lang getLanguage() const;
    void setLanguage (jack::Lang l);

    // Jack'in önerisini gerçek parametrelere yazar (host otomasyonuna da bildirir)
    void applyPatchToParams (const jack::Patch& patch);
    void setParamReal (const char* id, float realValue);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    jack::Compressor comp;
    double sampleRateHz = 44100.0;

    std::atomic<float>* pSource = nullptr;
    std::atomic<float>* pAmount = nullptr;
    std::atomic<float>* pFeel = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pAdvanced = nullptr;
    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pKnee = nullptr;
    std::atomic<float>* pHpf = nullptr;
    std::atomic<float>* pMakeup = nullptr;
    std::atomic<float>* pAutoMakeup = nullptr;
    std::atomic<float>* pProgRel = nullptr;

    // ses iş parçacığından arayüze (kilitsiz)
    std::atomic<float> grNow { 0.0f };
    std::atomic<float> inHoldDb { -120.0f };
    std::atomic<bool> stValid { false };
    std::atomic<float> stInPeak { -120.0f }, stOutPeak { -120.0f }, stAvgGr { 0.0f }, stMaxGr { 0.0f },
        stActive { 0.0f }, stStuck { 0.0f }, stLoudDiff { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JackAudioProcessor)
};
