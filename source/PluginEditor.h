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

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    // ==========================================================
    // VINTAGE LED BUTTON DESIGN (RED - ENGINE)
    // ==========================================================
    class VintageButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool, bool) override {
            auto bounds = button.getLocalBounds().toFloat();
            g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xff888888));
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds.withTrimmedTop(16), juce::Justification::centred);
            
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
                              bool, bool) override {
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

    // ==========================================================
    // 1176 TRANSLUCENT RATIO BUTTONS (GREY/ORANGE)
    // ==========================================================
    class Vintage1176RatioButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool, bool) override {
            auto bounds = button.getLocalBounds().toFloat();
            
            // Translucent grey plastic body
            g.setColour(juce::Colour(0xff363636));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
            g.setColour(juce::Colour(0xff555555));
            g.drawRoundedRectangle(bounds.reduced(2.0f), 2.0f, 1.0f);

            if (button.getToggleState()) {
                // Incandescent Orange Core Glow
                juce::ColourGradient glow(juce::Colour(0xffff6a00).withAlpha(0.8f), bounds.getCentreX(), bounds.getCentreY(),
                                          juce::Colour(0xffffaa00).withAlpha(0.0f), bounds.getCentreX(), bounds.getCentreY() + 15.0f, true);
                g.setGradientFill(glow);
                g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
                g.setColour(juce::Colours::white);
            } else {
                g.setColour(juce::Colour(0xff888888));
            }
            
            g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds, juce::Justification::centred);
        }
    };

    // ==========================================================
    // DENON CHUNKY ROCKER SWITCHES (BLACK/RED)
    // ==========================================================
    class ChunkyRockerSwitchLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool, bool) override {
            auto bounds = button.getLocalBounds().toFloat();
            bool isRed = (button.getName() == "B"); // B is the Red Switch
            bool isDown = button.getToggleState();
            
            juce::Colour baseColor = isRed ? juce::Colour(0xff990000) : juce::Colour(0xff1a1a1a);
            juce::Colour highlight = baseColor.brighter(0.3f);
            juce::Colour shadow = baseColor.darker(0.8f);
            
            // Recessed Bezel
            g.setColour(juce::Colour(0xff050505));
            g.fillRect(bounds);
            
            auto switchRect = bounds.reduced(3.0f, 6.0f);
            
            // 3D Rocker Gradient
            juce::ColourGradient grad;
            if (isDown) { // Pressed (Bottom half catches light)
                grad = juce::ColourGradient(shadow, switchRect.getX(), switchRect.getY(),
                                            highlight, switchRect.getX(), switchRect.getBottom(), false);
            } else { // Unpressed (Top half catches light)
                grad = juce::ColourGradient(highlight, switchRect.getX(), switchRect.getY(),
                                            shadow, switchRect.getX(), switchRect.getBottom(), false);
            }
            g.setGradientFill(grad);
            g.fillRect(switchRect);
            
            // Center tactile ridge
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawLine(switchRect.getX(), switchRect.getCentreY(), switchRect.getRight(), switchRect.getCentreY(), 2.0f);

            // Draw label above switch
            g.setColour(juce::Colour(0xffaaaaaa));
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds.getX(), bounds.getY() - 14.0f, bounds.getWidth(), 14.0f, juce::Justification::centred);
        }
    };

    // ==========================================================
    // MINI VINTAGE DESIGN (For the Shred Sub-Menu)
    // ==========================================================
    class MiniVintagePurpleButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool, bool) override {
            auto bounds = button.getLocalBounds().toFloat();
            g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xff888888));
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds, juce::Justification::centred);
            
            if (button.getToggleState()) {
                g.setColour(juce::Colour(0xffb026ff));
                g.fillRect(bounds.getWidth()/2.0f - 4.0f, bounds.getHeight() - 2.0f, 8.0f, 2.0f);
            }
        }
    };

    VintageButtonLookAndFeel vintageLookAndFeel;
    VintagePurpleButtonLookAndFeel purpleLookAndFeel;
    Vintage1176RatioButtonLookAndFeel ratioLookAndFeel;
    ChunkyRockerSwitchLookAndFeel rockerLookAndFeel;
    MiniVintagePurpleButtonLookAndFeel miniPurpleLookAndFeel;

    // Bank 1: The Engine (Red LEDs)
    juce::ToggleButton voxButton   { "VOX" };
    juce::ToggleButton spaceButton { "SPACE" };
    juce::ToggleButton punchButton { "PUNCH" };

    // Bank 2: The Modifiers (Purple LEDs)
    juce::ToggleButton flipButton  { "FLIP" };
    juce::ToggleButton shredButton { "SHRED" };
    juce::ToggleButton chopButton  { "CHOP" };

    // The Shred Sub-Menu Buttons
    juce::ToggleButton shredMode1 { "I" };
    juce::ToggleButton shredMode2 { "II" };
    juce::ToggleButton shredMode3 { "III" };

    // Bank 3: 1176 Ratio Panel (Horizontal)
    juce::ToggleButton ratio1Button { "1:1" };
    juce::ToggleButton ratio3Button { "3:1" };
    juce::ToggleButton ratio6Button { "6:1" };
    juce::ToggleButton ratio9Button { "9:1" };

    // Bank 4: Denon Chunky Switches
    juce::ToggleButton chunkyA { "A" }; // Black
    juce::ToggleButton chunkyB { "B" }; // Red

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