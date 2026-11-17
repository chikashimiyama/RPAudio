#include "dsp/RingModulator.h"

#include <algorithm>
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
            // -32768 * -1 = 32768 does not fit in int16_t, so clamp.
            const float product = static_cast<float>(input[i]) * carrier;
            output[i] = static_cast<int16_t>(std::clamp(product, -32768.0f, 32767.0f));

            phase_ += phaseIncrement_;
            if (phase_ >= twoPi)
            {
                phase_ -= twoPi;
            }
        }
    }

} // namespace rp::dsp
