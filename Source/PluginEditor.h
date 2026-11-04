#pragma once
#include "PluginProcessor.h"

class PianoEditor : public juce::AudioProcessorEditor
{
public:
    explicit PianoEditor (PianoProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PianoProcessor& proc;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttach> attach;   // last, so it is destroyed first
    };

    std::array<Knob, 6> knobs;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoEditor)
};
