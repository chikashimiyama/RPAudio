#include "multicore/DspCore.h"

#include "multicore/SioMailbox.h"
#include "pico/multicore.h"

namespace rp::multicore
{

    namespace
    {

        // Read by core 1 only after the launch handshake, which orders it after the write on core 0.
        DspPipeline* core1Pipeline = nullptr;

        // Runs from RAM so that core 1 does not wait on flash (XIP) between blocks.
        void __not_in_flash_func(core1Main)()
        {
            while (true)
            {
                core1Pipeline->processNext();
            }
        }

    } // namespace

    DspCore::DspCore(dsp::IAudioProcessorPtr processor)
        : pipeline_(std::make_unique<DspPipeline>(std::move(processor), std::make_unique<SioMailbox>(),
                                                  std::make_unique<SioMailbox>()))
    {
        core1Pipeline = pipeline_.get();
        multicore_launch_core1(core1Main);
    }

    DspCore::~DspCore()
    {
        multicore_reset_core1();
        core1Pipeline = nullptr;
    }

    bool DspCore::submit(const int16_t* samples, size_t numSamples)
    {
        return pipeline_->submit(samples, numSamples);
    }

} // namespace rp::multicore
