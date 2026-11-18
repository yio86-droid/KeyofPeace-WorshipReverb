#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Longitudes de las 8 líneas (ms): valores "irracionales" y crecientes
    // para espacios amplios y sin coincidencias de modos.
    constexpr float kLineMs[8] = { 37.3f, 43.1f, 51.7f, 59.3f, 67.1f, 74.9f, 83.3f, 91.7f };

    // Modulación sutil: profundidad (ms) y velocidad (Hz) distintas por línea
    constexpr float kModMs[8] = { 0.45f, 0.60f, 0.50f, 0.75f, 0.55f, 0.85f, 0.65f, 0.90f };
    constexpr float kLfoHz[8] = { 0.11f, 0.17f, 0.23f, 0.29f, 0.37f, 0.43f, 0.51f, 0.59f };

    constexpr float kMaxLineMs = 91.7f;
    constexpr float kMaxModMs = 0.9f;

    // Patrones Walsh (ortogonales) para decorrelar L y R a la salida
    constexpr float kSignL[8] = { +1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, -1.0f };
    constexpr float kSignR[8] = { +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, -1.0f, +1.0f };

    constexpr float kInputGain = 0.5f;
    constexpr float kOutputGain = 0.35f;
}

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    decayParam    = apvts.getRawParameterValue (ParamIDs::decay);
    preDelayParam = apvts.getRawParameterValue (ParamIDs::preDelay);
    lowCutParam   = apvts.getRawParameterValue (ParamIDs::lowCut);
    highCutParam  = apvts.getRawParameterValue (ParamIDs::highCut);
    mixParam      = apvts.getRawParameterValue (ParamIDs::mix);
}

PluginProcessor::~PluginProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<APF> (
        juce::ParameterID { ParamIDs::decay, 1 }, "Decay Time",
        juce::NormalisableRange<float> (1.5f, 6.0f, 0.01f), 3.5f,
        Attr().withLabel ("s")));

    params.push_back (std::make_unique<APF> (
        juce::ParameterID { ParamIDs::preDelay, 1 }, "Pre-Delay",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f,
        Attr().withLabel ("ms")));

    {
        juce::NormalisableRange<float> r (80.0f, 400.0f, 1.0f);
        r.setSkewForCentre (180.0f);
        params.push_back (std::make_unique<APF> (
            juce::ParameterID { ParamIDs::lowCut, 1 }, "Low-Cut",
            r, 180.0f, Attr().withLabel ("Hz")));
    }
    {
        juce::NormalisableRange<float> r (2000.0f, 12000.0f, 10.0f);
        r.setSkewForCentre (4900.0f);
        params.push_back (std::make_unique<APF> (
            juce::ParameterID { ParamIDs::highCut, 1 }, "High-Cut / Damping",
            r, 6500.0f, Attr().withLabel ("Hz")));
    }

    params.push_back (std::make_unique<APF> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 28.0f,
        Attr().withLabel ("%")));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
    return false;
}

bool PluginProcessor::producesMidi() const
{
    return false;
}

bool PluginProcessor::isMidiEffect() const
{
    return false;
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 8.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
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
bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sr = sampleRate;

    // FDN
    const int maxLine = (int) std::ceil (sr * (kMaxLineMs + kMaxModMs) * 0.001) + 8;
    for (int i = 0; i < kNumLines; ++i)
    {
        lines[i].prepare (maxLine);
        baseDelay[(size_t) i] = kLineMs[i] * 0.001f * (float) sr;
        delaySec[(size_t) i]  = kLineMs[i] * 0.001f;
        modDepth[(size_t) i]  = kModMs[i] * 0.001f * (float) sr;
        lfoInc[(size_t) i]    = 2.0f * kLfoHz[i] / (float) sr;   // el LFO recorre [-1,1) => periodo 2
        lfoPhase[(size_t) i]  = -1.0f + 2.0f * (float) i / (float) kNumLines;
        lpState[(size_t) i]   = 0.0f;
        hpState[(size_t) i]   = 0.0f;
        fbGain[(size_t) i]    = 0.0f;
    }

    // Pre-delay (máx. 100 ms)
    const int maxPre = (int) std::ceil (sr * 0.1) + 8;
    for (auto& d : preDelay)
        d.prepare (maxPre);

    // Difusión de entrada (distintas longitudes en L y R => más ancho)
    const double apMs[2][2] = { { 4.3, 7.1 }, { 4.9, 8.3 } };
    for (int c = 0; c < 2; ++c)
        for (int k = 0; k < 2; ++k)
            diffuser[c][k].prepare ((int) std::lround (apMs[c][k] * 0.001 * sr), 0.6f);

    inLow[0] = inLow[1] = 0.0f;

    // Suavizado de parámetros
    const double rampSec = 0.08;
    decaySm.reset (sr, rampSec);
    decaySm.setCurrentAndTargetValue (decayParam->load());
    preDelaySm.reset (sr, rampSec);
    preDelaySm.setCurrentAndTargetValue (preDelayParam->load());
    lowCutSm.reset (sr, rampSec);
    lowCutSm.setCurrentAndTargetValue (lowCutParam->load());
    highCutSm.reset (sr, rampSec);
    highCutSm.setCurrentAndTargetValue (highCutParam->load());
    mixSm.reset (sr, rampSec);
    mixSm.setCurrentAndTargetValue (mixParam->load());

    updateCoefficients (0);
}

