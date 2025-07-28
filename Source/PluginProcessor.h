#pragma once

#include "../JUCE/modules/juce_audio_processors/juce_audio_processors.h"
#include "../JUCE/modules/juce_dsp/juce_dsp.h"
#include "../JUCE/modules/juce_audio_basics/juce_audio_basics.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Forward declarations
class TransientDesigner;
class GainProcessor;

// PeakEater-inspired Clipper Types (optimized for drums)
enum class ClipperType {
    HARD = 0,
    QUINTIC,
    CUBIC, 
    TANGENT,
    ALGEBRAIC,
    ARCTANGENT
};

//==============================================================================
/**
*/
class AtakAtakAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AtakAtakAudioProcessor();
    ~AtakAtakAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& buses) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter access
    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }
    
    //==============================================================================
    // Parameter layout
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==============================================================================
    void updateParameters();
    void resetAllParametersToDefaults();



    // DSP processors
    std::unique_ptr<GainProcessor> inputGainProcessor;
    std::unique_ptr<GainProcessor> outputGainProcessor;
    std::unique_ptr<TransientDesigner> transientDesigner;
    
    // Parameter tree
    juce::AudioProcessorValueTreeState parameters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtakAtakAudioProcessor)
};

//==============================================================================
// Dual Envelope Transient Detector (based on Envolvigo approach)
// Fast envelope vs Slow envelope - continuous control, no gating!
class DualEnvelopeDetector {
private:
    float fastAttackCoeff, fastReleaseCoeff;
    float slowAttackCoeff, slowReleaseCoeff;
    float fastEnvelope, slowEnvelope;
    
public:
    DualEnvelopeDetector() : fastEnvelope(0.0f), slowEnvelope(0.0f) {}
    
    void prepare(float sampleRate) {
        // Fast envelope: instant attack, 10ms release
        fastAttackCoeff = 0.0f;  // Instant attack
        fastReleaseCoeff = std::exp(-1.0f / (sampleRate * 0.01f));  // 10ms release
        
        // Slow envelope: 50ms attack, 100ms release  
        slowAttackCoeff = std::exp(-1.0f / (sampleRate * 0.05f));   // 50ms attack
        slowReleaseCoeff = std::exp(-1.0f / (sampleRate * 0.1f));   // 100ms release
    }
    
    float process(float input) {
        float absInput = std::abs(input);
        
        // Fast envelope: instant up, slow down
        if (absInput > fastEnvelope) {
            fastEnvelope = absInput;  // Instant attack
        } else {
            fastEnvelope = fastEnvelope * fastReleaseCoeff + absInput * (1.0f - fastReleaseCoeff);
        }
        
        // Slow envelope: slow up, slow down
        if (absInput > slowEnvelope) {
            slowEnvelope = slowEnvelope * slowAttackCoeff + absInput * (1.0f - slowAttackCoeff);
        } else {
            slowEnvelope = slowEnvelope * slowReleaseCoeff + absInput * (1.0f - slowReleaseCoeff);
        }
        
        // Return difference (transient strength) - always >= 0
        return std::max(0.0f, fastEnvelope - slowEnvelope);
    }
    
    void reset() {
        fastEnvelope = slowEnvelope = 0.0f;
    }
    
    // Get individual envelopes for debugging
    float getFastEnvelope() const { return fastEnvelope; }
    float getSlowEnvelope() const { return slowEnvelope; }
};

//==============================================================================
// EnvelopeFollower from compendium
class EnvelopeFollower {
private:
    float attack_coeff, release_coeff;
    float envelope;
    
public:
    EnvelopeFollower() : envelope(0.0f) {}
    
    void set_times(float attack_ms, float release_ms, float sample_rate) {
        attack_coeff = exp(-1.0f / (attack_ms * sample_rate * 0.001f));
        release_coeff = exp(-1.0f / (release_ms * sample_rate * 0.001f));
    }
    
    float process(float input) {
        float input_level = fabs(input);
        
        if (input_level > envelope) {
            envelope = attack_coeff * envelope + (1.0f - attack_coeff) * input_level;
        } else {
            envelope = release_coeff * envelope + (1.0f - release_coeff) * input_level;
        }
        
        return envelope;
    }
    
