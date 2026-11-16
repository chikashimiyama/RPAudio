#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace rp::dsp
{

class IAudioProcessor
{
public:
    virtual ~IAudioProcessor() = default;

    // Processes numSamples mono 16-bit samples. input and output may alias.
    virtual void process(const int16_t* input, int16_t* output, size_t numSamples) = 0;
};

using IAudioProcessorPtr = std::unique_ptr<IAudioProcessor>;

} // namespace rp::dsp
