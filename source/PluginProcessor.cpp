#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (samplesPerBlock);

    // Reset the gain smoother with a 0.05 second (50ms) ramp
    smoothedGain.reset(sampleRate, 0.05);
    smoothedGain.setCurrentAndTargetValue(1.0f);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // 1. Check Main Output (Mono or Stereo)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // 2. Check Input matches Output (unless it's a Synth)
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    // 3. THE SIDECHAIN ADDITION
    // We check the Auxiliary Input Bus (Index 1)
    auto sidechainBus = layouts.getChannelSet(true, 1);

    // We allow it to be empty (Disabled), Mono, or Stereo
    if (!sidechainBus.isDisabled() &&
        sidechainBus != juce::AudioChannelSet::mono() &&
        sidechainBus != juce::AudioChannelSet::stereo())
        return false;

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 1. Clear extra channels to prevent noise/feedback
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // --- GAIN MATCHING GUTS START HERE ---

    // 2. Separate the Main (Wet) and Sidechain (Dry) buffers
    auto mainBuffer = getBusBuffer(buffer, true, 0); 
    auto sidechainBuffer = getBusBuffer(buffer, true, 1);

    // 3. Measure input levels
    float dryRMS   = sidechainBuffer.getRMSLevel(0, 0, sidechainBuffer.getNumSamples());
    float inputRMS = mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples());

    // Store sidechain level for UI
    sidechainBusLevel.store(dryRMS);

    // 4. Gain Factor — if sidechain is silent, force output to zero
    float gainFactor;
    if (dryRMS < 0.00001f)
    {
        gainFactor = 0.0f;
    }
    else
    {
        gainFactor = dryRMS / (inputRMS + 0.00001f);
        // 5. Safety: cap at 4x (12 dB)
        if (gainFactor > 4.0f) gainFactor = 4.0f;
    }

    // Store gain in dB — explicit zero guard prevents log(0) = -inf
    if (gainFactor <= 0.00001f)
        currentGainDb.store(-100.0f);
    else
        currentGainDb.store(20.0f * std::log10(gainFactor));

    // 6. Set the target value for the smoother
    smoothedGain.setTargetValue(gainFactor);

    // 7. Apply the smoothed gain sample-by-sample to the Main audio
    for (int sample = 0; sample < mainBuffer.getNumSamples(); ++sample)
    {
        float currentGain = smoothedGain.getNextValue();
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
        {
            auto* channelData = mainBuffer.getWritePointer(channel);
            channelData[sample] *= currentGain;
        }
    }

    // Measure output AFTER gain — this is what the Output meter displays
    mainBusLevel.store(mainBuffer.getRMSLevel(0, 0, mainBuffer.getNumSamples()));

    // --- GAIN MATCHING GUTS END HERE ---

    // 8. Apply soft clipping to prevent output clipping (tanh limits to ±1.0)
    for (int channel = 0; channel < totalNumOutputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] = std::tanh(channelData[sample]);
        }
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
