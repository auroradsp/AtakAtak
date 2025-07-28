#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AtakAtakAudioProcessor::AtakAtakAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       parameters(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Initialize DSP processors
    inputGainProcessor = std::make_unique<GainProcessor>();
    outputGainProcessor = std::make_unique<GainProcessor>();
    transientDesigner = std::make_unique<TransientDesigner>();
}

AtakAtakAudioProcessor::~AtakAtakAudioProcessor()
{
}

//==============================================================================
const juce::String AtakAtakAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AtakAtakAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AtakAtakAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AtakAtakAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AtakAtakAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AtakAtakAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AtakAtakAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AtakAtakAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AtakAtakAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AtakAtakAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AtakAtakAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize DSP
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (juce::jmax (getTotalNumInputChannels(), getTotalNumOutputChannels()));
    
    // Prepare gain processors
    inputGainProcessor->prepare(spec);
    outputGainProcessor->prepare(spec);
    
    // Reset processors
    inputGainProcessor->reset();
    outputGainProcessor->reset();
    
    // Initialize transient designer
    transientDesigner->prepare(spec);
}

void AtakAtakAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AtakAtakAudioProcessor::isBusesLayoutSupported (const BusesLayout& buses) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (buses);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (buses.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && buses.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (buses.getMainOutputChannelSet() != buses.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AtakAtakAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't have input data to prevent them from being processed by
    // the plugin.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Check if bypassed
    if (parameters.getRawParameterValue("bypass")->load())
    {
        return; // Pass through unchanged
    }

    // Update parameters
    updateParameters();
    
    // Process audio
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    
    // Apply input gain
    inputGainProcessor->process(context);
    
    // Apply transient designer
    transientDesigner->process(context);
    
    // Apply output gain
    outputGainProcessor->process(context);
}

//==============================================================================
bool AtakAtakAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AtakAtakAudioProcessor::createEditor()
{
    return new AtakAtakAudioProcessorEditor (*this);
}

//==============================================================================
void AtakAtakAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save all parameter values to memory block
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AtakAtakAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore all parameter values from memory block
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtakAtakAudioProcessor();
}

