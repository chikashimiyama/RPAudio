#include <memory>

#include "dsp/RingModulator.h"
#include "led/Ws2812.h"
#include "pico/stdlib.h"
#include "usb/AudioDevice.h"

namespace
{

constexpr float carrierFrequency = 440.0f;
constexpr float sampleRate = 48000.0f;

// Heartbeat LED: blinks while the main loop is running. If it stops, the firmware is stuck.
constexpr uint64_t heartbeatHalfPeriodUs = 250'000;
constexpr uint8_t heartbeatBrightness = 16;

class Heartbeat
{
public:
    Heartbeat()
        : led_(PICO_DEFAULT_WS2812_PIN)
    {
        led_.setColor(0, 0, 0);
    }

    void update()
    {
        const bool isOn = ((time_us_64() / heartbeatHalfPeriodUs) % 2) == 0;
        if (isOn == isOn_)
        {
            return;
        }
        isOn_ = isOn;
        led_.setColor(0, isOn ? heartbeatBrightness : 0, 0);
    }

private:
    rp::led::Ws2812 led_;
    bool isOn_ = false;
};

} // namespace

int main()
{
    stdio_init_all();
    Heartbeat heartbeat;

    rp::usb::AudioDevice audioDevice(std::make_unique<rp::dsp::RingModulator>(carrierFrequency, sampleRate));

    while (true)
    {
        audioDevice.task();
        heartbeat.update();
    }
}
