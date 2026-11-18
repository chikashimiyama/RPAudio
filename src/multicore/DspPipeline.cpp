#include "multicore/DspPipeline.h"

#include <algorithm>
#include <utility>

namespace rp::multicore
{

    namespace
    {

        constexpr uint32_t slotIndexBits = 0xFFFF;
        constexpr uint32_t numSamplesShift = 16;
        constexpr uint32_t allSlotsFree = (1u << DspPipeline::numSlots) - 1;

        static_assert(DspPipeline::numSlots <= 32, "free slots are tracked in a 32-bit mask");
        static_assert(DspPipeline::maxBlockSamples <= 0xFFFF, "the sample count must fit in 16 bits");

        // Lowest free slot. freeSlotMask must not be 0.
        size_t findFreeSlot(uint32_t freeSlotMask)
        {
            size_t slotIndex = 0;
            while (((freeSlotMask >> slotIndex) & 1u) == 0)
            {
                ++slotIndex;
            }
            return slotIndex;
        }

    } // namespace

    DspPipeline::DspPipeline(dsp::IAudioProcessorPtr processor, IMailboxPtr core0Mailbox, IMailboxPtr core1Mailbox)
        : processor_(std::move(processor))
        , core0Mailbox_(std::move(core0Mailbox))
        , core1Mailbox_(std::move(core1Mailbox))
        , freeSlotMask_(allSlotsFree)
    {
    }

    bool DspPipeline::submit(const int16_t* samples, size_t numSamples)
    {
        if (numSamples == 0)
        {
            return true;
        }
        if (numSamples > maxBlockSamples || freeSlotMask_ == 0)
        {
            return false;
        }

        const size_t slotIndex = findFreeSlot(freeSlotMask_);
        std::copy_n(samples, numSamples, slots_[slotIndex].begin());

        if (!core0Mailbox_->trySend(packMessage(slotIndex, numSamples)))
        {
            return false;
        }
        freeSlotMask_ &= ~(1u << slotIndex);
        return true;
    }

    size_t DspPipeline::numBlocksInFlight() const
    {
        size_t numFree = 0;
        for (size_t i = 0; i < numSlots; ++i)
        {
            numFree += (freeSlotMask_ >> i) & 1u;
        }
        return numSlots - numFree;
    }

    void DspPipeline::processNext()
    {
        const uint32_t message = core1Mailbox_->receiveBlocking();
        const size_t slotIndex = slotIndexOf(message);
        if (slotIndex >= numSlots)
        {
            return;
        }
        int16_t* const samples = slots_[slotIndex].data();
        processor_->process(samples, samples, numSamplesOf(message));
        core1Mailbox_->sendBlocking(message);
    }

    uint32_t DspPipeline::packMessage(size_t slotIndex, size_t numSamples)
    {
        return (static_cast<uint32_t>(numSamples) << numSamplesShift) | static_cast<uint32_t>(slotIndex);
    }

    size_t DspPipeline::slotIndexOf(uint32_t message)
    {
        return message & slotIndexBits;
    }

    size_t DspPipeline::numSamplesOf(uint32_t message)
    {
        return message >> numSamplesShift;
    }

    void DspPipeline::releaseSlot(size_t slotIndex)
    {
        freeSlotMask_ |= 1u << slotIndex;
    }

} // namespace rp::multicore
