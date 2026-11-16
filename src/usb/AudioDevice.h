#pragma once

#include "dsp/IAudioProcessor.h"

namespace rp::usb
{

// USB Audio Class 2 device: mono 16-bit 48 kHz.
// Audio received from the host (speaker) is processed and sent back to the host (microphone).
// Only one instance may exist, because the TinyUSB callbacks are global.
class AudioDevice
{
public:
    explicit AudioDevice(dsp::IAudioProcessorPtr processor);
    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    // Must be called repeatedly from the main loop.
    void task();

    // Called from the TinyUSB OUT-endpoint callback.
    void onAudioReceived(size_t numBytes);

private:
    dsp::IAudioProcessorPtr processor_;
};

} // namespace rp::usb
