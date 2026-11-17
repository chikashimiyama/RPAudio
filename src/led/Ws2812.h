#pragma once

#include <cstdint>

#include "hardware/pio.h"

namespace rp::led
{

    // Single WS2812 RGB LED driven by a PIO state machine.
    class Ws2812
    {
    public:
        explicit Ws2812(uint pin);
        ~Ws2812();

        Ws2812(const Ws2812&) = delete;
        Ws2812& operator=(const Ws2812&) = delete;

        void setColor(uint8_t red, uint8_t green, uint8_t blue);

    private:
        PIO pio_ = nullptr;
        uint stateMachine_ = 0;
        uint programOffset_ = 0;
    };

} // namespace rp::led
