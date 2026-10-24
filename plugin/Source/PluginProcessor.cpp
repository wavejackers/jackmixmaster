#include "PluginProcessor.h"

#include "PluginEditor.h"

using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterFloat;
using juce::NormalisableRange;
using juce::ParameterID;

namespace
{
juce::PropertiesFile::Options langFileOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "JackCompressor";
    o.folderName = "Wavejackers";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

int savedDefaultLang()
{
    juce::PropertiesFile pf (langFileOptions());
    return juce::jlimit (0, static_cast<int> (jack::Lang::Count) - 1, pf.getIntValue ("lang", 0));
}
} // namespace

jack::Lang JackAudioProcessor::getLanguage() const
{
    const int v = apvts.state.getProperty ("lang", savedDefaultLang());
    return static_cast<jack::Lang> (juce::jlimit (0, static_cast<int> (jack::Lang::Count) - 1, v));
}

void JackAudioProcessor::setLanguage (jack::Lang l)
{
    apvts.state.setProperty ("lang", static_cast<int> (l), nullptr);
    juce::PropertiesFile pf (langFileOptions());
    pf.setValue ("lang", static_cast<int> (l));
    pf.saveIfNeeded();
}

juce::AudioProcessorValueTreeState::ParameterLayout JackAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    juce::StringArray sources;
    for (int i = 0; i < static_cast<int> (jack::Source::Count); ++i)
        sources.add (juce::String::fromUTF8 (jack::profile (static_cast<jack::Source> (i)).name));  // İngilizce: DAW'da sabit

    juce::StringArray feels;
    feels.add ("Soft");
    feels.add ("Balanced");
    feels.add ("Vurucu");

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 }, "Source", sources, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "amount", 1 }, "Squeeze", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "feel", 1 }, "Feel", feels, 1));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "trim", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "advanced", 1 }, "Advanced", false));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "threshold", 1 }, "Threshold", NormalisableRange<float> (-70.0f, 0.0f, 0.1f), -18.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "ratio", 1 }, "Ratio", NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.4f), 3.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "attack", 1 }, "Attack", NormalisableRange<float> (0.1f, 200.0f, 0.01f, 0.4f), 10.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "release", 1 }, "Release", NormalisableRange<float> (10.0f, 1500.0f, 0.1f, 0.4f), 120.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "knee", 1 }, "Knee", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "hpf", 1 }, "Sidechain HPF", NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.5f), 80.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "makeup", 1 }, "Makeup", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "automakeup", 1 }, "Auto Makeup", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "progrel", 1 }, "Program Release", true));

    return { p.begin(), p.end() };
}

JackAudioProcessor::JackAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "JACK", createLayout())
{
    pSource = apvts.getRawParameterValue ("source");
    pAmount = apvts.getRawParameterValue ("amount");
    pFeel = apvts.getRawParameterValue ("feel");
    pMix = apvts.getRawParameterValue ("mix");
    pTrim = apvts.getRawParameterValue ("trim");
    pAdvanced = apvts.getRawParameterValue ("advanced");
    pThreshold = apvts.getRawParameterValue ("threshold");
    pRatio = apvts.getRawParameterValue ("ratio");
    pAttack = apvts.getRawParameterValue ("attack");
    pRelease = apvts.getRawParameterValue ("release");
    pKnee = apvts.getRawParameterValue ("knee");
    pHpf = apvts.getRawParameterValue ("hpf");
    pMakeup = apvts.getRawParameterValue ("makeup");
    pAutoMakeup = apvts.getRawParameterValue ("automakeup");
    pProgRel = apvts.getRawParameterValue ("progrel");
}

jack::Params JackAudioProcessor::readParams() const
{
    jack::Params p;
    p.source = static_cast<jack::Source> (juce::jlimit (0, static_cast<int> (jack::Source::Count) - 1,
                                                        juce::roundToInt (pSource->load())));
    p.amount = pAmount->load();
    p.feel = static_cast<jack::Feel> (juce::jlimit (0, 2, juce::roundToInt (pFeel->load())));
    p.mixPercent = pMix->load();
    p.outTrimDb = pTrim->load();
    p.advanced = pAdvanced->load() > 0.5f;
    p.thresholdDb = pThreshold->load();
    p.ratio = pRatio->load();
    p.attackMs = pAttack->load();
    p.releaseMs = pRelease->load();
    p.kneeDb = pKnee->load();
    p.scHpfHz = pHpf->load();
    p.makeupDb = pMakeup->load();
    p.autoMakeup = pAutoMakeup->load() > 0.5f;
    p.programRelease = pProgRel->load() > 0.5f;
    return p;
}

jack::Stats JackAudioProcessor::readStats() const
{
    jack::Stats s;
    s.valid = stValid.load();
    s.inPeakDb = stInPeak.load();
    s.outPeakDb = stOutPeak.load();
    s.avgGrDb = stAvgGr.load();
    s.maxGrDb = stMaxGr.load();
    s.activeRatio = stActive.load();
    s.stuckRatio = stStuck.load();
    s.loudDiffDb = stLoudDiff.load();
    return s;
}

void JackAudioProcessor::setParamReal (const char* id, float realValue)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
        prm->endChangeGesture();
    }
}

void JackAudioProcessor::applyPatchToParams (const jack::Patch& patch)
{
    const auto& v = patch.values;
    const auto m = patch.mask;
    if (m & jack::kPAmount) setParamReal ("amount", v.amount);
    if (m & jack::kPMix) setParamReal ("mix", v.mixPercent);
    if (m & jack::kPOutTrim) setParamReal ("trim", v.outTrimDb);
    if (m & jack::kPThreshold) setParamReal ("threshold", v.thresholdDb);
    if (m & jack::kPRatio) setParamReal ("ratio", v.ratio);
    if (m & jack::kPAttack) setParamReal ("attack", v.attackMs);
    if (m & jack::kPRelease) setParamReal ("release", v.releaseMs);
    if (m & jack::kPKnee) setParamReal ("knee", v.kneeDb);
    if (m & jack::kPHpf) setParamReal ("hpf", v.scHpfHz);
    if (m & jack::kPAutoMakeup) setParamReal ("automakeup", v.autoMakeup ? 1.0f : 0.0f);
}

void JackAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    sampleRateHz = sampleRate;
    comp.prepare (sampleRate);
    comp.setParams (readParams());
    comp.reset();
}

bool JackAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void JackAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int i = totalIn; i < totalOut; ++i)
        buffer.clear (i, 0, n);

    // arayüzün "sinyal var mı" bilgisi (işlemeden önceki giriş tepesi)
    const float blockPk = buffer.getMagnitude (0, n);
    const float hold = juce::jmax (jack::linToDb (blockPk),
                                   inHoldDb.load() - 20.0f * static_cast<float> (n) / static_cast<float> (sampleRateHz));
    inHoldDb.store (hold);

    comp.setParams (readParams());
    comp.process (buffer.getArrayOfWritePointers(), juce::jmin (buffer.getNumChannels(), jack::Compressor::kMaxCh), n);

    grNow.store (comp.currentGrDb());
    const auto& s = comp.getStats();
    stValid.store (s.valid);
    stInPeak.store (s.inPeakDb);
    stOutPeak.store (s.outPeakDb);
    stAvgGr.store (s.avgGrDb);
    stMaxGr.store (s.maxGrDb);
    stActive.store (s.activeRatio);
    stStuck.store (s.stuckRatio);
    stLoudDiff.store (s.loudDiffDb);
}

juce::AudioProcessorEditor* JackAudioProcessor::createEditor()
{
    return new JackAudioProcessorEditor (*this);
}

void JackAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JackAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// JUCE'un plugin giriş noktası
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JackAudioProcessor();
}
