#include "multicore/QueueMailbox.h"

namespace rp::multicore
{

    QueueMailbox::QueueMailbox(queue_t* sendQueue, queue_t* receiveQueue)
        : sendQueue_(sendQueue)
        , receiveQueue_(receiveQueue)
    {
    }

    bool QueueMailbox::trySend(const BlockMessage& message)
    {
        return queue_try_add(sendQueue_, &message);
    }

    void QueueMailbox::sendBlocking(const BlockMessage& message)
    {
        queue_add_blocking(sendQueue_, &message);
    }

    std::optional<BlockMessage> QueueMailbox::tryReceive()
    {
        BlockMessage message;
        if (!queue_try_remove(receiveQueue_, &message))
        {
            return std::nullopt;
        }
        return message;
    }

    BlockMessage QueueMailbox::receiveBlocking()
    {
        BlockMessage message;
        queue_remove_blocking(receiveQueue_, &message);
        return message;
    }

} // namespace rp::multicore