    void reset() {
        envelope = 0.0f;
    }
};

//==============================================================================
// Simple gain processor
class GainProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        gain.setGainLinear(1.0f);
        gain.prepare(spec);
    }

    void reset()
    {
        gain.reset();
    }

    void setGainLinear(float newGain)
    {
        gain.setGainLinear(newGain);
    }

    template<typename ProcessContext>
    void process(ProcessContext& context)
    {
        gain.process(context);
    }

private:
    juce::dsp::Gain<float> gain;
};

//==============================================================================
// Enhanced Transient Designer with SNAP techniques from compendium
class TransientDesigner
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        numChannels = spec.numChannels;
        
        // Initialize envelope followers for each channel
        attackEnvelopes.clear();
        sustainEnvelopes.clear();
        dualEnvelopeDetectors.clear();
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            attackEnvelopes.emplace_back();
            sustainEnvelopes.emplace_back();
            dualEnvelopeDetectors.emplace_back();
        }
        
        // Initialize Dual Envelope detectors - continuous, no gating!
        for (auto& detector : dualEnvelopeDetectors) {
            detector.prepare(sampleRate);
        }
        
        reset();
    }

    void reset()
    {
        for (auto& env : attackEnvelopes)
            env.reset();
        for (auto& env : sustainEnvelopes)
            env.reset();
        for (auto& detector : dualEnvelopeDetectors)
            detector.reset();
    }

    void setAttackAmount(float amount) { attackAmount = amount; }
    void setSustainAmount(float amount) { sustainAmount = amount; }
    void setAttackTime(float time) { 
        attackTime = time; 
        for (auto& env : attackEnvelopes)
            env.set_times(time, releaseTime, sampleRate);
    }
    void setReleaseTime(float time) { 
        releaseTime = time; 
        for (auto& env : attackEnvelopes)
            env.set_times(attackTime, time, sampleRate);
        for (auto& env : sustainEnvelopes)
            env.set_times(attackTime, time, sampleRate);
    }
    void setAttackThreshold(float threshold) { attackThreshold = threshold; }
    void setSustainThreshold(float threshold) { sustainThreshold = threshold; }
    // Sensitivity removed - STA/LTA is automatic!
    void setMix(float mixAmount) { mix = mixAmount; }
    void setMaskingThreshold(float threshold) { maskingThreshold = threshold; }
    void setCriticalBandWeight(float weight) { criticalBandWeight = weight; }
    void setTemporalWeight(float weight) { temporalWeight = weight; }
    void setSnapAmount(float amount) { snapAmount = amount; }
    void setSnapHardness(float hardness) { snapHardness = hardness; }
    void setHarmonicEnhancement(float enhancement) { harmonicEnhancement = enhancement; }
    
    // DrumSnapper-inspired setters
    void setFocus(float amount) { focus = amount; }
    void setHFGain(float gain) { hfGain = gain; }
    void setHFSaturation(float saturation) { hfSaturation = saturation; }
    void setTapeClip(bool enabled) { tapeClip = enabled; }
    void setAutoGainComp(bool enabled) { autoGainComp = enabled; }
    
    // PeakEater-style Clipper setters
    void setClipperEnabled(bool enabled) { clipperEnabled = enabled; }
    void setClipperCeiling(float ceiling) { clipperCeiling = ceiling; }
    void setClipperDrive(float drive) { clipperDrive = drive; }
    void setClipperType(ClipperType type) { clipperType = type; }
    
    // Dual Envelope is fully automatic - no parameter setup needed!

    template<typename ProcessContext>
    void process(ProcessContext& context)
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        
        // Debug counter for periodic output
        static int debugSampleCounter = 0;
        static bool debugThisBlock = false;
        
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* input = inputBlock.getChannelPointer(ch);
            auto* output = outputBlock.getChannelPointer(ch);
            
            for (int sample = 0; sample < inputBlock.getNumSamples(); ++sample)
            {
                float inputSample = input[sample];
                
                // Debug output every 44100 samples (1 second at 44.1kHz)
                if (++debugSampleCounter >= 44100) {
                    debugSampleCounter = 0;
                    debugThisBlock = (ch == 0 && sample == 0); // Only debug first channel, first sample
                }
                
                // 1. DUAL ENVELOPE Transient Detection - CONTINUOUS, NO GATING!
                float transientDetected = dualEnvelopeDetectors[ch].process(inputSample);
                
                // For envelope followers, use traditional method for attack/sustain shaping
                float attackEnv = attackEnvelopes[ch].process(inputSample);
                float sustainEnv = sustainEnvelopes[ch].process(inputSample);
                
                if (debugThisBlock) {
                    std::cout << "=== DUAL ENVELOPE MODE ===" << std::endl;
                    std::cout << "Input: " << inputSample << ", Transient: " << transientDetected 
                              << ", Fast: " << dualEnvelopeDetectors[ch].getFastEnvelope()
                              << ", Slow: " << dualEnvelopeDetectors[ch].getSlowEnvelope() << std::endl;
                }
                
                // 4. Calculate gains
                float attackGain = 1.0f;
                float sustainGain = 1.0f;
                
                if (attackAmount > 0.0f)
                {
                    // Boost attack - sensitivity only affects detection, not gain!
                    float attackScale = (attackAmount / 100.0f); // Remove sensitivity from gain calculation
                    attackGain = 1.0f + attackScale * 3.0f * transientDetected; // Stronger base effect
                }
                else if (attackAmount < 0.0f)
                {
                    // Reduce attack - sensitivity only affects detection, not gain!
                    float attackReduction = (-attackAmount / 100.0f); // Remove sensitivity from gain calculation
                    attackGain = 1.0f / (1.0f + attackReduction * 3.0f * transientDetected); // Stronger base effect
                }
                
                if (sustainAmount > 0.0f)
                {
                    // Boost sustain - sensitivity only affects detection, not gain!
                    float sustainScale = (sustainAmount / 100.0f); // Remove sensitivity from gain calculation
                    sustainGain = 1.0f + sustainScale * 3.0f; // Stronger base effect
                }
                else if (sustainAmount < 0.0f)
                {
                    // Reduce sustain - sensitivity only affects detection, not gain!
                    float sustainReduction = (-sustainAmount / 100.0f); // Remove sensitivity from gain calculation
                    sustainGain = 1.0f / (1.0f + sustainReduction * 3.0f); // Stronger base effect
                }
                
                if (debugThisBlock) {
                    std::cout << "Attack Amount: " << attackAmount << " -> Gain: " << attackGain << std::endl;
                    std::cout << "Sustain Amount: " << sustainAmount << " -> Gain: " << sustainGain << std::endl;
                    std::cout << "Dual Envelope Continuous, TransientDetected: " << transientDetected << std::endl;
                }
                
                // 5. Apply psychoacoustic weighting
                float psychoacousticWeight = 1.0f + (criticalBandWeight - 1.0f) * transientDetected;
                attackGain *= psychoacousticWeight;
                sustainGain *= (1.0f + (temporalWeight - 1.0f) * (1.0f - transientDetected));
                
                // 6. Apply SNAP processing (Variable Hardness Waveshaper from compendium) - SAFE
                float snapGain = 1.0f;
                if (snapAmount > 0.0f) {
                    // Normalize transientDetected to reasonable range [0,1] for dual envelope
                    float normalizedTransient = std::min(1.0f, transientDetected * 5.0f); // Less aggressive scaling
                    
                    // More conservative SNAP input to prevent overdriving
                    float snapInput = snapAmount / 100.0f * (0.2f + normalizedTransient * 0.5f); // Base 20% + smaller boost
                    snapGain = processSnapWaveshaper(snapInput);
                    
                    // Limit SNAP gain to prevent excessive boost
                    snapGain = std::min(2.0f, snapGain); // Max 2x gain
                }
                attackGain *= snapGain;
                
                // Apply SNAP to sustain as well for stronger effect - LIMITED
                float sustainSnapGain = 1.0f;
                if (snapAmount > 0.0f) {
                    float sustainSnapInput = snapAmount / 300.0f; // Even gentler for sustain
                    sustainSnapGain = processSnapWaveshaper(sustainSnapInput);
                    
                    // Limit sustain SNAP gain even more
                    sustainSnapGain = std::min(1.3f, sustainSnapGain); // Max 30% boost for sustain
                }
                sustainGain *= sustainSnapGain;
                
                if (debugThisBlock && snapAmount > 0.0f) {
                    float normalizedTransient = std::min(1.0f, transientDetected * 10.0f);
                    float snapInput = snapAmount / 100.0f * (0.3f + normalizedTransient * 0.7f);
                    std::cout << "SNAP Debug: Amount=" << snapAmount 
                              << ", TransientDetected=" << transientDetected 
                              << ", NormalizedTransient=" << normalizedTransient
                              << ", SnapInput=" << snapInput
                              << ", SnapGain=" << snapGain << std::endl;
                }
                
                // 7. Apply harmonic enhancement (Neve transformer style from compendium) - 3x stronger
                if (harmonicEnhancement > 0.0f) {
                    // Always apply harmonic enhancement when > 0, but scale with transient detection
                    float harmonicScale = 0.1f + transientDetected * 0.9f;
                    float harmonicContent = attackGain * attackGain * 0.3f; // 3x stronger
                    attackGain += harmonicContent * harmonicEnhancement * 0.06f * harmonicScale; // 3x stronger
                }
                
                // Apply harmonic enhancement to sustain as well - 3x stronger
                if (harmonicEnhancement > 0.0f) { // Remove sustainAmount > 0.0f condition - Harmonics should work independently
                    float sustainHarmonicContent = sustainGain * sustainGain * 0.15f; // 3x stronger for sustain
                    sustainGain += sustainHarmonicContent * harmonicEnhancement * 0.03f; // 3x stronger
                }
                
                // 8. Apply Focus (DrumSnapper-inspired) - sharpen attack 
                if (focus > 1.0f) {
                    attackGain *= focus;
                }
                
                // 8.5. SAFETY LIMITING - prevent extreme values
                attackGain = std::max(0.1f, std::min(5.0f, attackGain));   // Limit attack gain
                sustainGain = std::max(0.1f, std::min(3.0f, sustainGain)); // Limit sustain gain
                
                if (debugThisBlock) {
                    std::cout << "Final Attack Gain: " << attackGain << ", Final Sustain Gain: " << sustainGain << std::endl;
                    std::cout << "Focus: " << focus << ", HF Saturation: " << hfSaturation 
                              << ", Tape Clip: " << (tapeClip ? "ON" : "OFF") << std::endl;
                }
                
                // 9. Apply processing - DrumSnapper-style mixing for stronger effects
                float attackComponent, sustainComponent;
                
                if (attackAmount > 0.0f) {
                    // Safe exponential gain for attack (like DrumSnapper)
                    float expValue = std::max(-5.0f, std::min(5.0f, (attackGain - 1.0f))); // Clamp to safe range!
                    float expGain = powf(2.0f, expValue);
                    attackComponent = ((inputSample * expGain) - inputSample) * 2.0f; // Back to 2x for safety
                } else if (attackAmount < 0.0f) {
                    // Reduce attack - apply to all detected transients, not just strong ones
                    attackComponent = inputSample * attackGain;
                } else {
                    attackComponent = 0.0f;
                }
                
                // Sustain component (always present)
                sustainComponent = inputSample * sustainGain;
                
                // 10. Physical Sustain Shaping (multiband-transient-shaper style)
                // sustainEnv = 1 - attackEnv for physical decay control
                float physicalSustainEnv = 1.0f - transientDetected;
                
                // Apply sustain amount to envelope shape (not just gain)
                if (sustainAmount < 0.0f) {
                    // Negative sustain: physically shorten the decay
                    float sustainReduction = (-sustainAmount / 100.0f); // 0 to 1
                    physicalSustainEnv = std::pow(physicalSustainEnv, 1.0f + sustainReduction * 3.0f); // Exponential decay
                }
                
                // 11. Mix based on transient detection - with physical sustain shaping
                float processedSample;
                if (transientDetected > 0.05f) { // Lower threshold - attack works more often!
                    // During transients: add attack to sustain
                    float attackMix = std::min(1.0f, transientDetected * 2.0f); // More aggressive attack mixing
                    processedSample = sustainComponent + (attackComponent * attackMix);
                } else {
                    // During sustain: apply physical envelope shaping BUT keep sustain gain!
                    float finalSustainComponent = sustainComponent; // Keep the gain effect
                    
                    // Apply physical shaping only for negative sustain
                    if (sustainAmount < 0.0f) {
                        finalSustainComponent *= physicalSustainEnv; // Physical shortening
                    }
                    
                    processedSample = finalSustainComponent;
                }
                
                // 11. Apply HF Saturation (DrumSnapper-inspired)
                if (hfSaturation > 0.0f) {
                    // Simple high-frequency enhancement
                    float hfContent = processedSample * processedSample * hfGain;
                    processedSample += hfContent * (hfSaturation / 100.0f) * 0.3f;
                }
                
                // 12. Apply Tape Clipper (DrumSnapper-inspired)
                if (tapeClip) {
                    processedSample = processTapeClipper(processedSample);
                }
                
                // 13. Apply mix control with safety limiting
                float mixedSample = inputSample * (1.0f - mix) + processedSample * mix;
                
                // 13.5. FINAL SAFETY LIMITING - prevent clipping
                mixedSample = std::max(-2.0f, std::min(2.0f, mixedSample)); // Hard limit to prevent extreme values
                
                // 14. Automatic Gain Compensation (Pirkle style)
                if (autoGainComp) {
                    // Calculate input and output RMS for gain compensation
                    inputRMS = rmsCoeff * inputRMS + (1.0f - rmsCoeff) * (inputSample * inputSample);
                    float tempOutputRMS = rmsCoeff * outputRMS + (1.0f - rmsCoeff) * (mixedSample * mixedSample);
                    outputRMS = tempOutputRMS;
                    
                    // Calculate makeup gain to match input level
                    float makeupGain = 1.0f;
                    if (outputRMS > 1e-10f && inputRMS > 1e-10f) {
                        makeupGain = std::sqrt(inputRMS / outputRMS);
                        makeupGain = std::max(0.1f, std::min(3.0f, makeupGain)); // Limit makeup gain
                    }
                    
                    mixedSample *= makeupGain;
                }
                
                // 15. Apply PeakEater-style Clipper (TRUE FINAL STAGE - like PeakEater!)
                if (clipperEnabled) {
                    mixedSample = processClipper(mixedSample, clipperCeiling, clipperDrive, clipperType);
                }
                
                output[sample] = mixedSample;
                
                if (debugThisBlock) {
                    std::cout << "Attack Component: " << attackComponent << ", Sustain Component: " << sustainComponent << std::endl;
                    std::cout << "Physical Sustain Env: " << physicalSustainEnv << std::endl;
                    std::cout << "Processed Sample: " << processedSample << ", Final Output: " << output[sample] << std::endl;
                    std::cout << "Mix: " << mix << ", Auto Gain Comp: " << (autoGainComp ? "ON" : "OFF") << std::endl;
                    if (autoGainComp) {
                        std::cout << "Input RMS: " << std::sqrt(inputRMS) << ", Output RMS: " << std::sqrt(outputRMS) << std::endl;
                    }
                    std::cout << "===================" << std::endl;
                    debugThisBlock = false; // Only debug once per second
                }
            }
        }
    }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;
    
    // Envelope followers from compendium
    std::vector<EnvelopeFollower> attackEnvelopes;
    std::vector<EnvelopeFollower> sustainEnvelopes;
    
    // SPL Differential Envelope followers
    std::vector<DualEnvelopeDetector> dualEnvelopeDetectors;
    
    // Parameters
    float attackAmount = 0.0f;
    float sustainAmount = 0.0f;
    float attackTime = 1.0f;
    float releaseTime = 100.0f;
    float attackThreshold = 0.1f;
    float sustainThreshold = 0.1f;
    // sensitivity removed - STA/LTA is automatic!
    float mix = 1.0f;
    float maskingThreshold = -15.0f;
    float criticalBandWeight = 1.0f;
    float temporalWeight = 1.0f;
    
    // Dual Envelope parameters are built into the detector - no external config needed!
    
    // SNAP Enhancement Parameters
    float snapAmount = 0.0f;
    float snapHardness = 1.0f;
    float harmonicEnhancement = 0.0f;
    
    // DrumSnapper-inspired Parameters
    float focus = 1.0f;
    float hfGain = 1.0f;
    float hfSaturation = 0.0f;
    bool tapeClip = false;
    
    // PeakEater-style Clipper Parameters (Final Stage)
    bool clipperEnabled = false;
    float clipperCeiling = 0.8f; // Linear gain (0.0 to 1.0)
    float clipperDrive = 2.0f; // Drive intensity (1.0 to 10.0)
    ClipperType clipperType = ClipperType::QUINTIC; // Default: great for drums
    
    // Automatic Gain Compensation
    bool autoGainComp = true;
    float inputRMS = 0.0f;
    float outputRMS = 0.0f;
    float rmsCoeff = 0.999f; // Smoothing coefficient for RMS calculation
    
    // Variable Hardness Waveshaper from compendium - CONTROLLED SATURATION
    float processSnapWaveshaper(float input) {
        if (input == 0.0f) return 1.0f; // No change for zero input
        
        float absInput = std::abs(input);
        
        // Controlled saturation with proper limiting
        if (absInput < 0.1f) {
            // For small values, gentle boost
            float boost = absInput * snapHardness * 0.5f; // Gentler
            return 1.0f + boost;
        } else if (absInput < 0.5f) {
            // For medium values, progressive saturation
            float normalized = absInput / 0.5f; // 0-1 range
            float saturation = normalized * normalized * snapHardness * 0.3f; // Much gentler
            return 1.0f + saturation;
        } else {
            // For large values, soft limiting instead of hard saturation
            float excess = absInput - 0.5f;
            float softLimit = excess / (1.0f + excess * snapHardness); // Soft limiting
            return 1.0f + 0.3f + softLimit * 0.2f; // Max ~1.5x gain
        }
    }



    float processClipper(float input, float ceiling, float drive, ClipperType type) {
        // PeakEater-style clipper: Drive is input gain, Ceiling is threshold
        float drivenInput = input * drive; // Apply drive as input gain
        float absInput = std::abs(drivenInput);
        
        if (absInput <= ceiling) return drivenInput; // No clipping needed
        
        float sign = (drivenInput >= 0.0f) ? 1.0f : -1.0f;
        float normalizedInput = absInput / ceiling; // Normalize to ceiling for clipping algorithms
        float clippedValue = 0.0f;
        
        switch (type) {
            case ClipperType::HARD:
                clippedValue = 1.0f; // Hard clip at ceiling
                break;
                
            case ClipperType::QUINTIC: // Great for drums - smooth but punchy
                clippedValue = normalizedInput - (1.0f/5.0f) * std::pow(normalizedInput, 5.0f);
                clippedValue = std::min(1.0f, clippedValue);
                break;
                
            case ClipperType::CUBIC: // Warm saturation for cymbals
                clippedValue = normalizedInput - (1.0f/3.0f) * std::pow(normalizedInput, 3.0f);
                clippedValue = std::min(1.0f, clippedValue);
                break;
                
            case ClipperType::TANGENT: // Musical saturation
                clippedValue = std::tanh(normalizedInput * 0.7f) / std::tanh(0.7f);
                break;
                
            case ClipperType::ALGEBRAIC: // Smooth limiting
                clippedValue = normalizedInput / std::sqrt(1.0f + normalizedInput * normalizedInput);
                break;
                
            case ClipperType::ARCTANGENT: // Subtle enhancement
                clippedValue = (2.0f / M_PI) * std::atan(normalizedInput * M_PI * 0.5f);
                break;
        }
        
        // Return clipped output at ceiling level (PeakEater style)
        return sign * clippedValue * ceiling;
    }
    
    // Tape Clipper from DrumSnapper
    float processTapeClipper(float sample) {
        float x = sample;
        float s = juce::jlimit<float>(-0.95f, 0.95f, tanhf(powf(x, 5) + x) * 0.95f);
        return s;
    }
}; 