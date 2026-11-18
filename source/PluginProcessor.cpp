#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

#if (MSVC)
#include "ipps.h"
#endif

//==============================================================================
namespace ParamIDs
{
    inline constexpr const char* decay    = "decay";
    inline constexpr const char* preDelay = "predelay";
    inline constexpr const char* lowCut   = "lowcut";
    inline constexpr const char* highCut  = "highcut";
    inline constexpr const char* mix      = "mix";
}

//==============================================================================
/**
    KeyofPeace Worship Reverb

    Algoritmo: pre-delay -> difusión de entrada (2 allpass por canal)
               -> FDN de 8 líneas con matriz Hadamard (ortonormal, 24 sumas/restas)
               -> salida estéreo decorrelada con patrones Walsh.

    - Cada línea tiene delay modulado (LFO parabólico lento, <3 cents de desviación)
      para romper resonancias metálicas.
    - Cada línea tiene damping (paso-bajo 1 polo) y low-cut (paso-alto 1 polo),
      así la cola se vuelve más suave y oscura con el tiempo.
    - Ganancia de realimentación por línea calculada a partir de RT60.
*/
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    //==========================================================================
    static constexpr int kNumLines = 8;
    static constexpr int kSubBlock = 32;   // los coeficientes se actualizan cada 32 muestras

    // Delay circular (potencia de 2) con interpolación lineal.
    // tap(d): valor escrito d muestras antes de la PRÓXIMA escritura (d >= 1).
    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0;
        int writePos = 0;

        void prepare (int maxDelaySamples)
        {
            int size = 1;
            while (size < maxDelaySamples + 4)
                size <<= 1;
            buf.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0;
        }

        void reset()
        {
            std::fill (buf.begin(), buf.end(), 0.0f);
            writePos = 0;
        }

        inline void push (float x) noexcept
        {
            buf[(size_t) writePos] = x;
            writePos = (writePos + 1) & mask;
        }

        inline float tap (float delay) const noexcept
        {
            const int di = (int) delay;
            const float frac = delay - (float) di;
            const int i0 = (writePos - di) & mask;
            const int i1 = (i0 - 1) & mask;
            const float a = buf[(size_t) i0];
            const float b = buf[(size_t) i1];
            return a + frac * (b - a);
        }

        inline float tapInt (int delay) const noexcept
        {
            return buf[(size_t) ((writePos - delay) & mask)];
        }
    };

    // All-pass de Schroeder con longitud fija
    struct Allpass
    {
        DelayLine line;
        int length = 1;
        float g = 0.6f;

        void prepare (int len, float gain)
        {
            length = juce::jmax (1, len);
            g = gain;
            line.prepare (length + 2);
        }

        void reset() { line.reset(); }

        inline float process (float x) noexcept
        {
            const float z = line.tapInt (length);
            const float w = x - g * z;
            line.push (w);
            return g * w + z;
        }
    };

    static inline void hadamard8 (float* v) noexcept
    {
        for (int i = 0; i < 8; i += 2)
        {
            const float a = v[i], b = v[i + 1];
            v[i] = a + b;
            v[i + 1] = a - b;
        }
        for (int i = 0; i < 8; i += 4)
            for (int j = 0; j < 2; ++j)
            {
                const float a = v[i + j], b = v[i + j + 2];
                v[i + j] = a + b;
                v[i + j + 2] = a - b;
            }
        for (int j = 0; j < 4; ++j)
        {
            const float a = v[j], b = v[j + 4];
            v[j] = a + b;
            v[j + 4] = a - b;
        }
        constexpr float scale = 0.35355339059f; // 1/sqrt(8)
        for (int i = 0; i < 8; ++i)
            v[i] *= scale;
    }

    void updateCoefficients (int chunk) noexcept;
    inline void processFrame (float inL, float inR, float& wetL, float& wetR) noexcept;

    //==========================================================================
    double sr = 44100.0;

    // Parámetros (punteros atómicos del APVTS)
    std::atomic<float>* decayParam    = nullptr;
    std::atomic<float>* preDelayParam = nullptr;
    std::atomic<float>* lowCutParam   = nullptr;
    std::atomic<float>* highCutParam  = nullptr;
    std::atomic<float>* mixParam      = nullptr;

    juce::SmoothedValue<float> decaySm, preDelaySm, lowCutSm, highCutSm, mixSm;

    // Etapas
    DelayLine preDelay[2];
    Allpass diffuser[2][2];
    float inLow[2] { 0.0f, 0.0f };   // low-cut de entrada

    DelayLine lines[kNumLines];
    std::array<float, kNumLines> baseDelay {};   // muestras
    std::array<float, kNumLines> delaySec {};
    std::array<float, kNumLines> modDepth {};    // muestras
    std::array<float, kNumLines> lfoPhase {};
    std::array<float, kNumLines> lfoInc {};
    std::array<float, kNumLines> lpState {};
    std::array<float, kNumLines> hpState {};
    std::array<float, kNumLines> fbGain {};

    // Coeficientes actuales
    float lpA = 0.5f;    // damping (paso-bajo)
    float hpB = 0.01f;   // low-cut (seguidor de graves)
    float preSamples = 0.0f;
    float dryGain = 1.0f, wetGain = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
