#include "dsp/RingModulator.h"

#include <cmath>

namespace rp::dsp
{

namespace
{

constexpr float twoPi = 6.28318530717958647692f;

} // namespace

RingModulator::RingModulator(float carrierFrequency, float sampleRate)
    : phaseIncrement_(twoPi * carrierFrequency / sampleRate)
{
}

void RingModulator::process(const int16_t* input, int16_t* output, size_t numSamples)
{
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float carrier = std::sin(phase_);
        // |carrier| <= 1, so the product always fits in int16_t.
        output[i] = static_cast<int16_t>(static_cast<float>(input[i]) * carrier);

        phase_ += phaseIncrement_;
        if (phase_ >= twoPi)
        {
            phase_ -= twoPi;
        }
    }
}

} // namespace rp::dsp
