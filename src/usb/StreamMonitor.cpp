#include "usb/StreamMonitor.h"

#include "debug/DebugLog.h"
#include "hardware/sync.h"
#include "pico/time.h"

namespace rp::usb
{

    namespace
    {

        constexpr uint64_t neverUs = 0;
        constexpr uint64_t streamTimeoutUs = 50'000;
        constexpr uint64_t speakerIdleUs = 3'000;

        bool isWithin(uint64_t timestampUs, uint64_t windowUs)
        {
            return timestampUs != neverUs && time_us_64() - timestampUs < windowUs;
        }

    } // namespace

    void StreamMonitor::onSpeakerPacket(size_t numBytes)
    {
        lastSpeakerPacketUs_ = time_us_64();
        ++stats_.speakerPackets;
        stats_.speakerBytes += numBytes;
        if (numBytes == 0)
        {
            ++stats_.speakerZeroLengthPackets;
        }
    }

    void StreamMonitor::onMicrophonePacket(size_t numBytes)
    {
        lastMicrophonePacketUs_ = time_us_64();
        ++stats_.microphonePackets;
        stats_.microphoneBytes += numBytes;
    }

    bool StreamMonitor::isSpeakerStreaming() const
    {
        return isWithin(lastSpeakerPacketUs_, streamTimeoutUs);
    }

    bool StreamMonitor::isMicrophoneStreaming() const
    {
        return isWithin(lastMicrophonePacketUs_, streamTimeoutUs);
    }

    bool StreamMonitor::isSpeakerIdle() const
    {
        return !isWithin(lastSpeakerPacketUs_, speakerIdleUs);
    }

    void StreamMonitor::logStats()
    {
        const uint32_t interruptState = save_and_disable_interrupts();
        const Stats snapshot = stats_;
        stats_ = Stats{};
        restore_interrupts(interruptState);

        if (snapshot.speakerPackets == 0 && snapshot.microphonePackets == 0)
        {
            return;
        }
        rp::debug::log("stats/s: spk pkts=%lu zlp=%lu bytes=%lu | mic pkts=%lu bytes=%lu",
                       static_cast<unsigned long>(snapshot.speakerPackets),
                       static_cast<unsigned long>(snapshot.speakerZeroLengthPackets),
                       static_cast<unsigned long>(snapshot.speakerBytes),
                       static_cast<unsigned long>(snapshot.microphonePackets),
                       static_cast<unsigned long>(snapshot.microphoneBytes));
    }

} // namespace rp::usb
