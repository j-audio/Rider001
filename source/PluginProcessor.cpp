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
    juce::ignoreUnused (samplesPerBlock); // We no longer rely on block sizes!
    currentSampleRate = sampleRate;
    
    currentFaderGain[0] = 1.0f;
    currentFaderGain[1] = 1.0f;
    
    envStateLive[0] = 0.0f; envStateLive[1] = 0.0f;
    envStateGuide[0] = 0.0f; envStateGuide[1] = 0.0f;
    peakStateLive[0] = 0.0f; peakStateLive[1] = 0.0f;
    peakStateGuide[0] = 0.0f; peakStateGuide[1] = 0.0f;

    // Buffer-Agnostic 10ms RMS Envelope window
    envCoeff = static_cast<float>(std::exp(-1.0 / (0.010 * sampleRate)));
    peakReleaseCoeff = static_cast<float>(std::exp(-1.0 / (0.050 * sampleRate)));
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
    int numSamples = mainBuffer.getNumSamples();

    bool forceExt = forceExternalSidechain.load();
    bool hasSidechain = (getBusCount(true) > 1 && getBus(true, 1)->isEnabled());
    
    juce::AudioBuffer<float> scBuffer;
    int scChannels = 0;
    if (hasSidechain) {
        scBuffer = getBusBuffer(buffer, true, 1);
        scChannels = scBuffer.getNumChannels();
    }

    // ==========================================================
    // TEMPO & PLAYHEAD ENGINE
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
    
    bool justStartedPlaying = isPlaying && !wasPlaying;
    bool jumpedBackward = currentPPQ < previousPPQ;
    bool forceSnapFader = justStartedPlaying || (isPlaying && jumpedBackward);
    
    previousPPQ = currentPPQ;
    wasPlaying = isPlaying;
    
    // Clear continuous states on loop/play to prevent mathematical bleed-over
    if (forceSnapFader) {
        envStateLive[0] = 0.0f; envStateLive[1] = 0.0f;
        envStateGuide[0] = 0.0f; envStateGuide[1] = 0.0f;
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

    bool writeMode = isGhostRecording.load();
    bool readMode = isGhostReading.load();
    
    // We establish a high-resolution grid (100 scans per beat)
    double ppqResolution = 100.0; 
    
    // Safe memory expansion at the start of the block to prevent mid-loop audio dropouts
    if (writeMode && isPlaying) {
        ghostLedState.store(2);
        double endPPQ = currentPPQ + (numSamples * ((currentBPM / 60.0) / currentSampleRate));
        int maxNeededIndex = (int)(endPPQ * ppqResolution) + 10;
        
        if (maxNeededIndex >= ghostMapL.size()) {
            ghostMapL.resize(maxNeededIndex + 2000, -1.0f); // Pre-allocate chunks to save CPU
            ghostMapR.resize(maxNeededIndex + 2000, -1.0f);
        }
    } else if (writeMode && !isPlaying) {
        ghostLedState.store(3);
    } else {
        ghostLedState.store(0);
    }

    float sampleRateSafe = (currentSampleRate > 0.0) ? (float)currentSampleRate : 44100.0f;

    // Ballistics
    float attackTime = (readMode) ? 0.25f : ((mode == 1) ? 0.015f : ((mode == 2) ? 0.002f : 0.001f));
    float releaseTime = (readMode) ? 0.25f : ((mode == 1) ? 0.030f : ((mode == 2) ? musicalRelease * 8.0f : musicalRelease));
    float attackCoeff = 1.0f - std::exp(-1.0f / (attackTime * currentSampleRate));
    float releaseCoeff = 1.0f - std::exp(-1.0f / (releaseTime * currentSampleRate));

    // UI Meter Trackers
    float maxLiveRMS = 0.0f;
    float maxGuideRMS = 0.0f;
    float maxFaderVal = 0.0f;
    float displayGhostTarget = 0.0f;

    // Calculate exactly how much musical time passes during 1 single audio sample
    double ppqPerSample = (currentBPM / 60.0) / currentSampleRate;

    // ==========================================================
    // THE SAMPLE-ACCURATE ENGINE (100% Buffer Independent)
    // ==========================================================
    for (int i = 0; i < numSamples; ++i) 
    {
        // 1. Calculate Exact Micro-Position
        double exactSamplePPQ = currentPPQ + (i * ppqPerSample);
        double exactIndex = exactSamplePPQ * ppqResolution;
        int arrayIdx = (int)exactIndex;

        for (int ch = 0; ch < numChannels; ++ch) 
        {
            // 2. Fetch Raw Audio Samples
            float liveSample = mainBuffer.getSample(ch, i);
            float guideSample = liveSample; 
            
            if (forceExt && hasSidechain) {
                int scCh = (ch < scChannels) ? ch : 0;
                guideSample = scBuffer.getSample(scCh, i);
            } else if (forceExt && !hasSidechain) {
                guideSample = 0.0f; // Enforce silence if nothing is plugged in
            }

            // 3. Continuous Envelope Followers (Replaces block-based RMS)
            envStateLive[ch] = envCoeff * envStateLive[ch] + (1.0f - envCoeff) * (liveSample * liveSample);
            float currentLiveRMS = std::sqrt(envStateLive[ch]);
            
            envStateGuide[ch] = envCoeff * envStateGuide[ch] + (1.0f - envCoeff) * (guideSample * guideSample);
            float currentGuideRMS = std::sqrt(envStateGuide[ch]);
            
            peakStateLive[ch] = std::max(std::abs(liveSample), peakStateLive[ch] * peakReleaseCoeff);
            peakStateGuide[ch] = std::max(std::abs(guideSample), peakStateGuide[ch] * peakReleaseCoeff);

            // Record to Ghost Timeline Sample-by-Sample
            if (writeMode && isPlaying && arrayIdx < ghostMapL.size()) {
                if (ch == 0) ghostMapL[arrayIdx] = currentGuideRMS;
                if (ch == 1) ghostMapR[arrayIdx] = currentGuideRMS;
                lastRecordedPPQ.store(exactSamplePPQ);
            }

            // 4. Determine The Source of Truth
            float targetRMS = currentGuideRMS; 
            float targetGain = 1.0f;

            bool hasGhostData = false;
            if (readMode && isPlaying && arrayIdx + 1 < ghostMapL.size()) {
                float val1 = (ch == 0) ? ghostMapL[arrayIdx] : ghostMapR[arrayIdx];
                float val2 = (ch == 0) ? ghostMapL[arrayIdx + 1] : ghostMapR[arrayIdx + 1];
                
                if (val1 >= 0.0f && val2 >= 0.0f) {
                    hasGhostData = true;
                    // Perfect Sample-Accurate Linear Interpolation between grid points
                    float fraction = (float)(exactIndex - (double)arrayIdx);
                    targetRMS = val1 + fraction * (val2 - val1);
                    if (ch == 0) displayGhostTarget = targetRMS;
                }
            }

            // 5. The Math Core
            if (readMode && (!isPlaying || !hasGhostData)) {
                targetGain = (!isPlaying) ? 1.0f : 0.0f; // Pause = Unity, Exhausted = Silence
            }
            else if (currentLiveRMS >= 0.00001f) {
                if (targetRMS < 0.00001f) {
                    targetGain = 0.0f;
                } else {
                    float desiredLevel = targetRMS;

                    if (mode == 2) {
                        float threshX = 0.25f;  
                        float threshY = 0.01f;  
                        float exp = (ratio == 1) ? 0.5f : (1.0f / (float)ratio);

                        if (desiredLevel < threshX && desiredLevel > threshY) {
                            desiredLevel = threshX * std::pow(desiredLevel / threshX, exp);
                        } else if (desiredLevel <= threshY) {
                            float maxMult = std::pow(threshX / threshY, exp); 
                            float fade = desiredLevel / threshY; 
                            desiredLevel = desiredLevel * (1.0f + ((maxMult - 1.0f) * fade));
                        }
                    }

                    targetGain = desiredLevel / currentLiveRMS; // PERFECT 1:1 DIVISION

                    if (mode == 1 && ratio > 1) {
                        float db = 20.0f * std::log10(targetGain);
                        targetGain = std::pow(10.0f, (db * (float)ratio) / 20.0f);
                    }

                    bool isTransient = false;
                    if (mode == 3) {
                        float dryCrest = peakStateGuide[ch] / (targetRMS + 0.00001f);
                        float inCrest = peakStateLive[ch] / (currentLiveRMS + 0.00001f);
                        float loudComp = 1.0f + (1.0f - juce::jmin(1.0f, peakStateGuide[ch]));
                        
                        if (dryCrest > inCrest + 0.05f) {
                            targetGain *= ((dryCrest / inCrest) * loudComp * 0.8f);
                            isTransient = true;
                        } else if (std::abs(dryCrest - inCrest) <= 0.05f && dryCrest > 2.0f) {
                            targetGain *= std::min(1.0f + (0.09f * ratio * dryCrest * loudComp), 3.0f);
                            isTransient = true;
                        }
                    }

                    targetGain = std::clamp(targetGain, 0.0f, 32.0f);
                    if (hasGhostData) targetGain = std::clamp(targetGain, 0.25f, 4.0f);
                    if (mode == 2 && currentLiveRMS >= 0.25f) targetGain = 1.0f;
                }
            }

            // 6. Smooth the Fader & Apply
            if (forceSnapFader && i == 0) {
                currentFaderGain[ch] = targetGain; // Prevents the start lag
            } else {
                bool useFast = (mode == 3) ? (targetGain > currentFaderGain[ch]) : (targetGain < currentFaderGain[ch]);
                if (readMode) useFast = false;
                
                currentFaderGain[ch] += (useFast ? attackCoeff : releaseCoeff) * (targetGain - currentFaderGain[ch]);
            }

            float outSample = liveSample * (flipOn ? (1.0f / std::max(currentFaderGain[ch], 0.1f)) : currentFaderGain[ch]);

            // 7. Modifiers & Clippers
            if (shredOn) {
                if (shredMode == 1) {
                    outSample = (outSample * 0.5f) + (std::sin(outSample * 25.0f) * 0.25f);
                } else if (shredMode == 3) {
                    outSample = std::tanh(outSample * 50.0f) * 0.3f;
                }
            }
            if (chopOn && targetRMS < 0.05f) outSample = 0.0f;

            // Transparent Clipping (Protects 1:1 math)
            if (shredOn || mode != 3) {
                outSample = std::clamp(outSample, -1.0f, 1.0f);
            } else {
                outSample = std::tanh(outSample * 1.05f); 
            }

            mainBuffer.setSample(ch, i, outSample);

            // Track peaks for UI
            maxLiveRMS = std::max(maxLiveRMS, currentLiveRMS);
            maxGuideRMS = std::max(maxGuideRMS, targetRMS);
            maxFaderVal = std::max(maxFaderVal, currentFaderGain[ch]);
        }
    }

    // ==========================================================
    // UI UPDATES
    // ==========================================================
    mainBusLevel.store(maxLiveRMS);
    sidechainBusLevel.store(maxGuideRMS);
    currentGhostTargetUI.store(displayGhostTarget);
    
    if (maxFaderVal <= 0.00001f) currentGainDb.store(-100.0f);
    else                         currentGainDb.store(20.0f * std::log10(maxFaderVal));
}

//==============================================================================
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) { juce::ignoreUnused (destData); }
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) { juce::ignoreUnused (data, sizeInBytes); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }