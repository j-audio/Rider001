#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    double currentSampleRate { 44100.0 }; 
    float currentFaderGain { 1.0f }; // Our new continuous analog fader

    // Audio level tracking for UI
    std::atomic<float> mainBusLevel { 0.0f };
    std::atomic<float> sidechainBusLevel { 0.0f };
    std::atomic<float> currentGainDb { 0.0f };

    // ==========================================================
    // SMART PEAK TRACKER MEMORY
    // ==========================================================
    float currentMacroPeak { 0.0001f };
    float macroPeakRelease { 0.99f }; 

public:
    float getMainBusLevel() const { return mainBusLevel.load(); }
    float getSidechainBusLevel() const { return sidechainBusLevel.load(); }
    float getCurrentGainDb() const { return currentGainDb.load(); }
    
    // ==========================================================
    // MODE & MODIFIER ENGINES
    // ==========================================================
    std::atomic<int> currentMode { 0 };     // 0=Base, 1=VOX, 2=SPACE, 3=PUNCH
    std::atomic<int> currentModifier { 0 }; // 0=Clean, 1=FLIP, 2=SHRED, 3=CHOP
    std::atomic<int> currentRatio { 1 };
    
    // The SHRED Trilogy
    std::atomic<int> currentShredMode { 1 }; // 1=Wavefolder, 2=TempoCrush, 3=BlackHole
    float heldSample[8] { 0.0f }; // Memory for the S&H algorithm (supports up to 8 channels safely)
    int holdCounter[8] { 0 };     // Counter for the S&H algorithm

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};