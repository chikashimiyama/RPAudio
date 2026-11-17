#pragma once

#include <vector>

#include "dsp/IAudioProcessor.h"

namespace rp::dsp
{

    // FIR filter by direct convolution: y[n] = sum(c[k] * x[n-k]) for k = 0 .. N-1
    // Costs N multiply-adds per sample.
    class FirFilter : public IAudioProcessor
    {
    public:
        // coefficients: impulse response c[0] .. c[N-1]. Must not be empty.
        explicit FirFilter(const std::vector<float>& coefficients);

        void process(const int16_t* input, int16_t* output, size_t numSamples) override;

    private:
        // Coefficients in reverse order, so that they line up with the history window (oldest sample first).
        std::vector<float> reversedCoefficients_;

        // Input history, stored twice (2N entries) so that the last N samples are always contiguous:
        // no wrap-around check in the inner loop.
        std::vector<float> history_;

        // Slot of the newest sample (0 .. N-1).
        size_t position_ = 0;
    };

} // namespace rp::dsp
