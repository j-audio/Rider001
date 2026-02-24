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
    
    currentFaderGain[0] = 1.0f;
    currentFaderGain[1] = 1.0f;
    currentMacroPeak[0] = 0.0001f;
    currentMacroPeak[1] = 0.0001f;
    liveMacroPeak[0]    = 0.0001f;
    liveMacroPeak[1]    = 0.0001f;

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
    int numChannels = std::min(mainBuffer.getNumChannels(), 2); 

    // ==========================================================
    // INITIAL MEASUREMENT ARRAYS
    // ==========================================================
    float inputRMS[2] = {0.0f, 0.0f};
    float inputPeak[2] = {0.0f, 0.0f};
    float guideRMS[2] = {0.0f, 0.0f};
    float guidePeak[2] = {0.0f, 0.0f};
    float gainFactor[2] = {1.0f, 1.0f};

    for (int ch = 0; ch < numChannels; ++ch) {
        inputRMS[ch] = mainBuffer.getRMSLevel(ch, 0, mainBuffer.getNumSamples());
        inputPeak[ch] = mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples());
    }

    bool forceExt = forceExternalSidechain.load();
    bool hasSidechain = (getBusCount(true) > 1 && getBus(true, 1)->isEnabled());
    
    // Safely route the guide signal based on UI buttons and actual DAW routing
    for (int ch = 0; ch < numChannels; ++ch) {
        if (forceExt && !hasSidechain) {
            guideRMS[ch] = 0.0f;
            guidePeak[ch] = 0.0f;
        } 
        else if (forceExt && hasSidechain) {
            auto scBuffer = getBusBuffer(buffer, true, 1);
            int scCh = (ch < scBuffer.getNumChannels()) ? ch : 0; 
            guideRMS[ch] = scBuffer.getRMSLevel(scCh, 0, scBuffer.getNumSamples());
            guidePeak[ch] = scBuffer.getMagnitude(scCh, 0, scBuffer.getNumSamples());
        } 
        else {
            guideRMS[ch] = inputRMS[ch];
            guidePeak[ch] = inputPeak[ch];
        }
    }

    // ==========================================================
    // TEMPO & PLAYHEAD ENGINE (WITH SNAP DETECTOR)
    // ==========================================================
    double currentBPM = 120.0;
    double currentPPQ = 0.0;
    bool isPlaying = false;

    if (auto* activePlayHead = getPlayHead()) {
        if (auto positionInfo = activePlayHead->getPosition()) {
            if (positionInfo->getBpm().hasValue()) currentBPM = *positionInfo->getBpm();
            if (positionInfo->getPpqPosition().hasValue()) currentPPQ = *positionInfo->getPpqPosition();
            isPlaying = positionInfo->getIsPlaying();
        }
    }
    
    static double previousPPQ = 0.0;
    static bool wasPlaying = false;
    
    // FIX 1 & 3: Detect if we just pressed Play, or if the DAW looped backwards
    bool justStartedPlaying = isPlaying && !wasPlaying;
    bool jumpedBackward = currentPPQ < previousPPQ;
    bool forceSnapFader = justStartedPlaying || (isPlaying && jumpedBackward);
    
    previousPPQ = currentPPQ;
    wasPlaying = isPlaying;
    
    if (forceSnapFader) {
        currentMacroPeak[0] = 0.0001f;
        currentMacroPeak[1] = 0.0001f;
    }

    float secondsPerQuarter = 60.0f / (float)currentBPM;
    float secondsPer128th = secondsPerQuarter / 32.0f;
    float musicalRelease = std::clamp(secondsPer128th * (2.0f / 3.0f), 0.002f, 0.040f);
    
    int mode = currentMode.load();
    bool flipOn = isFlipActive.load();
    bool shredOn = isShredActive.load();
    bool chopOn = isChopActive.load();
    float chopThresh = chopThreshold.load();
    int ratio = currentRatio.load(); 
    int shredMode = currentShredMode.load();

    // ==========================================================
    // THE GHOST ENGINE: RECORDING LOGIC
    // ==========================================================
    bool writeMode = isGhostRecording.load();
    bool readMode = isGhostReading.load();
    int ppqIndex = (int)(currentPPQ * 50.0); 
    
    if (writeMode) 
    {
        if (!isPlaying) {
            ghostLedState.store(3); 
        } 
        else if (currentPPQ <= 0.0) {
            ghostLedState.store(1); 
        }
        else 
        {
            bool isAppending = (ghostMapL.size() == 0 || ppqIndex <= ghostMapL.size() + 2);
            if (isAppending) 
            {
                ghostLedState.store(2); 
                if (ppqIndex >= ghostMapL.size()) {
                    ghostMapL.resize((size_t)ppqIndex + 1, 0.0f);
                    ghostMapR.resize((size_t)ppqIndex + 1, 0.0f);
                }
                ghostMapL[(size_t)ppqIndex] = guideRMS[0];
                ghostMapR[(size_t)ppqIndex] = guideRMS[1];
                lastRecordedPPQ.store(currentPPQ);
            } else {
                ghostLedState.store(1); 
            }
        }
    } 
    else {
        ghostLedState.store(0);
    }

    // ==========================================================
    // UNIFIED SOURCE OF TRUTH
    // ==========================================================
    bool hasGhostData = (readMode && isPlaying && ppqIndex >= 0 && ppqIndex < ghostMapL.size());

    for (int ch = 0; ch < numChannels; ++ch) {
        if (hasGhostData) {
            float ghostVal = (ch == 0) ? ghostMapL[(size_t)ppqIndex] : ghostMapR[(size_t)ppqIndex];
            guideRMS[ch] = ghostVal;
            guidePeak[ch] = ghostVal; 
            if (ch == 0) currentGhostTargetUI.store(ghostVal);
        }
    }

    sidechainBusLevel.store(std::max(guideRMS[0], guideRMS[1]));

    // ==========================================================
    // DUAL MONO TARGET CALCULATION
    // ==========================================================
    for (int ch = 0; ch < numChannels; ++ch) 
    {
        if (readMode) {
            // FIX 2: If Paused, needle rests at 0 (Unity). If exhausted, silence.
            if (!isPlaying) {
                gainFactor[ch] = 1.0f; // Paused -> Unity
                if (ch == 0) currentGhostTargetUI.store(0.0f);
                continue;
            }
            if (!hasGhostData) {
                gainFactor[ch] = 0.0f; // Exhausted -> Silence
                if (ch == 0) currentGhostTargetUI.store(0.0f);
                continue; 
            }
        }

        if (guideRMS[ch] > currentMacroPeak[ch]) {
            currentMacroPeak[ch] = guideRMS[ch]; 
        } else {
            currentMacroPeak[ch] *= macroPeakRelease; 
        }
        if (currentMacroPeak[ch] < 0.0001f) currentMacroPeak[ch] = 0.0001f;

        if (inputRMS[ch] >= 0.00001f)
        {
            if (guideRMS[ch] < 0.00001f) {
                gainFactor[ch] = 0.0f;
            } else {
                float desiredTargetLevel = guideRMS[ch]; 
                float liveInputBase = inputRMS[ch]; 

                if (mode == 2) {
                    float thresholdX = currentMacroPeak[ch] * 0.25f;  
                    float thresholdY = currentMacroPeak[ch] * 0.01f;  
                    float spaceExponent = (ratio == 1) ? 0.5f : (1.0f / (float)ratio);

                    if (desiredTargetLevel < thresholdX && desiredTargetLevel > thresholdY) {
                        desiredTargetLevel = thresholdX * std::pow(desiredTargetLevel / thresholdX, spaceExponent);
                    } 
                    else if (desiredTargetLevel <= thresholdY && thresholdY > 0.00001f) {
                        float maxMultiplier = std::pow(thresholdX / thresholdY, spaceExponent); 
                        float fadeRatio = desiredTargetLevel / thresholdY; 
                        float evaporatingMultiplier = 1.0f + ((maxMultiplier - 1.0f) * fadeRatio);
                        desiredTargetLevel = desiredTargetLevel * evaporatingMultiplier;
                    }
                }

                float rawTargetGain = desiredTargetLevel / (liveInputBase + 0.00001f);

                if (mode == 1 && ratio > 1) {
                    float targetGainDb = 20.0f * std::log10(rawTargetGain);
                    targetGainDb *= (float)ratio; 
                    rawTargetGain = std::pow(10.0f, targetGainDb / 20.0f);
                }

                if (mode == 3) {
                    float dryCrest = guidePeak[ch] / (guideRMS[ch] + 0.00001f);
                    float inputCrest = inputPeak[ch] / (inputRMS[ch] + 0.00001f);
                    float loudnessComp = 1.0f + (1.0f - juce::jmin(1.0f, guidePeak[ch]));
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

                gainFactor[ch] = std::clamp(rawTargetGain, 0.0f, 32.0f);
                
                if (hasGhostData) {
                    gainFactor[ch] = std::clamp(gainFactor[ch], 0.25f, 4.0f); 
                }
                
                if (mode == 2) {
                    float thresholdX = currentMacroPeak[ch] * 0.25f;
                    if (inputRMS[ch] >= thresholdX) {
                        gainFactor[ch] = 1.0f; 
                    } else {
                        gainFactor[ch] = std::min(gainFactor[ch], 8.0f); 
                    }
                }
            }
        } else {
            gainFactor[ch] = 1.0f; 
        }
    }

    // ==========================================================
    // BALLISTICS SETTINGS
    // ==========================================================
    float attackTime = 0.05f; 
    float releaseTime = 0.10f; 
    
    if (readMode) {
        attackTime = 0.25f;  
        releaseTime = 0.25f; 
    }
    else if (mode == 1) { 
        attackTime = 0.015f; 
        releaseTime = 0.030f; 
    }
    else if (mode == 2) { 
        attackTime = 0.002f; 
        releaseTime = musicalRelease * 8.0f; 
    }
    else if (mode == 3) { 
        attackTime = 0.001f; 
        releaseTime = musicalRelease; 
    }

    float sampleRateSafe = (currentSampleRate > 0.0) ? (float)currentSampleRate : 44100.0f;
    float attackCoeff = 1.0f - std::exp(-1.0f / (attackTime * sampleRateSafe));
    float releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * sampleRateSafe));

    // ==========================================================
    // PER-CHANNEL SAMPLE PROCESSING
    // ==========================================================
    for (int ch = 0; ch < numChannels; ++ch)
    {
        bool isTransient = (mode == 3 && gainFactor[ch] > 1.05f);
        auto* channelData = mainBuffer.getWritePointer(ch);
        
        // FIX 1 & 3 APPLIED: Instantly snap the fader on Play or Loop backwards!
        if (forceSnapFader) {
            currentFaderGain[ch] = gainFactor[ch];
        }

       for (int sampleIndex = 0; sampleIndex < mainBuffer.getNumSamples(); ++sampleIndex)
        {
            bool useFastTime = false;
            
            if (readMode) {
                useFastTime = false; 
            }
            else if (mode == 3) {
                useFastTime = (gainFactor[ch] > currentFaderGain[ch]);
            } else {
                useFastTime = (gainFactor[ch] < currentFaderGain[ch]);
            }
            
            if (useFastTime) {
                currentFaderGain[ch] += attackCoeff * (gainFactor[ch] - currentFaderGain[ch]); 
            } else {
                currentFaderGain[ch] += releaseCoeff * (gainFactor[ch] - currentFaderGain[ch]); 
            }
            
            float sampleVal = channelData[sampleIndex];
            float appliedGain = currentFaderGain[ch];

            if (flipOn) { 
                appliedGain = 1.0f / std::max(currentFaderGain[ch], 0.1f); 
            } 
            
            sampleVal *= appliedGain;

            if (shredOn) {
                float rawSample = sampleVal; 

                if (shredMode == 1) { 
                    float drive = 5.0f + (currentMacroPeak[ch] * 25.0f); 
                    float folded = std::sin(sampleVal * drive);
                    sampleVal = (rawSample * 0.5f) + (folded * 0.25f);
                } 
                else if (shredMode == 2) { 
                    int holdTarget = juce::jmax(1, (int)(musicalRelease * sampleRateSafe * 0.45f));
                    if (holdCounter[ch] >= holdTarget) {
                        heldSample[ch] = sampleVal;
                        holdCounter[ch] = 0;
                    } else {
                        sampleVal = heldSample[ch];
                        holdCounter[ch]++;
                    }
                    float fatDry = std::tanh(rawSample * 2.0f) * 0.5f;
                    sampleVal = fatDry + (sampleVal * 0.8f); 
                } 
                else if (shredMode == 3) { 
                    float fuzz = std::tanh(sampleVal * 50.0f);
                    sampleVal = fuzz * 0.3f; 
                }
            } else {
                heldSample[ch] = sampleVal;
                holdCounter[ch] = 0;
            }

            if (chopOn) { 
                if (guideRMS[ch] < (currentMacroPeak[ch] * chopThresh)) {
                    sampleVal = 0.0f;
                }
            }

            // Transparent Hard Clipper for Base & Ghost modes
            if (shredOn) {
                sampleVal = std::clamp(sampleVal, -1.0f, 1.0f);
            } 
            else if (isTransient) {
                sampleVal = std::tanh(sampleVal * 1.05f); 
            } 
            else {
                sampleVal = std::clamp(sampleVal, -1.0f, 1.0f);
            }

            channelData[sampleIndex] = sampleVal;
        }
    }

    float maxFaderGain = std::max(currentFaderGain[0], currentFaderGain[1]);
    if (maxFaderGain <= 0.00001f) currentGainDb.store(-100.0f);
    else                          currentGainDb.store(20.0f * std::log10(maxFaderGain));

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