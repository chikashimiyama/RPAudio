#include <memory>

#include "Consts.h"
#include "debug/DebugLog.h"
#include "dsp/RingModulator.h"
#include "led/Heartbeat.h"
#include "pico/stdlib.h"
#include "usb/AudioDevice.h"
#include "util/PeriodicTimer.h"

int main()
{
    stdio_init_all();
    rp::led::Heartbeat heartbeat(PICO_DEFAULT_WS2812_PIN);

    rp::usb::AudioDevice audioDevice(std::make_unique<rp::dsp::RingModulator>(rp::carrierFrequency, rp::sampleRate));

    rp::debug::log("rpdsp started");
    rp::util::PeriodicTimer statsTimer(rp::statsPeriodUs, [&audioDevice]() { audioDevice.logStats(); });

    while (true)
    {
        audioDevice.task();
        heartbeat.update(audioDevice.isSpeakerStreaming(), audioDevice.isMicrophoneStreaming());
        statsTimer.update();
        rp::debug::flush();
    }
}
