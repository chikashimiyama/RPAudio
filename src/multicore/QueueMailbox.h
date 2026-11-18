#pragma once

#include "multicore/IMailbox.h"
#include "pico/util/queue.h"

namespace rp::multicore
{

    // Mailbox over two Pico SDK queues: one to send on, one to receive from. The queues are thread- and core-safe
    // and order memory accesses (their spin lock contains the barriers), so writes made before a send are visible
    // to the receiver. Waiting receives sleep in WFE.
    // The queues are not owned and must be initialised for 32-bit elements and outlive the mailbox.
    class QueueMailbox : public IMailbox
    {
    public:
        QueueMailbox(queue_t* sendQueue, queue_t* receiveQueue);

        bool trySend(uint32_t message) override;
        void sendBlocking(uint32_t message) override;
        std::optional<uint32_t> tryReceive() override;
        uint32_t receiveBlocking() override;

    private:
        queue_t* sendQueue_;
        queue_t* receiveQueue_;
    };

} // namespace rp::multicore
