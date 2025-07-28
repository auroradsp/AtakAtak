cmake_minimum_required(VERSION 3.15)

project(AtakAtak VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE subdirectory
add_subdirectory(JUCE)

# Create plugin
juce_add_plugin(AtakAtak
    COMPANY_NAME "DigitalAudioProcessing"
    COMPANY_WEBSITE "https://github.com/DigitalAudioProcessing"
    COMPANY_EMAIL "info@digitalaudioprocessing.com"
    PLUGIN_MANUFACTURER_CODE Atak
    PLUGIN_CODE Atak
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "AtakAtak"
    COMPANY_WEBSITE "https://github.com/DigitalAudioProcessing"
    COMPANY_EMAIL "info@digitalaudioprocessing.com"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_CHANNEL_CONFIGURATIONS "1,1" "2,2"
    VST3_CATEGORIES "Fx"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

# Add source files
target_sources(AtakAtak
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

# JUCE modules
target_compile_definitions(AtakAtak
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AtakAtak
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Set output directory
set_target_properties(AtakAtak PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
) 