#pragma once

#include <cstdint>

#include "led/Ws2812.h"

namespace rp::led
{

    // Status LED: blinks while the main loop is running. If it stops, the firmware is stuck.
    // Blink color: green = idle, blue = speaker data arriving, red = microphone polled, magenta = both.
    class Heartbeat
    {
    public:
        explicit Heartbeat(uint pin);

        // Call from the main loop; only writes to the LED when the color changes.
        void update(bool isSpeakerStreaming, bool isMicrophoneStreaming);

    private:
        Ws2812 led_;
        uint32_t color_ = 0;
    };

} // namespace rp::led
