#pragma once

#include "multicore/IMailbox.h"

namespace rp::multicore
{

    // Mailbox over the SIO inter-core FIFOs. The SIO maps each core to its own FIFO pair, so the same class serves
    // as the mailbox of either core. Each core must use its own instance from its own side only.
    // RP2350 FIFOs hold 4 messages per direction.
    class SioMailbox : public IMailbox
    {
    public:
        bool trySend(uint32_t message) override;
        void sendBlocking(uint32_t message) override;
        std::optional<uint32_t> tryReceive() override;
        uint32_t receiveBlocking() override;
    };

} // namespace rp::multicore
