#include "PluginEditor.h"

PianoEditor::PianoEditor (PianoProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    static const char* ids[]   = { "volume", "tone", "decay", "hammer", "width", "reverb" };
    static const char* names[] = { "Volume", "Tone", "Decay", "Hammer", "Width", "Reverb" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        if (i == 0)
            k.slider.setTextValueSuffix (" dB");

        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);

        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);

        k.attach = std::make_unique<SliderAttach> (proc.apvts, ids[i], k.slider);
    }

    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (14.0f);
    addAndMakeVisible (keyboard);

    setSize (540, 330);
}

void PianoEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1f24));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("SIMPLE PIANO", getLocalBounds().removeFromTop (40), juce::Justification::centred);
}

void PianoEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (30);

    keyboard.setBounds (area.removeFromBottom (80));
    area.removeFromBottom (10);

    const int colW = area.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (colW);
        k.label.setBounds (col.removeFromTop (20));
        k.slider.setBounds (col);
    }
}
