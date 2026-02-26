#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <vector>

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
    
    // ==========================================================
    // TRUE DUAL MONO MEMORY ARRAYS (0 = Left, 1 = Right)
    // ==========================================================
    float currentFaderGain[2] { 1.0f, 1.0f }; 
    float currentMacroPeak[2] { 0.0001f, 0.0001f }; // Used for the guide/target
    float liveMacroPeak[2]    { 0.0001f, 0.0001f }; // Dedicated tracker for the live input

    float macroPeakRelease { 0.99f }; 

    // Audio level tracking for UI
    std::atomic<float> mainBusLevel { 0.0f };
    std::atomic<float> sidechainBusLevel { 0.0f };
    std::atomic<float> currentGainDb { 0.0f };

public:
    float getMainBusLevel() const { return mainBusLevel.load(); }
    float getSidechainBusLevel() const { return sidechainBusLevel.load(); }
    float getCurrentGainDb() const { return currentGainDb.load(); }
    std::atomic<float> currentGhostTargetUI { 0.0f }; 
    
    // ==========================================================
    // MODE & MODIFIER ENGINES
    // ==========================================================
    std::atomic<int> currentMode { 0 };     // 0=Base, 1=VOX, 2=SPACE, 3=PUNCH
    // The Purple Modifiers (Now independent for chaining!)
    std::atomic<bool> isFlipActive { false };
    std::atomic<bool> isShredActive { false };
    std::atomic<bool> isChopActive { false };
    std::atomic<float> chopThreshold { 0.10f }; 
    std::atomic<int> currentRatio { 1 };
    
    // The SHRED Trilogy
    std::atomic<int> currentShredMode { 1 }; 
    float heldSample[8] { 0.0f };  
    int holdCounter[8] { 0 };   
    
    // ==========================================================
    // THE GHOST ENGINE MEMORY
    // ==========================================================
    std::atomic<bool> isGhostRecording { false }; 
    std::atomic<bool> isGhostReading { false };   

    std::vector<float> ghostMapL; 
    std::vector<float> ghostMapR;
    std::atomic<bool> forceExternalSidechain { false }; // False = IN, True = EXT
    
    // UI Feedback States
    std::atomic<int> ghostLedState { 0 }; 
    std::atomic<double> lastRecordedPPQ { 0.0 };
    std::atomic<int> lastRecordedIndex { -1 }; // ADD THIS LINE: Tracks array holes

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};