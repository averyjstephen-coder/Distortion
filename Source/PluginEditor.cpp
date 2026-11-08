#include "PluginEditor.h"

RockIconsEditor::RockIconsEditor (RockIconsProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (560, 330);

    for (int i = 0; i < rid::kNumModels; ++i)
        modelBox.addItem (rid::kModels[i].name, i + 1);
    addAndMakeVisible (modelBox);
    modelAttachment = std::make_unique<ComboAttachment> (proc.apvts, "model", modelBox);

    note.setJustificationType (juce::Justification::centredLeft);
    note.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (note);
    modelBox.onChange = [this] { updateNote(); };
    updateNote();

    addKnob (drive, "drive", "Drive", "");
    addKnob (tone, "tone", "Tone", "");
    addKnob (level, "level", "Level", " dB");
    addKnob (mix, "mix", "Mix", "");

    addAndMakeVisible (cabButton);
    cabAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "cab", cabButton);
}

void RockIconsEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& text,
                               const juce::String& suffix)
{
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (k.label);

    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    k.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (k.slider);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramId, k.slider);
}

void RockIconsEditor::updateNote()
{
    const int idx = juce::jlimit (0, rid::kNumModels - 1, modelBox.getSelectedItemIndex());
    note.setText (rid::kModels[idx].note, juce::dontSendNotification);
}

void RockIconsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1410));
    g.setColour (juce::Colour (0xffe8b04a));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("Rock Icons Drive", 20, 12, 320, 32, juce::Justification::centredLeft);
    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Voicings inspired by classic rock tones. Approximations, not circuit models.",
                20, 44, 520, 16, juce::Justification::centredLeft);
}

void RockIconsEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (60);

    modelBox.setBounds (area.removeFromTop (30));
    area.removeFromTop (4);
    note.setBounds (area.removeFromTop (36));
    area.removeFromTop (8);

    auto cabRow = area.removeFromBottom (28);
    cabButton.setBounds (cabRow.removeFromLeft (140));

    auto knobs = area;
    const int w = knobs.getWidth() / 4;
    for (Knob* k : { &drive, &tone, &level, &mix })
    {
        auto cell = knobs.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (20));
        k->slider.setBounds (cell.reduced (6));
    }
}
