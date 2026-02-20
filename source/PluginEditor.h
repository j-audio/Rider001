#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    // ==========================================================
    // VINTAGE LED BUTTON DESIGN (RED - THE ENGINE)
    // ==========================================================
    class VintageButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
            auto bounds = button.getLocalBounds().toFloat();
            
            // 1. Draw the text
            g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xff888888));
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds.withTrimmedTop(16), juce::Justification::centred);
            
            // 2. Draw the LED
            float ledWidth = 8.0f;
            float ledHeight = 4.0f;
            juce::Rectangle<float> ledRect((bounds.getWidth() - ledWidth) / 2.0f, 6.0f, ledWidth, ledHeight);
            
            if (button.getToggleState()) {
                g.setColour(juce::Colours::red);
                g.fillRect(ledRect); 
                g.setColour(juce::Colours::red.withAlpha(0.4f));
                g.fillRect(ledRect.expanded(2.0f, 2.0f)); 
            } else {
                g.setColour(juce::Colour(0xff440000));
                g.fillRect(ledRect);
            }
        }
    };

    // ==========================================================
    // VINTAGE LED BUTTON DESIGN (PURPLE - THE MODIFIERS)
    // ==========================================================
    class VintagePurpleButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override {
            auto bounds = button.getLocalBounds().toFloat();
            g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xff888888));
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds.withTrimmedTop(16), juce::Justification::centred);
            
            float ledWidth = 8.0f;
            float ledHeight = 4.0f;
            juce::Rectangle<float> ledRect((bounds.getWidth() - ledWidth) / 2.0f, 6.0f, ledWidth, ledHeight);
            
            if (button.getToggleState()) {
                g.setColour(juce::Colour(0xffb026ff)); 
                g.fillRect(ledRect); 
                g.setColour(juce::Colour(0xffb026ff).withAlpha(0.4f));
                g.fillRect(ledRect.expanded(2.0f, 2.0f));
            } else {
                g.setColour(juce::Colour(0xff2a0044));
                g.fillRect(ledRect);
            }
        }
    };

    VintageButtonLookAndFeel vintageLookAndFeel;
    VintagePurpleButtonLookAndFeel purpleLookAndFeel;

    // Bank 1: The Engine (Red LEDs)
    juce::ToggleButton voxButton   { "VOX" };
    juce::ToggleButton spaceButton { "SPACE" };
    juce::ToggleButton punchButton { "PUNCH" }; // Renamed from emptyButton

    // Bank 2: The Modifiers (Purple LEDs)
    juce::ToggleButton flipButton  { "FLIP" };
    juce::ToggleButton shredButton { "SHRED" };
    juce::ToggleButton chopButton  { "CHOP" };

    // Meter bounds
    juce::Rectangle<int> analyzedMeter;
    juce::Rectangle<int> actionMeter;
    juce::Rectangle<int> outputMeter;

    bool sidechainActive { false };
    bool peakActive      { false };
    bool actionPeak      { false };

    float smoothedAnalyzed { -90.0f };
    float smoothedOutput   { -90.0f };
    float smoothedAction   {   0.0f };

    void drawVintageMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float levelDb);
    void drawActionMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb);
    void drawMeterArc(juce::Graphics& g, juce::Point<float> arcCenter, float arcRadius, juce::Rectangle<float> meterArea);
    void drawPeakLED(juce::Graphics& g, float x, float y);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};