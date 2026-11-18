#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "dsp/IAudioProcessor.h"
#include "multicore/IMailbox.h"

namespace rp::multicore
{

    // Hands audio blocks from core 0 to an audio processor running on core 1 and back.
    //
    // Blocks live in a fixed pool of slots. Each slot is owned by exactly one core at a time; ownership moves with
    // the BlockMessage that carries the slot index, so the cores never touch the same slot concurrently and no lock is
    // needed. The slot count does not exceed the mailbox depth, so sending never has to wait.
    //
    // Core 0 calls submit() and drain(), which never block. Core 1 calls processNext() in a loop.
    // No hardware access: the cores are connected through the given mailboxes.
    class DspPipeline
    {
    public:
        static constexpr size_t numSlots = 4;
        static constexpr size_t maxBlockSamples = 256;

        // core0Mailbox / core1Mailbox: each core's end of the channel between the two cores.
        DspPipeline(dsp::IAudioProcessorPtr processor, IMailboxPtr core0Mailbox, IMailboxPtr core1Mailbox);

        DspPipeline(const DspPipeline&) = delete;
        DspPipeline& operator=(const DspPipeline&) = delete;

        // ---- Core 0 ----

        // Copies numSamples samples into a free slot and passes it to core 1. Returns false if the block was
        // dropped: no free slot (core 1 is falling behind) or numSamples exceeds maxBlockSamples.
        // An empty block is accepted and ignored.
        bool submit(const int16_t* samples, size_t numSamples);

        // Calls sink(const int16_t* samples, size_t numSamples) for every block core 1 has finished, in submission
        // order, and frees its slot afterwards.
        template<typename Sink>
        void drain(Sink&& sink);

        // Number of blocks submitted and not yet drained.
        size_t numBlocksInFlight() const;

        // ---- Core 1 ----

        // Waits for the next block, processes it in place and passes it back to core 0.
        void processNext();

    private:
        using Slot = std::array<int16_t, maxBlockSamples>;

        // Index of a free slot, or nothing if all slots are in flight.
        std::optional<size_t> findFreeSlot() const;

        dsp::IAudioProcessorPtr processor_;
        IMailboxPtr core0Mailbox_;
        IMailboxPtr core1Mailbox_;
        std::array<Slot, numSlots> slots_{};
        std::array<bool, numSlots> isSlotFree_; // Touched by core 0 only.
    };

    template<typename Sink>
    void DspPipeline::drain(Sink&& sink)
    {
        while (const std::optional<BlockMessage> message = core0Mailbox_->tryReceive())
        {
            if (message->slotIndex >= numSlots)
            {
                continue;
            }
            sink(static_cast<const int16_t*>(slots_[message->slotIndex].data()), message->numSamples);
            isSlotFree_[message->slotIndex] = true;
        }
    }

} // namespace rp::multicore
