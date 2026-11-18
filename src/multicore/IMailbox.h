#pragma once

#include <memory>
#include <optional>

#include "multicore/BlockMessage.h"

namespace rp::multicore
{

    // One core's end of a message channel to the other core.
    // Messages are delivered in order. A message sent after writing shared memory makes those writes visible to the
    // receiver before it sees the message.
    class IMailbox
    {
    public:
        virtual ~IMailbox() = default;

        // Sends without waiting. Returns false if the channel is full.
        virtual bool trySend(const BlockMessage& message) = 0;

        // Sends, waiting while the channel is full.
        virtual void sendBlocking(const BlockMessage& message) = 0;

        // Receives without waiting. Returns nothing if no message is pending.
        virtual std::optional<BlockMessage> tryReceive() = 0;

        // Receives, waiting until a message arrives.
        virtual BlockMessage receiveBlocking() = 0;
    };

    using IMailboxPtr = std::unique_ptr<IMailbox>;

} // namespace rp::multicore
