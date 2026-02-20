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
    
    // Reset Dual Mono Memories
    currentFaderGain[0] = 1.0f;
    currentFaderGain[1] = 1.0f;
    currentMacroPeak[0] = 0.0001f;
    currentMacroPeak[1] = 0.0001f;

    double blocksPerSecond = sampleRate / static_cast<double>(samplesPerBlock);
    macroPeakRelease = static_cast<float>(std::exp(-1.0 / (2.0 * blocksPerSecond)));
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
    int numChannels = std::min(mainBuffer.getNumChannels(), 2); // Ensure we cap at stereo for our arrays

    // ==========================================================
    // DUAL MONO MEASUREMENT ARRAYS
    // ==========================================================
    float inputRMS[2] = {0.0f, 0.0f};
    float inputPeak[2] = {0.0f, 0.0f};
    float dryRMS[2] = {0.0f, 0.0f};
    float dryPeak[2] = {0.0f, 0.0f};
    float gainFactor[2] = {1.0f, 1.0f};

    for (int ch = 0; ch < numChannels; ++ch) {
        inputRMS[ch] = mainBuffer.getRMSLevel(ch, 0, mainBuffer.getNumSamples());
        inputPeak[ch] = mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples());
    }

    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) 
    {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        int scChannels = sidechainBuffer.getNumChannels();
        for (int ch = 0; ch < numChannels; ++ch) {
            int scCh = (ch < scChannels) ? ch : 0; // Route mono sidechains safely to both L and R
            dryRMS[ch] = sidechainBuffer.getRMSLevel(scCh, 0, sidechainBuffer.getNumSamples());
            dryPeak[ch] = sidechainBuffer.getMagnitude(scCh, 0, sidechainBuffer.getNumSamples());
        }
    }

    // Update UI Sidechain Meter (Use the loudest channel)
    sidechainBusLevel.store(std::max(dryRMS[0], dryRMS[1]));

    // ==========================================================
    // TEMPO SYNC ENGINE (For PUNCH Mode Release)
    // ==========================================================
    double currentBPM = 120.0;
    if (auto* activePlayHead = getPlayHead()) {
        if (auto positionInfo = activePlayHead->getPosition()) {
            if (positionInfo->getBpm().hasValue()) {
                currentBPM = *positionInfo->getBpm();
                if (currentBPM <= 0.0) currentBPM = 120.0;
            }
        }
    }
    
    float secondsPerQuarter = 60.0f / (float)currentBPM;
    float secondsPer128th = secondsPerQuarter / 32.0f;
    float musicalRelease = std::clamp(secondsPer128th * (2.0f / 3.0f), 0.002f, 0.040f);

    int mode = currentMode.load();
    int mod = currentModifier.load(); 
    int ratio = currentRatio.load(); 
    int shredMode = currentShredMode.load();

    // ==========================================================
    // DUAL MONO TARGET CALCULATION
    // ==========================================================
    for (int ch = 0; ch < numChannels; ++ch) 
    {
        // Smart Macro Peak Tracker (Per Channel)
        if (dryRMS[ch] > currentMacroPeak[ch]) {
            currentMacroPeak[ch] = dryRMS[ch]; 
        } else {
            currentMacroPeak[ch] *= macroPeakRelease; 
        }
        if (currentMacroPeak[ch] < 0.0001f) currentMacroPeak[ch] = 0.0001f;

        if (inputRMS[ch] >= 0.00001f)
        {
            if (dryRMS[ch] < 0.00001f) {
                gainFactor[ch] = 0.0f;
            } else {
                float desiredTargetLevel = dryRMS[ch]; 

                // SPACE MODE
                if (mode == 2) {
                    float thresholdX = currentMacroPeak[ch] * 0.25f;  
                    float thresholdY = currentMacroPeak[ch] * 0.01f;  
                    float spaceExponent = (ratio == 1) ? 0.5f : (1.0f / (float)ratio);

                    if (dryRMS[ch] < thresholdX && dryRMS[ch] > thresholdY) {
                        desiredTargetLevel = thresholdX * std::pow(dryRMS[ch] / thresholdX, spaceExponent);
                    } 
                    else if (dryRMS[ch] <= thresholdY && thresholdY > 0.00001f) {
                        float maxMultiplier = std::pow(thresholdX / thresholdY, spaceExponent); 
                        float fadeRatio = dryRMS[ch] / thresholdY; 
                        float evaporatingMultiplier = 1.0f + ((maxMultiplier - 1.0f) * fadeRatio);
                        desiredTargetLevel = dryRMS[ch] * evaporatingMultiplier;
                    }
                }

                float rawTargetGain = desiredTargetLevel / (inputRMS[ch] + 0.00001f);

                // VOX MODE
                if (mode == 1 && ratio > 1) {
                    float targetGainDb = 20.0f * std::log10(rawTargetGain);
                    targetGainDb *= (float)ratio; 
                    rawTargetGain = std::pow(10.0f, targetGainDb / 20.0f);
                }

                // PUNCH MODE
                if (mode == 3) {
                    float dryCrest = dryPeak[ch] / (dryRMS[ch] + 0.00001f);
                    float inputCrest = inputPeak[ch] / (inputRMS[ch] + 0.00001f);
                    float loudnessComp = 1.0f + (1.0f - juce::jmin(1.0f, dryPeak[ch]));
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

                gainFactor[ch] = std::min(rawTargetGain, 32.0f);
                
                // SPACE MODE SAFETY CAP: Prevent tails from blowing out the speakers
                if (mode == 2) {
                    gainFactor[ch] = std::min(gainFactor[ch], 8.0f); 
                }
            }
        }
    }

    // ==========================================================
    // BALLISTICS SETTINGS
    // ==========================================================
    float attackTime = 0.05f; 
    float releaseTime = 0.10f; 
    
    if (mode == 1) { 
        attackTime = 0.015f; 
        releaseTime = 0.030f; 
    }
    else if (mode == 2) { 
        attackTime = 0.002f; // Lightning fast attack prevents transient slip-through
        releaseTime = musicalRelease * 8.0f; // Tempo-synced rhythmic swell
    }
    else if (mode == 3) { 
        attackTime = 0.001f; 
        releaseTime = musicalRelease; 
    }

    float sampleRateSafe = (currentSampleRate > 0.0) ? (float)currentSampleRate : 44100.0f;
    float attackCoeff = 1.0f - std::exp(-1.0f / (attackTime * sampleRateSafe));
    float releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * sampleRateSafe));

    // Update Action Meter (using highest L/R gain factor)
    float maxGainFactor = std::max(gainFactor[0], gainFactor[1]);
    if (maxGainFactor <= 0.00001f) currentGainDb.store(-100.0f);
    else                           currentGainDb.store(20.0f * std::log10(maxGainFactor));

    // ==========================================================
    // PER-CHANNEL SAMPLE PROCESSING
    // ==========================================================
    for (int ch = 0; ch < numChannels; ++ch)
    {
        bool isTransient = (mode == 3 && gainFactor[ch] > 1.05f);
        auto* channelData = mainBuffer.getWritePointer(ch);

       for (int sampleIndex = 0; sampleIndex < mainBuffer.getNumSamples(); ++sampleIndex)
        {
            // Dual Mono Fader Glide: Smart Directional Ballistics
            bool useFastTime = false;
            
            if (mode == 3) {
                // PUNCH MODE: Fast when fader goes UP (Spiking the transient)
                useFastTime = (gainFactor[ch] > currentFaderGain[ch]);
            } else {
                // VOX, SPACE, BASE: Fast when fader goes DOWN (Protecting from peaks)
                useFastTime = (gainFactor[ch] < currentFaderGain[ch]);
            }
            
            if (useFastTime) {
                currentFaderGain[ch] += attackCoeff * (gainFactor[ch] - currentFaderGain[ch]); 
            } else {
                currentFaderGain[ch] += releaseCoeff * (gainFactor[ch] - currentFaderGain[ch]); 
            }
            
            float sampleVal = channelData[sampleIndex];
            float appliedGain = currentFaderGain[ch];

            // Purple Modifiers
            if (mod == 1) { 
                appliedGain = 1.0f / std::max(currentFaderGain[ch], 0.1f); 
            } 
            else if (mod == 3) { 
                if (dryRMS[ch] < (currentMacroPeak[ch] * 0.1f)) appliedGain = 0.0f;
            }

            sampleVal *= appliedGain;

            // 2. THE SHRED TRILOGY
            if (mod == 2) {
                float rawSample = sampleVal; // Keep a copy of the dry signal for blending

                if (shredMode == 1) { 
                    // MODE I: THE SHADOW VERB (Living Wavefolder)
                    float drive = 5.0f + (currentMacroPeak[ch] * 25.0f); 
                    float folded = std::sin(sampleVal * drive);
                    
                    // Radically reduced the Wet mix to completely eliminate the volume explosion
                    sampleVal = (rawSample * 0.5f) + (folded * 0.25f);
                } 
                else if (shredMode == 2) { 
                    // MODE II: THE CRUSHED SYNTH (Tempo S&H)
                    // Increased multiplier to 0.45f to create musical "blocks" instead of sandy glitching
                    int holdTarget = juce::jmax(1, (int)(musicalRelease * sampleRateSafe * 0.45f));
                    if (holdCounter[ch] >= holdTarget) {
                        heldSample[ch] = sampleVal;
                        holdCounter[ch] = 0;
                    } else {
                        sampleVal = heldSample[ch];
                        holdCounter[ch]++;
                    }
                    // Blend 40% Dry signal back in to preserve the musical tone of guitars/synths
                    sampleVal = (rawSample * 0.4f) + (sampleVal * 0.6f * 0.5f); 
                } 
                else if (shredMode == 3) { 
                    // MODE III: THE BLACK HOLE (Wall of Fuzz)
                    float fuzz = std::tanh(sampleVal * 50.0f);
                    sampleVal = fuzz * 0.3f; 
                }
            } else {
                heldSample[ch] = sampleVal;
                holdCounter[ch] = 0;
            }

            // Clipper
            if (isTransient) {
                sampleVal = std::tanh(sampleVal * 1.05f); 
            } else {
                sampleVal = std::tanh(sampleVal);
            }

            channelData[sampleIndex] = sampleVal;
        }
    }

    // Measure Output Meter (Highest of L/R after processing)
    float outRmsL = mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples());
    float outRmsR = (numChannels > 1) ? mainBuffer.getRMSLevel(1, 0, mainBuffer.getNumSamples()) : outRmsL;
    mainBusLevel.store(std::max(outRmsL, outRmsR));
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) { juce::ignoreUnused (destData); }
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) { juce::ignoreUnused (data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }