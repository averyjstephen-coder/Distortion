#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout RockIconsProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto pid = [] (const char* id) { return juce::ParameterID { id, 1 }; };

    juce::StringArray names;
    for (int i = 0; i < rid::kNumModels; ++i)
        names.add (rid::kModels[i].name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (pid ("model"), "Model", names, 1));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("drive"), "Drive", juce::NormalisableRange<float> (0.f, 1.f, 0.001f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("tone"), "Tone", juce::NormalisableRange<float> (-1.f, 1.f, 0.01f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("level"), "Level", juce::NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        pid ("mix"), "Mix", juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 1.f));
    layout.add (std::make_unique<juce::AudioParameterBool> (pid ("cab"), "Cab Sim", true));
    return layout;
}

RockIconsProcessor::RockIconsProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    modelParam = apvts.getRawParameterValue ("model");
    driveParam = apvts.getRawParameterValue ("drive");
    toneParam = apvts.getRawParameterValue ("tone");
    levelParam = apvts.getRawParameterValue ("level");
    mixParam = apvts.getRawParameterValue ("mix");
    cabParam = apvts.getRawParameterValue ("cab");
}

bool RockIconsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void RockIconsProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    numChannels = juce::jlimit (1, 2, getTotalNumInputChannels());
    maxBlock = juce::jmax (1, samplesPerBlock);

    // 4x oversampling (2^2) keeps the clipping stages from aliasing.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, (size_t) 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();

    for (auto& e : engines)
        e.prepare (sr * 4.0);

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, (juce::uint32) numChannels };
    mixer.prepare (spec);
    const auto latency = oversampling->getLatencyInSamples();
    mixer.setWetLatency (latency);
    setLatencySamples ((int) std::lround (latency));

    level.reset (sr, 0.03);
    level.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (levelParam->load()));
}

void RockIconsProcessor::processChunk (juce::dsp::AudioBlock<float>& block)
{
    const int nCh = (int) block.getNumChannels();
    const int model = (int) std::lround (modelParam->load());
    const float drive = driveParam->load();
    const float tone = toneParam->load();
    const bool cab = cabParam->load() > 0.5f;

    for (int ch = 0; ch < nCh; ++ch)
        engines[(size_t) ch].configure (model, drive, tone, cab);

    mixer.setWetMixProportion (mixParam->load());
    mixer.pushDrySamples (block);

    auto up = oversampling->processSamplesUp (block);
    for (int ch = 0; ch < nCh; ++ch)
    {
        float* d = up.getChannelPointer ((size_t) ch);
        auto& e = engines[(size_t) ch];
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            d[i] = e.process (d[i]);
    }
    oversampling->processSamplesDown (block);

    level.setTargetValue (juce::Decibels::decibelsToGain (levelParam->load()));
    for (size_t i = 0; i < block.getNumSamples(); ++i)
    {
        const float g = level.getNextValue();
        for (int ch = 0; ch < nCh; ++ch)
            block.getChannelPointer ((size_t) ch)[i] *= g;
    }

    mixer.mixWetSamples (block);
}

void RockIconsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (buffer.getNumChannels(), numChannels);
    if (oversampling == nullptr || nCh < 1)
        return;

    juce::dsp::AudioBlock<float> whole (buffer);
    auto channels = whole.getSubsetChannelBlock (0, (size_t) nCh);

    // Hosts may send blocks bigger than the size we prepared for, so work in chunks.
    for (size_t start = 0; start < channels.getNumSamples(); start += (size_t) maxBlock)
    {
        const auto len = juce::jmin ((size_t) maxBlock, channels.getNumSamples() - start);
        auto chunk = channels.getSubBlock (start, len);
        processChunk (chunk);
    }
}

void RockIconsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void RockIconsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RockIconsProcessor::createEditor()
{
    return new RockIconsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RockIconsProcessor();
}
