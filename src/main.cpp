#include <memory>

#include "dsp/RingModulator.h"
#include "pico/stdlib.h"
#include "usb/AudioDevice.h"

namespace
{

constexpr float carrierFrequency = 440.0f;
constexpr float sampleRate = 48000.0f;

} // namespace

int main()
{
    stdio_init_all();

    rp::usb::AudioDevice audioDevice(std::make_unique<rp::dsp::RingModulator>(carrierFrequency, sampleRate));

    while (true)
    {
        audioDevice.task();
    }
}
