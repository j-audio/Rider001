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
    juce::LinearSmoothedValue<float> smoothedGain;

    // Audio level tracking for UI (atomic for thread-safety)
    std::atomic<float> mainBusLevel { 0.0f };
    std::atomic<float> sidechainBusLevel { 0.0f };
    std::atomic<float> currentGainDb { 0.0f };

public:
    // Getters for UI to access bus levels
    float getMainBusLevel() const { return mainBusLevel.load(); }
    float getSidechainBusLevel() const { return sidechainBusLevel.load(); }
    float getCurrentGainDb() const { return currentGainDb.load(); }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
