#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "dsp/IAudioProcessor.h"
#include "multicore/DspPipeline.h"

namespace rp::multicore
{

    // Runs an audio processor on core 1. Blocks are submitted and collected on core 0 (see DspPipeline).
    // Only one instance may exist, because there is only one core 1.
    class DspCore
    {
    public:
        // Launches core 1. The processor is used by core 1 only from then on.
        explicit DspCore(dsp::IAudioProcessorPtr processor);

        // Stops core 1.
        ~DspCore();

        DspCore(const DspCore&) = delete;
        DspCore& operator=(const DspCore&) = delete;

        bool submit(const int16_t* samples, size_t numSamples);

        template<typename Sink>
        void drain(Sink&& sink)
        {
            pipeline_->drain(std::forward<Sink>(sink));
        }

    private:
        // On the heap: the slots are too large for the main stack.
        std::unique_ptr<DspPipeline> pipeline_;
    };

} // namespace rp::multicore
