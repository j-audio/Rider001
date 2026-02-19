#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true))
{}

PluginProcessor::~PluginProcessor() {}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    smoothedGain.reset(sampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue(1.0f);
    double blocksPerSecond = sampleRate / static_cast<double>(samplesPerBlock);
    macroPeakRelease = static_cast<float>(std::exp(-1.0 / (2.0 * blocksPerSecond)));
    currentMacroPeak = 0.0001f;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainBuffer = getBusBuffer(buffer, true, 0); 
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    float dryRMS = sidechainBuffer.getRMSLevel(0, 0, sidechainBuffer.getNumSamples());
    float inputRMS = mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples());

    float dryPeak = 0.0f;
    float inputPeak = 0.0f;
    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch) {
        dryPeak = juce::jmax(dryPeak, sidechainBuffer.getMagnitude(ch, 0, sidechainBuffer.getNumSamples()));
        inputPeak = juce::jmax(inputPeak, mainBuffer.getMagnitude(ch, 0, mainBuffer.getNumSamples()));
    }

    if (dryRMS > currentMacroPeak) currentMacroPeak = dryRMS; 
    else currentMacroPeak *= macroPeakRelease; 

    float gainFactor = 1.0f;
    int mode = currentMode.load();

    if (inputRMS >= 0.00001f && dryRMS >= 0.00001f)
    {
        float desiredTargetLevel = dryRMS; 

        if (mode == 2) { // SPACE
            float thresholdX = currentMacroPeak * 0.25f;  
            float thresholdY = currentMacroPeak * 0.01f;  
            if (dryRMS < thresholdX && dryRMS > thresholdY)
                desiredTargetLevel = thresholdX * std::sqrt(dryRMS / thresholdX);
        }

        float rawTargetGain = desiredTargetLevel / (inputRMS + 0.00001f);

        if (mode == 3) { // PUNCH
            float dryCrest = dryPeak / (dryRMS + 0.00001f);
            float inputCrest = inputPeak / (inputRMS + 0.00001f);
            float loudnessComp = 1.0f + (1.0f - juce::jmin(1.0f, dryPeak));

            if (dryCrest > inputCrest + 0.05f) {
                rawTargetGain *= (dryCrest / inputCrest) * (1.0f + (0.15f * loudnessComp));
            } else if (std::abs(dryCrest - inputCrest) <= 0.05f && dryCrest > 2.0f) {
                rawTargetGain *= 1.0f + (0.12f * dryCrest * loudnessComp);
            }
        }

        if (mode == 2 && rawTargetGain < 1.0f) rawTargetGain = 1.0f; 
        gainFactor = juce::jmin(rawTargetGain, 16.0f); 
    }

    float smoothingTime = 0.050f;
    if (mode == 1)      smoothingTime = (gainFactor < smoothedGain.getCurrentValue()) ? 0.015f : 0.030f; 
    else if (mode == 2) smoothingTime = (gainFactor < smoothedGain.getCurrentValue()) ? 0.050f : 0.250f; 
    else if (mode == 3) smoothingTime = (gainFactor < smoothedGain.getCurrentValue()) ? 0.002f : 0.025f; 

    smoothedGain.reset(currentSampleRate, smoothingTime);
    smoothedGain.setTargetValue(gainFactor);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        float g = smoothedGain.getNextValue();
        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            float* data = buffer.getWritePointer(ch);
            data[sample] = std::tanh(data[sample] * g);
        }
    }
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void PluginProcessor::releaseResources() {}
const juce::String PluginProcessor::getName() const { return "J-RIDER"; }
bool PluginProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor (*this); }
bool PluginProcessor::acceptsMidi() const { return false; }
bool PluginProcessor::producesMidi() const { return false; }
double PluginProcessor::getTailLengthSeconds() const { return 0.0; }
int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int index) {}
const juce::String PluginProcessor::getProgramName (int index) { return {}; }
void PluginProcessor::changeProgramName (int index, const juce::String& newName) {}
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) {}
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) {}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }