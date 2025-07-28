#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AtakAtakAudioProcessorEditor::AtakAtakAudioProcessorEditor (AtakAtakAudioProcessor& p)
    : GenericAudioProcessorEditor (p)
{
    // GenericAudioProcessorEditor automatically creates UI for all parameters
    // and sets appropriate size
}

AtakAtakAudioProcessorEditor::~AtakAtakAudioProcessorEditor()
{
} 