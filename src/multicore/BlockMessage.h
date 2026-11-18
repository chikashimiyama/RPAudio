#pragma once

#include <cstddef>

namespace rp::multicore
{

    // Message passed between the cores: which slot holds a block, and how many samples it has.
    // Sending it hands ownership of the slot to the receiving core.
    struct BlockMessage
    {
        size_t slotIndex = 0;
        size_t numSamples = 0;
    };

} // namespace rp::multicore
