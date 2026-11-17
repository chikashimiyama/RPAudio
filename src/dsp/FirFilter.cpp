#include "dsp/FirFilter.h"

#include <algorithm>
#include <cassert>

namespace rp::dsp
{

    FirFilter::FirFilter(const std::vector<float>& coefficients)
        : reversedCoefficients_(coefficients.rbegin(), coefficients.rend())
        , history_(coefficients.size() * 2, 0.0f)
    {
        assert(!coefficients.empty());
    }

    void FirFilter::process(const int16_t* input, int16_t* output, size_t numSamples)
    {
        const size_t numTaps = reversedCoefficients_.size();

        for (size_t i = 0; i < numSamples; ++i)
        {
            position_ = (position_ + 1 == numTaps) ? 0 : position_ + 1;

            const float x = static_cast<float>(input[i]);
            history_[position_] = x;
            history_[position_ + numTaps] = x;

            // history_[position_ + 1] .. history_[position_ + numTaps] holds x[n-N+1] .. x[n].
            const float* window = &history_[position_ + 1];
            float y = 0.0f;
            for (size_t k = 0; k < numTaps; ++k)
            {
                y += reversedCoefficients_[k] * window[k];
            }

            output[i] = static_cast<int16_t>(std::clamp(y, -32768.0f, 32767.0f));
        }
    }

} // namespace rp::dsp
