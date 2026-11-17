#pragma once

#include "dsp/IAudioProcessor.h"

namespace rp::dsp
{

    // Second-order lowpass (RBJ Audio EQ Cookbook), Direct Form I:
    //   y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    class BiquadFilter : public IAudioProcessor
    {
    public:
        BiquadFilter(float cutoffFrequency, float q, float sampleRate);

        void process(const int16_t* input, int16_t* output, size_t numSamples) override;

    private:
        // Coefficients, normalised so that a0 = 1.
        float b0_;
        float b1_;
        float b2_;
        float a1_;
        float a2_;

        // Previous inputs and outputs.
        float x1_ = 0.0f;
        float x2_ = 0.0f;
        float y1_ = 0.0f;
        float y2_ = 0.0f;
    };

} // namespace rp::dsp
