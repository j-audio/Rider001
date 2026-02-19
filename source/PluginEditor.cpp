#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    auto setup = [this](juce::ToggleButton& b, juce::Colour tick, juce::Colour text) {
        b.setColour (juce::ToggleButton::tickColourId, tick);
        b.setColour (juce::ToggleButton::textColourId, text);
        addAndMakeVisible (b);
    };

    setup(voxButton, juce::Colours::red, juce::Colours::white);
    setup(spaceButton, juce::Colours::red, juce::Colours::white);
    setup(punchButton, juce::Colours::red, juce::Colours::white);
    setup(flipButton, juce::Colours::purple, juce::Colours::white);
    setup(shredButton, juce::Colours::purple, juce::Colours::white);
    setup(chopButton, juce::Colours::purple, juce::Colours::white);

    voxButton.onClick = [this] { updateButtonStates(&voxButton, true); };
    spaceButton.onClick = [this] { updateButtonStates(&spaceButton, true); };
    punchButton.onClick = [this] { updateButtonStates(&punchButton, true); };
    flipButton.onClick = [this] { updateButtonStates(&flipButton, false); };
    shredButton.onClick = [this] { updateButtonStates(&shredButton, false); };
    chopButton.onClick = [this] { updateButtonStates(&chopButton, false); };

    setSize (400, 300);
}

PluginEditor::~PluginEditor() {}

void PluginEditor::updateButtonStates (juce::ToggleButton* activeBtn, bool isEngine)
{
    if (isEngine) {
        if (activeBtn == &voxButton) { spaceButton.setToggleState(false, juce::dontSendNotification); punchButton.setToggleState(false, juce::dontSendNotification); processorRef.currentMode.store(voxButton.getToggleState() ? 1 : 0); }
        else if (activeBtn == &spaceButton) { voxButton.setToggleState(false, juce::dontSendNotification); punchButton.setToggleState(false, juce::dontSendNotification); processorRef.currentMode.store(spaceButton.getToggleState() ? 2 : 0); }
        else if (activeBtn == &punchButton) { voxButton.setToggleState(false, juce::dontSendNotification); spaceButton.setToggleState(false, juce::dontSendNotification); processorRef.currentMode.store(punchButton.getToggleState() ? 3 : 0); }
    } else {
        if (activeBtn == &flipButton) { shredButton.setToggleState(false, juce::dontSendNotification); chopButton.setToggleState(false, juce::dontSendNotification); processorRef.currentModifier.store(flipButton.getToggleState() ? 1 : 0); }
        else if (activeBtn == &shredButton) { flipButton.setToggleState(false, juce::dontSendNotification); chopButton.setToggleState(false, juce::dontSendNotification); processorRef.currentModifier.store(shredButton.getToggleState() ? 2 : 0); }
        else if (activeBtn == &chopButton) { flipButton.setToggleState(false, juce::dontSendNotification); shredButton.setToggleState(false, juce::dontSendNotification); processorRef.currentModifier.store(chopButton.getToggleState() ? 3 : 0); }
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawText ("J-RIDER", getLocalBounds().removeFromTop(60), juce::Justification::centred);
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced(20);
    r.removeFromTop(60);
    auto row1 = r.removeFromTop(60);
    voxButton.setBounds(row1.removeFromLeft(r.getWidth()/3));
    spaceButton.setBounds(row1.removeFromLeft(r.getWidth()/3));
    punchButton.setBounds(row1);
    
    r.removeFromTop(20);
    auto row2 = r.removeFromTop(60);
    flipButton.setBounds(row2.removeFromLeft(r.getWidth()/3));
    shredButton.setBounds(row2.removeFromLeft(r.getWidth()/3));
    chopButton.setBounds(row2);
}