void PluginProcessor::releaseResources()
{
}

//==============================================================================
void PluginProcessor::updateCoefficients (int chunk) noexcept
{
    const float decay  = chunk > 0 ? decaySm.skip (chunk)    : decaySm.getCurrentValue();
    const float preMs  = chunk > 0 ? preDelaySm.skip (chunk) : preDelaySm.getCurrentValue();
    const float lowHz  = chunk > 0 ? lowCutSm.skip (chunk)   : lowCutSm.getCurrentValue();
    float highHz       = chunk > 0 ? highCutSm.skip (chunk)  : highCutSm.getCurrentValue();
    const float mixPct = chunk > 0 ? mixSm.skip (chunk)      : mixSm.getCurrentValue();

    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    const float fs = (float) sr;

    highHz = juce::jmin (highHz, 0.45f * fs);

    lpA = 1.0f - std::exp (-twoPi * highHz / fs);
    hpB = 1.0f - std::exp (-twoPi * lowHz / fs);

    // RT60 -> ganancia por pasada: g = 10^(-3 * delay / RT60)
    const float invRT = -3.0f / juce::jmax (0.1f, decay);
    for (int i = 0; i < kNumLines; ++i)
        fbGain[(size_t) i] = std::pow (10.0f, invRT * delaySec[(size_t) i]);

    preSamples = preMs * 0.001f * fs;

    // Mezcla equal-power
    const float mixAmt = juce::jlimit (0.0f, 1.0f, mixPct * 0.01f);
    dryGain = std::cos (mixAmt * juce::MathConstants<float>::halfPi);
    wetGain = std::sin (mixAmt * juce::MathConstants<float>::halfPi);
}

//==============================================================================
inline void PluginProcessor::processFrame (float inL, float inR,
                                           float& wetL, float& wetR) noexcept
{
    // --- Low-cut de entrada (evita retumbo en las primeras reflexiones)
    inLow[0] += hpB * (inL - inLow[0]);
    inLow[1] += hpB * (inR - inLow[1]);
    const float xl = inL - inLow[0];
    const float xr = inR - inLow[1];

    // --- Pre-delay (push primero, luego tap con +1 => delay 0 válido)
    preDelay[0].push (xl);
    preDelay[1].push (xr);
    float dl = preDelay[0].tap (preSamples + 1.0f);
    float dr = preDelay[1].tap (preSamples + 1.0f);

    // --- Difusión de entrada
    dl = diffuser[0][1].process (diffuser[0][0].process (dl));
    dr = diffuser[1][1].process (diffuser[1][0].process (dr));

    // --- FDN: lectura con modulación, damping y low-cut por línea
    float y[kNumLines];
    for (int i = 0; i < kNumLines; ++i)
    {
        float t = lfoPhase[(size_t) i];
        const float mod = 4.0f * t * (1.0f - std::abs (t));   // seno parabólico barato
        t += lfoInc[(size_t) i];
        if (t >= 1.0f)
            t -= 2.0f;
        lfoPhase[(size_t) i] = t;

        const float d = baseDelay[(size_t) i] + modDepth[(size_t) i] * mod;
        float s = lines[i].tap (d);

        float& lp = lpState[(size_t) i];
        lp += lpA * (s - lp);   // damping (paso-bajo)
        s = lp;

        float& hp = hpState[(size_t) i];
        hp += hpB * (s - hp);   // seguidor de graves
        s -= hp;                // low-cut (paso-alto)

        y[i] = s * fbGain[(size_t) i];
    }

    // --- Salida estéreo decorrelada
    float oL = 0.0f, oR = 0.0f;
    for (int i = 0; i < kNumLines; ++i)
    {
        oL += kSignL[i] * y[i];
        oR += kSignR[i] * y[i];
    }
    wetL = oL * kOutputGain;
    wetR = oR * kOutputGain;

    // --- Mezcla ortonormal + inyección de entrada (L en líneas pares, R en impares)
    hadamard8 (y);
    for (int i = 0; i < kNumLines; ++i)
    {
        const float in = ((i & 1) ? dr : dl) * kInputGain;
        lines[i].push (y[i] + in);
    }
}

//==============================================================================
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0)
        return;

    decaySm.setTargetValue (decayParam->load());
    preDelaySm.setTargetValue (preDelayParam->load());
    lowCutSm.setTargetValue (lowCutParam->load());
    highCutSm.setTargetValue (highCutParam->load());
    mixSm.setTargetValue (mixParam->load());

    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
    const bool monoIn = getTotalNumInputChannels() < 2;

    int pos = 0;
    while (pos < numSamples)
    {
        const int chunk = juce::jmin (kSubBlock, numSamples - pos);
        updateCoefficients (chunk);

        for (int n = 0; n < chunk; ++n)
        {
            const float dryL = left[pos + n];
            const float dryR = (right != nullptr && ! monoIn) ? right[pos + n] : dryL;

            float wl = 0.0f, wr = 0.0f;
            processFrame (dryL, dryR, wl, wr);

            left[pos + n] = dryL * dryGain + wl * wetGain;
            if (right != nullptr)
                right[pos + n] = dryR * dryGain + wr * wetGain;
        }
        pos += chunk;
    }
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
