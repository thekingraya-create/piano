#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <cmath>

// Pointers to the knob values, shared by all voices
struct PianoParams
{
    std::atomic<float>* tone   = nullptr;
    std::atomic<float>* decay  = nullptr;
    std::atomic<float>* hammer = nullptr;
    std::atomic<float>* width  = nullptr;
};

struct PianoSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Additive "string model" piano voice:
//  - stretched (slightly inharmonic) partials like real piano strings
//  - higher partials die faster, high notes die faster than low notes
//  - harder hits are brighter
//  - hammer strike position notches some partials
//  - two slightly detuned strings on the lower partials for gentle beating
//  - short filtered-noise "hammer thump" at the start
class PianoVoice : public juce::SynthesiserVoice
{
public:
    explicit PianoVoice (const PianoParams& p) : params (p) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<PianoSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        constexpr float pi    = juce::MathConstants<float>::pi;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        const float sr     = (float) getSampleRate();
        const float f0     = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        const float tone   = params.tone->load();
        const float decay  = params.decay->load();
        const float hammer = params.hammer->load();
        const float width  = params.width->load();
        const float vel    = juce::jlimit (0.05f, 1.0f, velocity);

        // How much the string "stretches" its overtones (higher notes = more)
        const float B = juce::jlimit (0.00005f, 0.004f,
                                      0.00012f * std::exp ((float) (midiNote - 48) * 0.05f));

        // Time for the note to fade by 60 dB: ~14 s for the lowest key, ~1.5 s at the top
        const float T0 = 14.0f * std::pow (2.0f, -(float) (midiNote - 21) / 24.0f) * decay;

        // Higher notes tilt to the right, lower to the left
        const float slope = (1.6f - 1.0f * tone) * (1.35f - 0.7f * vel);

        numPartials = 0;
        auto add = [&] (float f, float a, float decayMul)
        {
            if (numPartials < (int) partials.size())
                partials[(size_t) numPartials++] = { a, 0.0f, twoPi * f / sr, 1.0f, decayMul };
        };

        const float nyquistLimit = 0.45f * sr;

        for (int n = 1; n <= 24; ++n)
        {
            const float f = (float) n * f0 * std::sqrt (1.0f + B * (float) (n * n));
            if (f > nyquistLimit)
                break;

            float a = std::pow ((float) n, -slope);
            a *= 0.25f + 0.75f * std::abs (std::sin (pi * (float) n * 0.12f)); // hammer position

            if (n == 1 && midiNote < 52) // tame very low fundamentals (hard to hear anyway)
                a *= 0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, (float) (midiNote - 21) / 31.0f);

            const float t60 = juce::jmax (0.05f, T0 / (1.0f + 0.35f * (float) (n - 1)));
            const float tau = t60 / 6.91f;
            const float decayMul = std::exp (-1.0f / (tau * sr));

            if (n <= 4)
            {
                const float d = 0.0005f;
                add (f * (1.0f + d), a * 0.5f, decayMul);
                add (f * (1.0f - d), a * 0.5f, decayMul);
            }
            else
                add (f, a, decayMul);
        }

        gain = 0.16f * std::pow (vel, 1.3f);

        bump    = 1.0f;
        bumpMul = std::exp (-1.0f / (0.5f * sr));

        hamEnv  = vel * hammer * 0.35f;
        hamMul  = std::exp (-1.0f / (0.012f * sr));
        lpCoef  = 0.15f + 0.35f * vel;
        lp      = 0.0f;

        releasing = false;
        relGain   = 1.0f;
        const float relTau = juce::jmap ((float) juce::jlimit (21, 108, midiNote),
                                         21.0f, 108.0f, 0.18f, 0.05f);
        relMul = std::exp (-1.0f / (relTau * sr));

        const float pan   = juce::jlimit (-1.0f, 1.0f, (float) (midiNote - 60) / 45.0f) * 0.5f * width;
        const float angle = (pan + 1.0f) * pi * 0.25f;
        gainL = std::cos (angle) * 1.4142f;
        gainR = std::sin (angle) * 1.4142f;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            releasing = true;   // damper comes down
        else
            clearCurrentNote();
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        auto* left  = buffer.getWritePointer (0, startSample);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;

            for (int p = 0; p < numPartials; ++p)
            {
                auto& part = partials[(size_t) p];
                s += part.amp * part.env * std::sin (part.phase);
                part.phase += part.inc;
                if (part.phase > twoPi)
                    part.phase -= twoPi;
                part.env *= part.decayMul;
            }

            s *= gain * (0.6f + 0.4f * bump);
            bump *= bumpMul;

            // hammer thump
            lp += lpCoef * ((rng.nextFloat() * 2.0f - 1.0f) - lp);
            s += lp * hamEnv;
            hamEnv *= hamMul;

            if (releasing)
            {
                s *= relGain;
                relGain *= relMul;
            }

            left[i] += s * gainL;
            if (right != nullptr)
                right[i] += s * gainR;

            if ((releasing && relGain < 0.0005f) || partials[0].env < 0.001f)
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    struct Partial { float amp, phase, inc, env, decayMul; };

    const PianoParams& params;
    std::array<Partial, 40> partials {};
    int numPartials = 0;

    float gain = 0.0f, gainL = 1.0f, gainR = 1.0f;
    float bump = 1.0f, bumpMul = 1.0f;
    float hamEnv = 0.0f, hamMul = 1.0f, lp = 0.0f, lpCoef = 0.2f;
    bool  releasing = false;
    float relGain = 1.0f, relMul = 1.0f;
    juce::Random rng;
};
