# AtakAtak - Psychoacoustic Transient Designer

A professional audio plugin for transient shaping with psychoacoustic processing capabilities.

## Features

- **Input/Output Gain Control**: -24dB to +24dB
- **Attack Processing**: Amount (-100% to +100%), Time (0.1ms to 100ms), Threshold (-60dB to 0dB)
- **Sustain Processing**: Amount (-100% to +100%), Release Time (1ms to 1000ms), Threshold (-60dB to 0dB)
- **Psychoacoustic Parameters**: Masking Threshold, Critical Band Weight, Temporal Weight
- **Control Parameters**: Sensitivity, Mix, Bypass
- **Format Support**: VST3, AU, Standalone

## Building

### Prerequisites

- CMake 3.15 or higher
- Ninja build system
- JUCE 8.0.8

### Build Commands

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -G Ninja -DJUCE_BUILD_EXAMPLES=OFF -DJUCE_BUILD_EXTRAS=OFF ..

# Build
ninja
```

### Build Options

- `-DJUCE_BUILD_EXAMPLES=OFF`: Skip building JUCE examples
- `-DJUCE_BUILD_EXTRAS=OFF`: Skip building JUCE extras

## Plugin Structure

```
AtakAtak/
├── CMakeLists.txt          # Main build configuration
├── Source/
│   ├── PluginProcessor.h   # Main processor header
│   ├── PluginProcessor.cpp # Main processor implementation
│   ├── PluginEditor.h      # UI header
│   └── PluginEditor.cpp    # UI implementation
└── README.md              # This file
```

## Parameter Layout

### Top Section
- **Input Gain**: -24dB to +24dB
- **Output Gain**: -24dB to +24dB
- **Bypass Button**: Enable/disable processing

### Middle Section (3 rows of 3 knobs each)

**Row 1 - Attack Parameters:**
- Attack Amount: -100% to +100%
- Attack Time: 0.1ms to 100ms
- Attack Threshold: -60dB to 0dB

**Row 2 - Sustain Parameters:**
- Sustain Amount: -100% to +100%
- Release Time: 1ms to 1000ms
- Sustain Threshold: -60dB to 0dB

**Row 3 - Psychoacoustic Parameters:**
- Masking Threshold: -30dB to 0dB
- Critical Band Weight: 0.0 to 2.0
- Temporal Weight: 0.0 to 2.0

### Bottom Section
- **Sensitivity**: 0.1 to 10.0
- **Mix**: 0% to 100%

## Development Status

- ✅ Basic plugin structure
- ✅ Parameter system with AudioProcessorValueTreeState
- ✅ UI layout with 3x3 knob arrangement
- ✅ Input/Output gain controls
- ✅ Bypass functionality
- 🔄 Transient Designer DSP (placeholder)
- 🔄 Psychoacoustic processing (placeholder)

## Next Steps

1. Implement TransientDesigner DSP class
2. Add psychoacoustic processing algorithms
3. Implement parameter automation
4. Add preset system
5. Add metering and visualization
6. Optimize for real-time performance

## License

This project is part of the DigitalAudioProcessing educational series. 