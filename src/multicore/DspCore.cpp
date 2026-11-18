#include "multicore/DspCore.h"

#include "multicore/QueueMailbox.h"
#include "pico/multicore.h"

namespace rp::multicore
{

    namespace
    {

        // Holding every slot, so that a send never has to wait.
        constexpr uint queueCapacity = DspPipeline::numSlots;

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
    {
        queue_init(&toCore1Queue_, sizeof(uint32_t), queueCapacity);
        queue_init(&toCore0Queue_, sizeof(uint32_t), queueCapacity);
        pipeline_ = std::make_unique<DspPipeline>(std::move(processor),
                                                  std::make_unique<QueueMailbox>(&toCore1Queue_, &toCore0Queue_),
                                                  std::make_unique<QueueMailbox>(&toCore0Queue_, &toCore1Queue_));

        core1Pipeline = pipeline_.get();
        multicore_launch_core1(core1Main);
    }

    DspCore::~DspCore()
    {
        multicore_reset_core1();
        core1Pipeline = nullptr;
        queue_free(&toCore1Queue_);
        queue_free(&toCore0Queue_);
    }

    bool DspCore::submit(const int16_t* samples, size_t numSamples)
    {
        return pipeline_->submit(samples, numSamples);
    }

} // namespace rp::multicore
