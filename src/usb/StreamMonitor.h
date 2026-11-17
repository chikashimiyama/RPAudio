#pragma once

#include <cstddef>
#include <cstdint>

namespace rp::usb
{

    // Tracks when the speaker (OUT) and microphone (IN) streams last moved data, and counts packets for the
    // once-per-second statistics line in the debug log.
    class StreamMonitor
    {
    public:
        void onSpeakerPacket(size_t numBytes);
        void onMicrophonePacket(size_t numBytes);

        // True if the stream moved data within the last 50 ms (for the status LED).
        bool isSpeakerStreaming() const;
        bool isMicrophoneStreaming() const;

        // True if no speaker packet arrived within the last 3 ms (the microphone is then padded with silence).
        bool isSpeakerIdle() const;

        // Logs packet counts since the previous call and resets them. Logs nothing while both streams are idle.
        void logStats();

    private:
        struct Stats
        {
            uint32_t speakerPackets = 0;
            uint32_t speakerZeroLengthPackets = 0;
            uint32_t speakerBytes = 0;
            uint32_t microphonePackets = 0;
            uint32_t microphoneBytes = 0;
        };

        uint64_t lastSpeakerPacketUs_ = 0; // 0 = never
        uint64_t lastMicrophonePacketUs_ = 0;
        Stats stats_{};
    };

} // namespace rp::usb
