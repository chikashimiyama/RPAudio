#include "multicore/DspPipeline.h"

#include <algorithm>
#include <utility>

namespace rp::multicore
{

    DspPipeline::DspPipeline(dsp::IAudioProcessorPtr processor, IMailboxPtr core0Mailbox, IMailboxPtr core1Mailbox)
        : processor_(std::move(processor))
        , core0Mailbox_(std::move(core0Mailbox))
        , core1Mailbox_(std::move(core1Mailbox))
    {
        isSlotFree_.fill(true);
    }

    bool DspPipeline::submit(const int16_t* samples, size_t numSamples)
    {
        if (numSamples == 0)
        {
            return true;
        }
        if (numSamples > maxBlockSamples)
        {
            return false;
        }
        const std::optional<size_t> slotIndex = findFreeSlot();
        if (!slotIndex)
        {
            return false;
        }

        std::copy_n(samples, numSamples, slots_[*slotIndex].begin());

        const BlockMessage message{*slotIndex, numSamples};
        if (!core0Mailbox_->trySend(message))
        {
            return false;
        }
        isSlotFree_[*slotIndex] = false;
        return true;
    }

    size_t DspPipeline::numBlocksInFlight() const
    {
        return static_cast<size_t>(std::count(isSlotFree_.begin(), isSlotFree_.end(), false));
    }

    void DspPipeline::processNext()
    {
        const BlockMessage message = core1Mailbox_->receiveBlocking();
        if (message.slotIndex >= numSlots)
        {
            return;
        }
        int16_t* const samples = slots_[message.slotIndex].data();
        processor_->process(samples, samples, message.numSamples);
        core1Mailbox_->sendBlocking(message);
    }

    std::optional<size_t> DspPipeline::findFreeSlot() const
    {
        for (size_t slotIndex = 0; slotIndex < numSlots; ++slotIndex)
        {
            if (isSlotFree_[slotIndex])
            {
                return slotIndex;
            }
        }
        return std::nullopt;
    }

} // namespace rp::multicore
