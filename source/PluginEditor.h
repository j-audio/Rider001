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
    // CHOP THRESHOLD KNOB LOOK & FEEL
    // ==========================================================
    class ChopKnobLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                              juce::Slider&) override {
            auto radius = (float)juce::jmin(width / 2, height / 2) - 2.0f;
            auto centreX = (float)x + (float)width * 0.5f;
            auto centreY = (float)y + (float)height * 0.5f;
            auto rx = centreX - radius;
            auto ry = centreY - radius;
            auto rw = radius * 2.0f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            g.setColour(juce::Colour(0xff1a1a1a));
            g.fillEllipse(rx, ry, rw, rw);
            g.setColour(juce::Colour(0xffb026ff).withAlpha(0.5f));
            g.drawEllipse(rx, ry, rw, rw, 1.0f);

            juce::Path p;
            auto pointerLength = radius * 0.8f;
            auto pointerThickness = 2.0f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

            g.setColour(juce::Colour(0xffb026ff));
            g.fillPath(p);
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
            
            g.setColour(juce::Colour(0xff363636));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
            g.setColour(juce::Colour(0xff555555));
            g.drawRoundedRectangle(bounds.reduced(2.0f), 2.0f, 1.0f);

            if (button.getToggleState()) {
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
            bool isRed = (button.getName() == "B"); 
            bool isDown = button.getToggleState();
            
            juce::Colour baseColor = isRed ? juce::Colour(0xff990000) : juce::Colour(0xff1a1a1a);
            juce::Colour highlight = baseColor.brighter(0.3f);
            juce::Colour shadow = baseColor.darker(0.8f);
            
            g.setColour(juce::Colour(0xff050505));
            g.fillRect(bounds);
            
            auto switchRect = bounds.reduced(3.0f, 6.0f);
            
            juce::ColourGradient grad;
            if (isDown) { 
                grad = juce::ColourGradient(shadow, switchRect.getX(), switchRect.getY(),
                                            highlight, switchRect.getX(), switchRect.getBottom(), false);
            } else { 
                grad = juce::ColourGradient(highlight, switchRect.getX(), switchRect.getY(),
                                            shadow, switchRect.getX(), switchRect.getBottom(), false);
            }
            g.setGradientFill(grad);
            g.fillRect(switchRect);
            
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.drawLine(switchRect.getX(), switchRect.getCentreY(), switchRect.getRight(), switchRect.getCentreY(), 2.0f);

            g.setColour(juce::Colour(0xffaaaaaa));
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(button.getName(), 
                       static_cast<int>(bounds.getX()), 
                       static_cast<int>(bounds.getY() - 14.0f), 
                       static_cast<int>(bounds.getWidth()), 
                       14, 
                       juce::Justification::centred);
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

    // ==========================================================
    // SOURCE SELECTOR BUTTONS (IN / EXT)
    // ==========================================================
    class SourceButtonLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool, bool) override {
            auto bounds = button.getLocalBounds().toFloat();
            
            // Draw background
            g.setColour(button.getToggleState() ? juce::Colour(0xff555555) : juce::Colour(0xff1a1a1a));
            g.fillRoundedRectangle(bounds, 3.0f);
            g.setColour(juce::Colour(0xff333333));
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

            // Draw text
            g.setColour(button.getToggleState() ? juce::Colours::white : juce::Colour(0xff888888));
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(button.getName(), bounds, juce::Justification::centred);
        }
    };

    VintageButtonLookAndFeel vintageLookAndFeel;
    VintagePurpleButtonLookAndFeel purpleLookAndFeel;
    Vintage1176RatioButtonLookAndFeel ratioLookAndFeel;
    ChunkyRockerSwitchLookAndFeel rockerLookAndFeel;
    MiniVintagePurpleButtonLookAndFeel miniPurpleLookAndFeel;
    ChopKnobLookAndFeel chopKnobLookAndFeel;
    SourceButtonLookAndFeel sourceButtonLookAndFeel;

    juce::ToggleButton voxButton   { "VOX" };
    juce::ToggleButton spaceButton { "SPACE" };
    juce::ToggleButton punchButton { "PUNCH" };

    juce::ToggleButton flipButton  { "FLIP" };
    juce::ToggleButton shredButton { "SHRED" };
    juce::ToggleButton chopButton  { "CHOP" };

    juce::ToggleButton shredMode1 { "I" };
    juce::ToggleButton shredMode2 { "II" };
    juce::ToggleButton shredMode3 { "III" };
    
    juce::Slider chopSlider;

    juce::ToggleButton ratio1Button { "1:1" };
    juce::ToggleButton ratio3Button { "3:1" };
    juce::ToggleButton ratio6Button { "6:1" };
    juce::ToggleButton ratio9Button { "9:1" };

    juce::ToggleButton chunkyA { "A" }; 
    juce::ToggleButton chunkyB { "B" }; 
    juce::ComboBox ghostSelector;       
    juce::TextButton saveGhostButton { "SAVE" }; 

    juce::ToggleButton sourceInButton  { "IN" };
    juce::ToggleButton sourceExtButton { "EXT" };

    juce::Rectangle<int> analyzedMeter;
    juce::Rectangle<int> actionMeter;
    juce::Rectangle<int> outputMeter;

    bool sidechainActive { false };
    bool peakActive      { false };
    bool actionPeak      { false };

    float smoothedAnalyzed { -90.0f };
    float smoothedOutput   { -90.0f };
    float smoothedActionL  {   0.0f };
    float smoothedActionR  {   0.0f };

    int currentGhostLedState { 0 }; 
    bool blinkState { false };      

    void drawVintageMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float levelDb);
    void drawActionMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDbL, float gainDbR);
    void drawMeterArc(juce::Graphics& g, juce::Point<float> arcCenter, float arcRadius, juce::Rectangle<float> meterArea);
    void drawPeakLED(juce::Graphics& g, float x, float y);
    void drawGhostLED(juce::Graphics& g, juce::Rectangle<int> switchBounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};