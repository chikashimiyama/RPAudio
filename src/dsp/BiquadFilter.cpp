#include "dsp/BiquadFilter.h"

#include <algorithm>
#include <cmath>

namespace rp::dsp
{

    namespace
    {

        constexpr float twoPi = 6.28318530717958647692f;

    } // namespace

    BiquadFilter::BiquadFilter(float cutoffFrequency, float q, float sampleRate)
    {
        const float omega = twoPi * cutoffFrequency / sampleRate;
        const float cosOmega = std::cos(omega);
        const float alpha = std::sin(omega) / (2.0f * q);
        const float a0 = 1.0f + alpha;

        b0_ = (1.0f - cosOmega) / 2.0f / a0;
        b1_ = (1.0f - cosOmega) / a0;
        b2_ = b0_;
        a1_ = -2.0f * cosOmega / a0;
        a2_ = (1.0f - alpha) / a0;
    }

    void BiquadFilter::process(const int16_t* input, int16_t* output, size_t numSamples)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float x = static_cast<float>(input[i]);
            const float y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

            x2_ = x1_;
            x1_ = x;
            y2_ = y1_;
            y1_ = y;

            // A resonant lowpass can overshoot full scale, so clamp.
            output[i] = static_cast<int16_t>(std::clamp(y, -32768.0f, 32767.0f));
        }
    }

} // namespace rp::dsp
