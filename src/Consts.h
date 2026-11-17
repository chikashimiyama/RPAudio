#pragma once

#include <cstdint>

// Application settings. Module internals (USB register bits, buffer sizes, PIO timings) stay in their .cpp files.

namespace rp
{

    inline constexpr float sampleRate = 48000.0f; // must match RP_AUDIO_SAMPLE_RATE in usb/tusb_config.h

    // Ring modulator
    inline constexpr float carrierFrequency = 440.0f;

    // Lowpass filter
    inline constexpr float lowpassCutoffFrequency = 300.0f;
    inline constexpr float lowpassQ = 0.70710678f; // 1/sqrt(2): Butterworth, no resonance peak

    // Debug log: interval of the stream statistics line
    inline constexpr uint64_t statsPeriodUs = 1'000'000;

    // Status LED
    inline constexpr uint64_t heartbeatHalfPeriodUs = 250'000;
    inline constexpr uint8_t heartbeatBrightness = 16;

} // namespace rp
