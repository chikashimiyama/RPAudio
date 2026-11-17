#pragma once

#include "dsp/IAudioProcessor.h"

namespace rp::dsp
{

    // Multiplies the input by a sine carrier: y[n] = x[n] * sin(2*pi*f*n/fs)
    class RingModulator : public IAudioProcessor
    {
    public:
        RingModulator(float carrierFrequency, float sampleRate);

        void process(const int16_t* input, int16_t* output, size_t numSamples) override;

    private:
        float phase_ = 0.0f;
        float phaseIncrement_;
    };

} // namespace rp::dsp
