#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm> 

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
    smoothedGain.reset(currentSampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue(1.0f);

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
    
    // ==========================================================
    // ABLETON CRASH FIX: Safely measure main input
    // ==========================================================
    float inputRMS = mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples());
    float inputPeak = 0.0f;
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch) {
        inputPeak = juce::jmax(inputPeak, mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples()));
    }

    float dryRMS = 0.0f;
    float dryPeak = 0.0f;

    // Safely measure sidechain ONLY if DAW provides it
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) 
    {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        dryRMS = sidechainBuffer.getRMSLevel(0, 0, sidechainBuffer.getNumSamples());
        for (int ch = 0; ch < sidechainBuffer.getNumChannels(); ++ch) {
            dryPeak = juce::jmax(dryPeak, sidechainBuffer.getMagnitude(ch, 0, sidechainBuffer.getNumSamples()));
        }
    }

    sidechainBusLevel.store(dryRMS);

    // ==========================================================
    // TEMPO SYNC ENGINE (For PUNCH Mode Release)
    // ==========================================================
    double currentBPM = 120.0;
    if (auto* activePlayHead = getPlayHead()) {
        if (auto positionInfo = activePlayHead->getPosition()) {
            if (positionInfo->getBpm().hasValue()) {
                currentBPM = *positionInfo->getBpm();
                if (currentBPM <= 0.0) currentBPM = 120.0; // Safety check
            }
        }
    }
    
    // Calculate a 1/128th Triplet Note in seconds
    float secondsPerQuarter = 60.0f / (float)currentBPM;
    float secondsPer128th = secondsPerQuarter / 32.0f;
    float musicalRelease = secondsPer128th * (2.0f / 3.0f);
    
    // Safety clamps: Keep it strictly between 2ms and 40ms
    if (musicalRelease < 0.002f) musicalRelease = 0.002f;
    if (musicalRelease > 0.040f) musicalRelease = 0.040f;

    // ==========================================================
    // SMART MACRO PEAK TRACKER
    // ==========================================================
    if (dryRMS > currentMacroPeak) {
        currentMacroPeak = dryRMS; // Instant Attack
    } else {
        currentMacroPeak *= macroPeakRelease; // 2-second Release
    }
    if (currentMacroPeak < 0.0001f) currentMacroPeak = 0.0001f;

    float gainFactor = 1.0f;
    int mode = currentMode.load();
    int mod = currentModifier.load(); 
    int ratio = currentRatio.load(); // Fetch the diagnostic ratio

    if (inputRMS >= 0.00001f)
    {
        if (dryRMS < 0.00001f)
        {
            gainFactor = 0.0f;
        }
        else
        {
            float desiredTargetLevel = dryRMS; 

            // ==========================================================
            // SPACE MODE: EXAGGERATED EXPANSION
            // ==========================================================
            if (mode == 2) 
            {
                float thresholdX = currentMacroPeak * 0.25f;  
                float thresholdY = currentMacroPeak * 0.01f;  
                
                // If Ratio is 1, use 1/2. Otherwise use 1/3, 1/6, 1/9
                float spaceExponent = (ratio == 1) ? 0.5f : (1.0f / (float)ratio);

                if (dryRMS < thresholdX && dryRMS > thresholdY) {
                    desiredTargetLevel = thresholdX * std::pow(dryRMS / thresholdX, spaceExponent);
                } 
                else if (dryRMS <= thresholdY && thresholdY > 0.00001f) {
                    float maxMultiplier = std::pow(thresholdX / thresholdY, spaceExponent); 
                    float fadeRatio = dryRMS / thresholdY; 
                    float evaporatingMultiplier = 1.0f + ((maxMultiplier - 1.0f) * fadeRatio);
                    desiredTargetLevel = dryRMS * evaporatingMultiplier;
                }
            }

            float rawTargetGain = desiredTargetLevel / (inputRMS + 0.00001f);

            // ==========================================================
            // VOX MODE: EXAGGERATED RIDING
            // ==========================================================
            if (mode == 1 && ratio > 1) {
                // Mathematically multiply the fader's travel distance in decibels
                float targetGainDb = 20.0f * std::log10(rawTargetGain);
                targetGainDb *= (float)ratio; 
                rawTargetGain = std::pow(10.0f, targetGainDb / 20.0f);
            }

            // ==========================================================
            // PUNCH MODE: EXAGGERATED IMPACT
            // ==========================================================
            if (mode == 3)
            {
                float dryCrest = dryPeak / (dryRMS + 0.00001f);
                float inputCrest = inputPeak / (inputRMS + 0.00001f);
                float loudnessComp = 1.0f + (1.0f - juce::jmin(1.0f, dryPeak));
                
                // Scale the 0.09f boost by our diagnostic ratio
                float punchMultiplier = 0.09f * (float)ratio;

                if (dryCrest > inputCrest + 0.05f) {
                    float restorationAmount = dryCrest / inputCrest;
                    rawTargetGain *= (restorationAmount * loudnessComp * 0.8f); 
                }
                else if (std::abs(dryCrest - inputCrest) <= 0.05f && dryCrest > 2.0f) {
                    float smartBoost = 1.0f + (punchMultiplier * dryCrest * loudnessComp);
                    if (smartBoost > 3.0f) smartBoost = 3.0f; 
                    rawTargetGain *= smartBoost;
                }
            }

            // ==========================================================
            // HYSTERESIS DEADBAND
            // ==========================================================
            float currentTarget = smoothedGain.getTargetValue();
            float difference = std::abs(rawTargetGain - currentTarget);
            
            if (difference < 0.06f && (mode == 1 || mode == 2)) { 
                gainFactor = currentTarget; 
            } else {
                gainFactor = rawTargetGain; 
            }

            if (gainFactor > 32.0f) gainFactor = 32.0f; 
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
        else          smoothingTime = 0.250f; 
    }
    else if (mode == 3) // PUNCH MODE
    {
        if (isAttack) smoothingTime = 0.001f; // Surgical 1ms Attack
        else          smoothingTime = musicalRelease; // Tempo-Synced 1/128 Triplet
    }
    else // BASE MODE
    {
        if (isAttack) smoothingTime = 0.050f; 
        else          smoothingTime = 0.100f; 
    }

    smoothedGain.reset(currentSampleRate, smoothingTime);

    if (gainFactor <= 0.00001f) currentGainDb.store(-100.0f);
    else                        currentGainDb.store(20.0f * std::log10(gainFactor));

    smoothedGain.setTargetValue(gainFactor);

    // ==========================================================
    // MASTER OUTPUT LOOP: GAIN, PURPLE MODIFIERS & CLIPPER
    // ==========================================================
    bool isTransient = (mode == 3 && gainFactor > 1.05f); // Used for punch clipper

    for (int sampleIndex = 0; sampleIndex < mainBuffer.getNumSamples(); ++sampleIndex)
    {
        float currentGain = smoothedGain.getNextValue();
        
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            auto* channelData = mainBuffer.getWritePointer(channel);
            float sampleVal = channelData[sampleIndex];
            float appliedGain = currentGain;

            // --- PURPLE MODIFIERS ---
            if (mod == 1) { 
                appliedGain = 1.0f / std::max(currentGain, 0.1f); 
            } 
            else if (mod == 3) { 
                if (dryRMS < (currentMacroPeak * 0.1f)) {
                    appliedGain = 0.0f;
                }
            }

            // 1. Apply Gain
            sampleVal *= appliedGain;

            // 2. SHRED Mod
            if (mod == 2) {
                sampleVal = std::sin(sampleVal * 5.0f); 
            }

            // 3. Clipper
            if (isTransient) {
                sampleVal = std::tanh(sampleVal * 1.05f); 
            } else {
                sampleVal = std::tanh(sampleVal);
            }

            channelData[sampleIndex] = sampleVal;
        }
    }

    mainBusLevel.store(mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples()));
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) { juce::ignoreUnused (destData); }
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) { juce::ignoreUnused (data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }