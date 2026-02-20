#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm> // Ensuring no compiler memory hiccups

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

PluginProcessor::~PluginProcessor() {}

const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }
bool PluginProcessor::acceptsMidi() const { return false; }
bool PluginProcessor::producesMidi() const { return false; }
bool PluginProcessor::isMidiEffect() const { return false; }
double PluginProcessor::getTailLengthSeconds() const { return 0.0; }
int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String PluginProcessor::getProgramName (int index) { return {}; }
void PluginProcessor::changeProgramName (int index, const juce::String& newName) {}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Reset the gain smoother
    smoothedGain.reset(currentSampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue(1.0f);

    // Calculate the release coefficient for the Macro Peak Tracker (approx 2 seconds)
    double blocksPerSecond = sampleRate / static_cast<double>(samplesPerBlock);
    macroPeakRelease = static_cast<float>(std::exp(-1.0 / (2.0 * blocksPerSecond)));
    currentMacroPeak = 0.0001f;
}

void PluginProcessor::releaseResources() {}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) return false;

    if (layouts.inputBuses.size() > 1) {
        auto sidechain = layouts.inputBuses.getReference(1);
        if (! sidechain.isDisabled() && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto mainBuffer = getBusBuffer(buffer, true, 0); 
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);

    float dryRMS   = sidechainBuffer.getRMSLevel(0, 0, sidechainBuffer.getNumSamples());
    float inputRMS = mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples());

    // Get Absolute Peaks (The Transients)
    float dryPeak = 0.0f;
    float inputPeak = 0.0f;
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch) {
        dryPeak = juce::jmax(dryPeak, sidechainBuffer.getMagnitude(ch, 0, sidechainBuffer.getNumSamples()));
        inputPeak = juce::jmax(inputPeak, mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples()));
    }

    sidechainBusLevel.store(dryRMS);

    // ==========================================================
    // 1. SMART MACRO PEAK TRACKER
    // ==========================================================
    if (dryRMS > currentMacroPeak) {
        currentMacroPeak = dryRMS; // Instant Attack
    } else {
        currentMacroPeak *= macroPeakRelease; // Slow 2-second Release
    }
    if (currentMacroPeak < 0.0001f) currentMacroPeak = 0.0001f;

    float gainFactor = 1.0f;
    int mode = currentMode.load();
    int mod = currentModifier.load(); // Fetch the purple modifier state

    if (inputRMS >= 0.00001f)
    {
        if (dryRMS < 0.00001f)
        {
            gainFactor = 0.0f;
        }
        else
        {
            float desiredTargetLevel = dryRMS; // Default is perfect match

            // ==========================================================
            // SPACE MODE: SMART HALF-RATE DECAY (UPWARD COMPRESSION)
            // ==========================================================
            if (mode == 2) 
            {
                float thresholdX = currentMacroPeak * 0.25f;  // Approx -12dB from peak
                float thresholdY = currentMacroPeak * 0.01f;  // Approx -40dB from peak

                if (dryRMS < thresholdX && dryRMS > thresholdY) {
                    desiredTargetLevel = thresholdX * std::sqrt(dryRMS / thresholdX);
                }
            }

            // Calculate the required gain to hit our new desired target
            float rawTargetGain = desiredTargetLevel / (inputRMS + 0.00001f);

            // ==========================================================
            // PUNCH MODE: UNIFIED CONTEXT-AWARE LOGIC
            // ==========================================================
            if (mode == 3)
            {
                float dryCrest = dryPeak / (dryRMS + 0.00001f);
                float inputCrest = inputPeak / (inputRMS + 0.00001f);
                
                // Inverse Loudness Compensation (The Decibel Paradox)
                float loudnessComp = 1.0f + (1.0f - juce::jmin(1.0f, dryPeak));

                // Case A: Restoration (VSTs crushed the punch)
                if (dryCrest > inputCrest + 0.05f) {
                    float restorationAmount = dryCrest / inputCrest;
                    rawTargetGain *= (restorationAmount * loudnessComp * 0.8f); 
                }
                // Case B: Live Studio Thickener (Raw signal clipping protector)
                else if (std::abs(dryCrest - inputCrest) <= 0.05f && dryCrest > 2.0f) {
                    float smartBoost = 1.0f + (0.15f * dryCrest * loudnessComp);
                    if (smartBoost > 3.0f) smartBoost = 3.0f; // Safety ceiling
                    rawTargetGain *= smartBoost;
                }
            }

            // The SPACE "Floor" Rule (Never attenuate)
            if (mode == 2 && rawTargetGain < 1.0f) {
                rawTargetGain = 1.0f; 
            }

            // ==========================================================
            // HYSTERESIS DEADBAND (Prevents shivering)
            // ==========================================================
            float currentTarget = smoothedGain.getTargetValue();
            float difference = std::abs(rawTargetGain - currentTarget);
            
            if (difference < 0.06f && (mode == 1 || mode == 2)) { 
                gainFactor = currentTarget; 
            } else {
                gainFactor = rawTargetGain; 
            }

            if (gainFactor > 32.0f) gainFactor = 32.0f; // Cap at +30dB boost
        }
    }

    // ==========================================================
    // MODE ENGINE: BALLISTICS (SPEED)
    // ==========================================================
    bool isAttack = (gainFactor < smoothedGain.getCurrentValue());
    float smoothingTime = 0.05f; 
    
    if (mode == 1) // VOX MODE
    {
        if (isAttack) smoothingTime = 0.015f; 
        else          smoothingTime = 0.030f; 
    }
    else if (mode == 2) // SPACE MODE
    {
        if (isAttack) smoothingTime = 0.050f; 
        else          smoothingTime = 0.250f; // Lush, slow upward ride
    }
    else if (mode == 3) // PUNCH MODE
    {
        if (isAttack) smoothingTime = 0.002f; // Lightning fast snap
        else          smoothingTime = 0.020f; // Quick drop back to body
    }
    else // BASE MODE
    {
        if (isAttack) smoothingTime = 0.050f; 
        else          smoothingTime = 0.100f; 
    }

    smoothedGain.reset(currentSampleRate, smoothingTime);

    // Store dB for UI Action Meter
    if (gainFactor <= 0.00001f) currentGainDb.store(-100.0f);
    else                        currentGainDb.store(20.0f * std::log10(gainFactor));

    smoothedGain.setTargetValue(gainFactor);

    // Apply the gain
    for (int sample = 0; sample < mainBuffer.getNumSamples(); ++sample)
    {
        float currentGain = smoothedGain.getNextValue();
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            auto* channelData = mainBuffer.getWritePointer(channel);
            channelData[sample] *= currentGain;
        }
    }

    mainBusLevel.store(mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples()));

    // ==========================================================
    // PURPLE MODIFIERS & TAPE SOFT CLIPPER
    // ==========================================================
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            // PLACEHOLDER FOR FLIP, SHRED, CHOP 
            // We will do illegal math to channelData[sample] here based on `mod` 

            // Final Analog Tape Soft Clipper (Keeps live thickener from clipping)
            channelData[sample] = std::tanh(channelData[sample]);
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) { juce::ignoreUnused (destData); }
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) { juce::ignoreUnused (data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }