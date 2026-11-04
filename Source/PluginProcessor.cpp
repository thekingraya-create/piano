#include "PluginProcessor.h"
#include "PluginEditor.h"

PianoProcessor::PianoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    params.tone   = apvts.getRawParameterValue ("tone");
    params.decay  = apvts.getRawParameterValue ("decay");
    params.hammer = apvts.getRawParameterValue ("hammer");
    params.width  = apvts.getRawParameterValue ("width");
    volumeParam   = apvts.getRawParameterValue ("volume");
    reverbParam   = apvts.getRawParameterValue ("reverb");

    for (int i = 0; i < 24; ++i)
        synth.addVoice (new PianoVoice (params));

    synth.addSound (new PianoSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout PianoProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "volume", 1 }, "Volume",
        NormalisableRange<float> (-30.0f, 6.0f, 0.1f), -6.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "decay", 1 }, "Decay",
        NormalisableRange<float> (0.3f, 2.0f, 0.01f), 1.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "hammer", 1 }, "Hammer",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "width", 1 }, "Width",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.6f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "reverb", 1 }, "Reverb",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));

    return layout;
}

void PianoProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    volumeSmooth.reset (sampleRate, 0.02);
    volumeSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (volumeParam->load()));
}

bool PianoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PianoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    synth.renderNextBlock (buffer, midi, 0, n);

    if (buffer.getNumChannels() < 2)
        return;

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    const float mix = reverbParam->load();
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.55f;
    rp.damping    = 0.5f;
    rp.width      = 1.0f;
    rp.wetLevel   = mix * 0.2f;
    rp.dryLevel   = 0.5f * (1.0f - 0.3f * mix);
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverb.processStereo (l, r, n);

    volumeSmooth.setTargetValue (juce::Decibels::decibelsToGain (volumeParam->load()));

    for (int i = 0; i < n; ++i)
    {
        const float g = volumeSmooth.getNextValue();
        l[i] = std::tanh (l[i] * g);   // soft safety limiter
        r[i] = std::tanh (r[i] * g);
    }
}

juce::AudioProcessorEditor* PianoProcessor::createEditor()
{
    return new PianoEditor (*this);
}

void PianoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PianoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PianoProcessor();
}
