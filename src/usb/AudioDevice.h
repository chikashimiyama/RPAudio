#pragma once

#include <cstddef>
#include <cstdint>

#include "dsp/IAudioProcessor.h"
#include "multicore/DspCore.h"
#include "usb/StreamMonitor.h"

namespace rp::usb
{

    // USB Audio Class 2 device: mono 16-bit 48 kHz.
    // Audio received from the host (speaker) is processed on core 1 and sent back to the host (microphone).
    // Only one instance may exist, because the TinyUSB callbacks are global.
    class AudioDevice
    {
    public:
        // Launches core 1, which runs the processor from then on.
        explicit AudioDevice(dsp::IAudioProcessorPtr processor);
        ~AudioDevice();

        AudioDevice(const AudioDevice&) = delete;
        AudioDevice& operator=(const AudioDevice&) = delete;

        // Must be called repeatedly from the main loop.
        void task();

        // Logs packet counts since the previous call. Call about once per second.
        void logStats();

        // True if the host sent speaker data recently.
        bool isSpeakerStreaming() const;

        // True if the host polled the microphone endpoint recently.
        bool isMicrophoneStreaming() const;

        // ---- Called from the TinyUSB audio callbacks ----

        // A speaker (OUT) packet was decoded into the receive FIFO.
        void onSpeakerPacketReceived(size_t numBytes);

        // The next microphone (IN) packet is about to be loaded from the transmit FIFO.
        void onMicrophonePacketLoading();

        // A microphone (IN) packet was sent.
        void onMicrophonePacketSent(size_t numBytes);

        // Streaming on an interface stops (alt 0) or restarts.
        void onStreamClosing(uint8_t interfaceNumber);

    private:
        // Queues the blocks core 1 has finished for the IN endpoint.
        void collectProcessedAudio();

        multicore::DspCore dspCore_;
        StreamMonitor streamMonitor_;
    };

} // namespace rp::usb
