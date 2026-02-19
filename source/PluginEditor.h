#pragma once
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor (PluginProcessor&);
    ~PluginEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processorRef;
    juce::ToggleButton voxButton { "VOX" }, spaceButton { "SPACE" }, punchButton { "PUNCH" };
    juce::ToggleButton flipButton { "FLIP" }, shredButton { "SHRED" }, chopButton { "CHOP" };

    void updateButtonStates (juce::ToggleButton* activeBtn, bool isEngine);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};