juce::AudioProcessorValueTreeState::ParameterLayout AtakAtakAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Input/Output parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("inputGain", "Input Gain", -24.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("outputGain", "Output Gain", -24.0f, 24.0f, 0.0f));
    
    // Attack parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attackAmount", "Attack Amount", -100.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attackTime", "Attack Time", 0.1f, 100.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attackThreshold", "Attack Threshold", -60.0f, 0.0f, -40.0f));
    
    // Sustain parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustainAmount", "Sustain Amount", -100.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("releaseTime", "Release Time", 1.0f, 1000.0f, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("sustainThreshold", "Sustain Threshold", -60.0f, 0.0f, -40.0f));
    
    // Psychoacoustic parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("maskingThreshold", "Masking Threshold", -30.0f, 0.0f, -15.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("criticalBandWeight", "Critical Band Weight", 0.0f, 2.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("temporalWeight", "Temporal Weight", 0.0f, 2.0f, 1.0f));
    
    // SPL Differential Envelope parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("fastAttackMs", "Fast Attack", 0.1f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("slowAttackMs", "Slow Attack", 5.0f, 50.0f, 15.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("releaseMs", "Release", 5.0f, 100.0f, 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("powerMemoryMs", "Power Memory", 0.1f, 5.0f, 1.0f));
    // SPL Mode removed - now using STA/LTA automatic detection!
    
    // SNAP Enhancement parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("snapAmount", "Snap Amount", 0.0f, 200.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("snapHardness", "Snap Hardness", 0.1f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("harmonicEnhancement", "Harmonic Enhancement", 0.0f, 100.0f, 0.0f));
    
    // DrumSnapper-inspired parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>("focus", "Focus", 1.0f, 5.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hfGain", "HF Gain", 1.0f, 10.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hfSaturation", "HF Saturation", 0.0f, 100.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("tapeClip", "Tape Clip", false));
    
    // PeakEater-style Clipper parameters (Final Stage)
    params.push_back(std::make_unique<juce::AudioParameterBool>("clipperEnabled", "Clipper", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("clipperCeiling", "Clipper Ceiling", 0.1f, 1.0f, 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("clipperType", "Clipper Type", 
        juce::StringArray{"Hard", "Quintic", "Cubic", "Tangent", "Algebraic", "Arctangent"}, 1)); // Default: Quintic
    
    // Sensitivity removed - STA/LTA is automatic!
    // Mix parameter
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 100.0f, 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoGainComp", "Auto Gain Comp", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("resetToDefaults", "Reset to Defaults", false));
    
    return { params.begin(), params.end() };
}

void AtakAtakAudioProcessor::updateParameters()
{
    // Update gain processors
    float inputGainLinear = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("inputGain")->load());
    float outputGainLinear = juce::Decibels::decibelsToGain(parameters.getRawParameterValue("outputGain")->load());
    
    inputGainProcessor->setGainLinear(inputGainLinear);
    outputGainProcessor->setGainLinear(outputGainLinear);
    
    // Update transient designer parameters
    transientDesigner->setAttackAmount(parameters.getRawParameterValue("attackAmount")->load());
    transientDesigner->setSustainAmount(parameters.getRawParameterValue("sustainAmount")->load());
    transientDesigner->setAttackTime(parameters.getRawParameterValue("attackTime")->load());
    transientDesigner->setReleaseTime(parameters.getRawParameterValue("releaseTime")->load());
    transientDesigner->setAttackThreshold(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("attackThreshold")->load()));
    transientDesigner->setSustainThreshold(juce::Decibels::decibelsToGain(parameters.getRawParameterValue("sustainThreshold")->load()));
    // Sensitivity removed - STA/LTA is automatic!
    transientDesigner->setMix(parameters.getRawParameterValue("mix")->load() / 100.0f);
    
    // Update psychoacoustic parameters
    transientDesigner->setMaskingThreshold(parameters.getRawParameterValue("maskingThreshold")->load());
    transientDesigner->setCriticalBandWeight(parameters.getRawParameterValue("criticalBandWeight")->load());
    transientDesigner->setTemporalWeight(parameters.getRawParameterValue("temporalWeight")->load());
    
    // Update SPL Differential Envelope parameters
    float fastAttackMs = parameters.getRawParameterValue("fastAttackMs")->load();
    float slowAttackMs = parameters.getRawParameterValue("slowAttackMs")->load();
    float releaseMs = parameters.getRawParameterValue("releaseMs")->load();
    float powerMemoryMs = parameters.getRawParameterValue("powerMemoryMs")->load();
    // SPL Mode removed - STA/LTA is automatic!
    // STA/LTA parameters are built-in - no setup needed!
    
    // Update SNAP enhancement parameters
    float snapAmount = parameters.getRawParameterValue("snapAmount")->load();
    float snapHardness = parameters.getRawParameterValue("snapHardness")->load();
    float harmonicEnhancement = parameters.getRawParameterValue("harmonicEnhancement")->load();
    
    transientDesigner->setSnapAmount(snapAmount);
    transientDesigner->setSnapHardness(snapHardness);
    transientDesigner->setHarmonicEnhancement(harmonicEnhancement);
    
    // Update DrumSnapper-inspired parameters
    float focus = parameters.getRawParameterValue("focus")->load();
    float hfGain = parameters.getRawParameterValue("hfGain")->load();
    float hfSaturation = parameters.getRawParameterValue("hfSaturation")->load();
    bool tapeClip = parameters.getRawParameterValue("tapeClip")->load();
    
    transientDesigner->setFocus(focus);
    transientDesigner->setHFGain(hfGain);
    transientDesigner->setHFSaturation(hfSaturation);
    transientDesigner->setTapeClip(tapeClip);
    
    // Update PeakEater-style Clipper parameters
    bool clipperEnabled = parameters.getRawParameterValue("clipperEnabled")->load();
    float clipperCeiling = parameters.getRawParameterValue("clipperCeiling")->load();
    int clipperTypeIndex = static_cast<int>(parameters.getRawParameterValue("clipperType")->load());
    
    transientDesigner->setClipperEnabled(clipperEnabled);
    transientDesigner->setClipperCeiling(clipperCeiling);
    transientDesigner->setClipperType(static_cast<ClipperType>(clipperTypeIndex));
    
    // Update Auto Gain Compensation
    bool autoGainComp = parameters.getRawParameterValue("autoGainComp")->load();
    transientDesigner->setAutoGainComp(autoGainComp);
    
    // Handle reset to defaults
    bool resetToDefaults = parameters.getRawParameterValue("resetToDefaults")->load();
    if (resetToDefaults) {
        resetAllParametersToDefaults();
        // Reset the button back to false
        parameters.getRawParameterValue("resetToDefaults")->store(0.0f);
    }
    
    // Debug output (remove in production)
    static int debugCounter = 0;
    if (++debugCounter % 44100 == 0) { // Once per second at 44.1kHz
        std::cout << "STA/LTA Mode: AUTOMATIC"
                  << ", Fast Attack: " << fastAttackMs 
                  << ", Slow Attack: " << slowAttackMs
                  << ", Release: " << releaseMs
                  << ", Power Memory: " << powerMemoryMs << std::endl;
    }
}

