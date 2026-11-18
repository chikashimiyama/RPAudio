#include "multicore/QueueMailbox.h"

namespace rp::multicore
{

    QueueMailbox::QueueMailbox(queue_t* sendQueue, queue_t* receiveQueue)
        : sendQueue_(sendQueue)
        , receiveQueue_(receiveQueue)
    {
    }

    bool QueueMailbox::trySend(uint32_t message)
    {
        return queue_try_add(sendQueue_, &message);
    }

    void QueueMailbox::sendBlocking(uint32_t message)
    {
        queue_add_blocking(sendQueue_, &message);
    }

    std::optional<uint32_t> QueueMailbox::tryReceive()
    {
        uint32_t message = 0;
        if (!queue_try_remove(receiveQueue_, &message))
        {
            return std::nullopt;
        }
        return message;
    }

    uint32_t QueueMailbox::receiveBlocking()
    {
        uint32_t message = 0;
        queue_remove_blocking(receiveQueue_, &message);
        return message;
    }

} // namespace rp::multicore
