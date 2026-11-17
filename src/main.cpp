#include <memory>

#include "debug/DebugLog.h"
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
constexpr uint64_t statsPeriodUs = 1'000'000;

class Heartbeat
{
public:
    Heartbeat()
        : led_(PICO_DEFAULT_WS2812_PIN)
    {
        led_.setColor(0, 0, 0);
    }

    // Blink color: green = idle, blue = speaker data arriving, red = microphone polled, magenta = both.
    void update(bool isSpeakerStreaming, bool isMicrophoneStreaming)
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

private:
    rp::led::Ws2812 led_;
    uint32_t color_ = 0;
};

} // namespace

int main()
{
    stdio_init_all();
    Heartbeat heartbeat;

    rp::usb::AudioDevice audioDevice(std::make_unique<rp::dsp::RingModulator>(carrierFrequency, sampleRate));

    rp::debug::log("rpdsp started");
    uint64_t nextStatsUs = time_us_64() + statsPeriodUs;

    while (true)
    {
        audioDevice.task();
        heartbeat.update(audioDevice.isSpeakerStreaming(), audioDevice.isMicrophoneStreaming());

        if (time_us_64() >= nextStatsUs)
        {
            nextStatsUs += statsPeriodUs;
            audioDevice.logStats();
        }
        rp::debug::flush();
    }
}
