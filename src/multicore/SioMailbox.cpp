#include "multicore/SioMailbox.h"

#include "hardware/sync.h"
#include "pico/multicore.h"

namespace rp::multicore
{

    // The barriers order shared-memory accesses against the FIFO register access: writes before a send are visible
    // to the other core before the message, and reads after a receive do not happen before the message arrived.

    bool SioMailbox::trySend(uint32_t message)
    {
        if (!multicore_fifo_wready())
        {
            return false;
        }
        __dmb();
        multicore_fifo_push_blocking(message);
        return true;
    }

    void SioMailbox::sendBlocking(uint32_t message)
    {
        __dmb();
        multicore_fifo_push_blocking(message);
    }

    std::optional<uint32_t> SioMailbox::tryReceive()
    {
        if (!multicore_fifo_rvalid())
        {
            return std::nullopt;
        }
        const uint32_t message = multicore_fifo_pop_blocking();
        __dmb();
        return message;
    }

    uint32_t SioMailbox::receiveBlocking()
    {
        const uint32_t message = multicore_fifo_pop_blocking(); // sleeps in WFE while empty
        __dmb();
        return message;
    }

} // namespace rp::multicore
