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
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    // Meter bounds for drawing (160x110 pixels each)
    juce::Rectangle<int> analyzedMeter;
    juce::Rectangle<int> actionMeter;
    juce::Rectangle<int> outputMeter;

    // Sidechain LED indicator state
    bool sidechainActive { false };
    bool peakActive      { false };
    bool actionPeak      { false };

    // Smoothed meter values (dB domain)
    float smoothedAnalyzed { -90.0f };
    float smoothedOutput   { -90.0f };
    float smoothedAction   {   0.0f };

    // Helper methods for drawing vintage VU meters
    void drawVintageMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float levelDb);
    void drawActionMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb);
    void drawMeterArc(juce::Graphics& g, juce::Point<float> arcCenter, float arcRadius, juce::Rectangle<float> meterArea);
    void drawPeakLED(juce::Graphics& g, float x, float y);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
