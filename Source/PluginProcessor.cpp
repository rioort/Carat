#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::String input = "input", sat = "sat", satAmt = "satamt", ceiling = "ceiling",
        tpLimit = "tplimit", listen = "listen", delta = "delta", bypass = "bypass", match = "match";
}

//==============================================================================
CaratAudioProcessor::CaratAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CaratState", createLayout())
{
    pInput   = apvts.getRawParameterValue (ids::input);
    pSat     = apvts.getRawParameterValue (ids::sat);
    pSatAmt  = apvts.getRawParameterValue (ids::satAmt);
    pCeiling = apvts.getRawParameterValue (ids::ceiling);
    pTpLimit = apvts.getRawParameterValue (ids::tpLimit);
    pListen  = apvts.getRawParameterValue (ids::listen);
    pDelta   = apvts.getRawParameterValue (ids::delta);
    pBypass  = apvts.getRawParameterValue (ids::bypass);
    pMatch   = apvts.getRawParameterValue (ids::match);
}

CaratAudioProcessor::SatSetting CaratAudioProcessor::satFromParams (int choice, float variableAmount)
{
    switch (choice)
    {
        case 1:  return { true, 3.0f };
        case 2:  return { true, 6.0f };
        case 3:  return { true, variableAmount };
        default: return { false, 3.0f };
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout CaratAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbText = [] (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::input, 1 }, "Input",
        NormalisableRange<float> (-12.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::sat, 1 }, "Soft Saturation",
        StringArray { "Off", "+3 dB", "+6 dB", "Variable" }, 0));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::satAmt, 1 }, "Saturation Amount",
        NormalisableRange<float> (1.0f, 9.0f, 0.1f), 4.5f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ceiling, 1 }, "Ceiling",
        NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -1.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::tpLimit, 1 }, "True Peak Limit", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::listen, 1 }, "Listen",
        StringArray { "Stereo", "Mid", "Side" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::delta, 1 }, "Delta", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::match, 1 }, "Level Match", true));

    return { p.begin(), p.end() };
}

//==============================================================================
bool CaratAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Plugin da mastering: solo stereo -> stereo. Cosi' nessun host puo'
    // caricarlo in una configurazione mono o mista.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CaratAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (1, samplesPerBlock);

    // Oversampling 8x, filtri FIR a fase lineare: il clip e la saturazione
    // lavorano lontano dalla frequenza di Nyquist (niente aliasing udibile).
    // Sempre preparato per 2 canali, qualunque cosa annunci l'host.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) 2, 3,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();

    converter.prepare (sampleRate * (double) oversampling->getOversamplingFactor());
    bandLimiter.reset();
    limiter.prepare (sampleRate);
    inLoudness.prepare (sampleRate);
    outLoudness.prepare (sampleRate);
    for (auto& t : tpDisplay) t.reset();

    totalLatency = juce::roundToInt (oversampling->getLatencyInSamples()) + carat::BandLimiter::latency
                   + limiter.getLatency();
    setLatencySamples (totalLatency);

    dryBuffer.setSize (2, maxBlock);
    dryDelay.assign (2, std::vector<float> ((size_t) totalLatency + 1, 0.0f));
    dryDelayPos = 0;

    smoothCoeff = carat::onePoleCoeff (0.02, sampleRate);
    matchCoeff = carat::onePoleCoeff (0.5, sampleRate);
    sCeiling = carat::dbToLin (pCeiling->load());
    sBypass = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    sDelta = pDelta->load() > 0.5f ? 1.0f : 0.0f;
    sMatch = 1.0f;
    {
        const auto s = satFromParams ((int) pSat->load(), pSatAmt->load());
        sRef = carat::dbToLin (pInput->load()) * (s.on ? carat::SoftSat::makeupFor (s.amountDb) : 1.0f) * sCeiling;
    }
    tpMaxLin = 0.0f;
}

void CaratAudioProcessor::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
}

void CaratAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    hostInChannels.store (getTotalNumInputChannels());
    hostOutChannels.store (getTotalNumOutputChannels());
    hostBufferChannels.store (buffer.getNumChannels());

    // canali in piu' (se l'host ne passa) vengono azzerati
    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    if (numCh == 0 || n == 0 || oversampling == nullptr) return;

    if (resetMeters.exchange (false))
    {
        inLoudness.reset();
        outLoudness.reset();
        tpMaxLin = 0.0f;
        truePeakMaxDb.store (-100.0f);
    }

    // Blocchi piu' grandi del previsto vengono divisi
    for (int start = 0; start < n; start += maxBlock)
    {
        const int len = juce::jmin (maxBlock, n - start);
        juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), numCh, start, len);
        processChunk (sub, numCh, len);
    }
}

void CaratAudioProcessor::processChunk (juce::AudioBuffer<float>& io, int numCh, int n)
{
    // --- parametri
    const float drive = carat::dbToLin (pInput->load());
    const auto sat = satFromParams ((int) pSat->load(), pSatAmt->load());
    const float ceilingTarget = carat::dbToLin (pCeiling->load());
    const bool tpOn = pTpLimit->load() > 0.5f;
    const int listen = (int) pListen->load();
    const float deltaTarget = pDelta->load() > 0.5f ? 1.0f : 0.0f;
    const float bypassTarget = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    const bool matchOn = pMatch->load() > 0.5f;

    // --- ingresso: pulizia, copia dry, meter (per canale)
    float inPeak[2] = { 0.0f, 0.0f };
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = io.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            d[i] = carat::sanitize (d[i]);
            inPeak[c] = juce::jmax (inPeak[c], std::abs (d[i]));
        }
        dryBuffer.copyFrom (c, 0, io, c, 0, n);
    }
    {
        const float* dp[2] = { dryBuffer.getReadPointer (0), dryBuffer.getReadPointer (numCh > 1 ? 1 : 0) };
        inLoudness.process (dp, numCh, n);
    }

    // --- convertitore (clip + soft saturation) in oversampling 8x
    float clipMax = 0.0f;
    {
        juce::dsp::AudioBlock<float> block (io.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto os = oversampling->processSamplesUp (block);
        float* osPtr[2] = { os.getChannelPointer (0), os.getChannelPointer (numCh > 1 ? 1 : 0) };
        converter.setTargets (drive, sat.amountDb, sat.on);
        const int osN = (int) os.getNumSamples();
        for (int i = 0; i < osN; ++i)
            clipMax = juce::jmax (clipMax, converter.processFrame (osPtr, numCh, i));
        oversampling->processSamplesDown (block);
    }

    // --- filtro anti-Nyquist, ceiling, limiter True Peak
    float* wet[2] = { io.getWritePointer (0), io.getWritePointer (numCh > 1 ? 1 : 0) };
    bandLimiter.process (wet, numCh, n);
    for (int i = 0; i < n; ++i)
    {
        sCeiling = ceilingTarget + (sCeiling - ceilingTarget) * smoothCoeff;
        for (int c = 0; c < numCh; ++c) wet[c][i] *= sCeiling;
    }
    limiter.process (wet, numCh, n, ceilingTarget, tpOn);

    // --- meter dell'uscita elaborata
    outLoudness.process (wet, numCh, n);
    for (int c = 0; c < numCh; ++c)
        for (int i = 0; i < n; ++i)
            tpMaxLin = juce::jmax (tpMaxLin, tpDisplay[(size_t) c].push (wet[c][i]));

    // --- confronto a pari volume: guadagno da dare al segnale originale
    const float inS = inLoudness.getShortTerm(), outS = outLoudness.getShortTerm();
    const bool loudValid = inS > -70.0f && outS > -70.0f;
    const float loudDiff = loudValid ? juce::jlimit (-24.0f, 24.0f, outS - inS) : loudnessGainDb.load();
    const float matchTarget = matchOn ? carat::dbToLin (loudDiff) : 1.0f;

    // guadagno lineare "di riferimento" per il Delta (cio' che il clipper toglie)
    const float refTarget = drive * (sat.on ? carat::SoftSat::makeupFor (sat.amountDb) : 1.0f) * ceilingTarget;

    // matrice di ascolto (Stereo / Mid / Side)
    const float tMidL = listen == 0 ? 1.0f : 0.5f;
    const float tMidR = listen == 0 ? 0.0f : 0.5f;
    const float tSide = listen == 2 ? 1.0f : 0.0f;

    for (int i = 0; i < n; ++i)
    {
        sRef    = refTarget    + (sRef    - refTarget)    * smoothCoeff;
        sDelta  = deltaTarget  + (sDelta  - deltaTarget)  * smoothCoeff;
        sBypass = bypassTarget + (sBypass - bypassTarget) * smoothCoeff;
        sMatch  = matchTarget  + (sMatch  - matchTarget)  * matchCoeff;
        sMidL   = tMidL + (sMidL - tMidL) * smoothCoeff;
        sMidR   = tMidR + (sMidR - tMidR) * smoothCoeff;
        sSide   = tSide + (sSide - tSide) * smoothCoeff;

        const int rd = (dryDelayPos + 1) % (totalLatency + 1);
        float out[2] = { 0.0f, 0.0f };
        for (int c = 0; c < numCh; ++c)
        {
            auto& dl = dryDelay[(size_t) c];
            dl[(size_t) dryDelayPos] = dryBuffer.getSample (c, i);
            const float dry = dl[(size_t) rd];               // allineato alla latenza
            const float w = wet[c][i];
            const float delta = sRef * dry - w;              // solo cio' che viene tolto
            const float processed = w + (delta - w) * sDelta;
            out[c] = processed + (dry * sMatch - processed) * sBypass;
        }
        dryDelayPos = rd;

        if (numCh == 2)
        {
            // Stereo: L,R  |  Mid: (L+R)/2 su entrambi  |  Side: (L-R)/2 su entrambi
            const float L = out[0], R = out[1];
            const float l2 = sMidL * L + sMidR * R;
            const float r2 = sMidR * L + sMidL * R;
            const float side = 0.5f * (L - R);
            out[0] = l2 + (side - l2) * sSide;
            out[1] = r2 + (side - r2) * sSide;
        }
        for (int c = 0; c < numCh; ++c)
            wet[c][i] = juce::jlimit (-4.0f, 4.0f, carat::sanitize (out[c]));
    }

    // --- pubblica i meter (uscita misurata su cio' che esce davvero dal plugin)
    auto storeMax = [] (std::atomic<float>& a, float v) { a.store (juce::jmax (a.load(), v)); };
    for (int c = 0; c < numCh; ++c)
    {
        float outPeak = 0.0f;
        for (int i = 0; i < n; ++i) outPeak = juce::jmax (outPeak, std::abs (wet[c][i]));
        storeMax (meterInPeakDb[c], juce::Decibels::gainToDecibels (inPeak[c], -100.0f));
        storeMax (meterOutPeakDb[c], juce::Decibels::gainToDecibels (outPeak, -100.0f));
    }
    storeMax (meterClipDb, clipMax);
    storeMax (meterTpLimitDb, limiter.takeMaxReductionDb());
    lufsMomentary.store (outLoudness.getMomentary());
    lufsShort.store (outS);
    lufsIntegrated.store (outLoudness.getIntegrated());
    truePeakMaxDb.store (juce::Decibels::gainToDecibels (tpMaxLin, -100.0f));
    if (loudValid) loudnessGainDb.store (loudDiff);
}

//==============================================================================
juce::AudioProcessorEditor* CaratAudioProcessor::createEditor()
{
    return new CaratAudioProcessorEditor (*this);
}

void CaratAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CaratAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CaratAudioProcessor();
}
