#pragma once

#include "../JUCE/modules/juce_audio_processors/juce_audio_processors.h"
#include "../JUCE/modules/juce_gui_basics/juce_gui_basics.h"
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AtakAtakAudioProcessorEditor  : public juce::GenericAudioProcessorEditor
{
public:
    AtakAtakAudioProcessorEditor (AtakAtakAudioProcessor&);
    ~AtakAtakAudioProcessorEditor() override;

private:
    // GenericAudioProcessorEditor handles everything automatically

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtakAtakAudioProcessorEditor)
}; 