void AtakAtakAudioProcessor::resetAllParametersToDefaults()
{
    // Reset all parameters to their default values
    parameters.getRawParameterValue("inputGain")->store(0.0f);
    parameters.getRawParameterValue("outputGain")->store(0.0f);
    
    parameters.getRawParameterValue("attackAmount")->store(0.0f);
    parameters.getRawParameterValue("attackTime")->store(1.0f);
    parameters.getRawParameterValue("attackThreshold")->store(-40.0f);
    
    parameters.getRawParameterValue("sustainAmount")->store(0.0f);
    parameters.getRawParameterValue("releaseTime")->store(100.0f);
    parameters.getRawParameterValue("sustainThreshold")->store(-40.0f);
    
    parameters.getRawParameterValue("maskingThreshold")->store(-15.0f);
    parameters.getRawParameterValue("criticalBandWeight")->store(1.0f);
    parameters.getRawParameterValue("temporalWeight")->store(1.0f);
    
    parameters.getRawParameterValue("fastAttackMs")->store(1.0f);
    parameters.getRawParameterValue("slowAttackMs")->store(15.0f);
    parameters.getRawParameterValue("releaseMs")->store(20.0f);
    parameters.getRawParameterValue("powerMemoryMs")->store(1.0f);
    // SPL Mode removed - STA/LTA is automatic!
    
    parameters.getRawParameterValue("snapAmount")->store(0.0f);
    parameters.getRawParameterValue("snapHardness")->store(1.0f);
    parameters.getRawParameterValue("harmonicEnhancement")->store(0.0f);
    
    parameters.getRawParameterValue("focus")->store(1.0f);
    parameters.getRawParameterValue("hfGain")->store(1.0f);
    parameters.getRawParameterValue("hfSaturation")->store(0.0f);
    parameters.getRawParameterValue("tapeClip")->store(0.0f);
    
    // Reset PeakEater-style Clipper parameters
    parameters.getRawParameterValue("clipperEnabled")->store(0.0f);
    parameters.getRawParameterValue("clipperCeiling")->store(0.8f);
    parameters.getRawParameterValue("clipperType")->store(1.0f); // Quintic
    
    // Sensitivity removed - STA/LTA is automatic!
    parameters.getRawParameterValue("mix")->store(100.0f);
    parameters.getRawParameterValue("autoGainComp")->store(1.0f);
    parameters.getRawParameterValue("bypass")->store(0.0f);
} 