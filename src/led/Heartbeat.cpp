#include "led/Heartbeat.h"

#include "Consts.h"
#include "pico/time.h"

namespace rp::led
{

    Heartbeat::Heartbeat(uint pin)
        : led_(pin)
    {
        led_.setColor(0, 0, 0);
    }

    void Heartbeat::update(bool isSpeakerStreaming, bool isMicrophoneStreaming)
    {
        const bool isOn = ((time_us_64() / heartbeatHalfPeriodUs) % 2) == 0;
        const uint8_t red = isMicrophoneStreaming ? heartbeatBrightness : 0;
        const uint8_t blue = isSpeakerStreaming ? heartbeatBrightness : 0;
        const uint8_t green = (!isSpeakerStreaming && !isMicrophoneStreaming) ? heartbeatBrightness : 0;
        const uint32_t color = isOn ? ((red << 16) | (green << 8) | blue) : 0;
        if (color == color_)
        {
            return;
        }
        color_ = color;
        led_.setColor(isOn ? red : 0, isOn ? green : 0, isOn ? blue : 0);
    }

} // namespace rp::led
