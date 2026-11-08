#pragma once

#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

class RockIconsEditor : public juce::AudioProcessorEditor
{
public:
    explicit RockIconsEditor (RockIconsProcessor&);
    ~RockIconsEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Attachment is declared last so it is destroyed before the slider it points at.
    struct Knob
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId, const juce::String& text, const juce::String& suffix);
    void updateNote();

    RockIconsProcessor& proc;

    juce::ComboBox modelBox;
    std::unique_ptr<ComboAttachment> modelAttachment;
    juce::Label note;

    Knob drive, tone, level, mix;
    juce::ToggleButton cabButton { "Cab sim" };
    std::unique_ptr<ButtonAttachment> cabAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RockIconsEditor)
};
