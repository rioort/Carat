#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "CaratDsp.h"

class CaratAudioProcessor : public juce::AudioProcessor
{
public:
    CaratAudioProcessor();
    ~CaratAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Saturazione: amount in dB (0 = spenta) a partire dai parametri
    struct SatSetting { bool on; float amountDb; };
    static SatSetting satFromParams (int choice, float variableAmount);

    juce::AudioProcessorValueTreeState apvts;

    // Meter letti dall'interfaccia (valori di picco: l'interfaccia li azzera)
    std::atomic<float> meterInPeakDb[2] { { -100.0f }, { -100.0f } }, meterOutPeakDb[2] { { -100.0f }, { -100.0f } };
    std::atomic<float> meterClipDb { 0.0f }, meterTpLimitDb { 0.0f };
    // Valori continui
    std::atomic<float> lufsMomentary { -100.0f }, lufsShort { -100.0f }, lufsIntegrated { -100.0f };
    std::atomic<float> truePeakMaxDb { -100.0f }, loudnessGainDb { 0.0f };
    std::atomic<bool> resetMeters { false };
    // Diagnostica: canali che l'host sta effettivamente usando
    std::atomic<int> hostInChannels { 0 }, hostOutChannels { 0 }, hostBufferChannels { 0 };

private:
    void processChunk (juce::AudioBuffer<float>& mainBus, int numCh, int n);

    carat::ConverterStage converter;
    carat::BandLimiter bandLimiter;
    carat::TruePeakLimiter limiter;
    carat::LoudnessMeter inLoudness, outLoudness;
    std::array<carat::TruePeakEstimator, 2> tpDisplay;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    juce::AudioBuffer<float> dryBuffer;
    std::vector<std::vector<float>> dryDelay;
    int dryDelayPos = 0, totalLatency = 0, maxBlock = 0;

    // smoothing a sample rate base
    float smoothCoeff = 0.0f, matchCoeff = 0.0f;
    float sCeiling = 1.0f, sRef = 1.0f, sBypass = 0.0f, sDelta = 0.0f, sMatch = 1.0f;
    float sMidL = 1.0f, sMidR = 0.0f, sSide = 0.0f; // matrice di ascolto
    float tpMaxLin = 0.0f;

    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pSat = nullptr;
    std::atomic<float>* pSatAmt = nullptr;
    std::atomic<float>* pCeiling = nullptr;
    std::atomic<float>* pTpLimit = nullptr;
    std::atomic<float>* pListen = nullptr;
    std::atomic<float>* pDelta = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pMatch = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CaratAudioProcessor)
};
