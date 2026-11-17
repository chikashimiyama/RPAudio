#pragma once

#include <array>
#include <cstdint>

#include "tusb.h"

namespace rp::usb
{

    // Answers UAC2 class requests addressed to the audio control entities: the clock source (fixed 48 kHz) and the
    // speaker feature unit (mute/volume). Mute and volume are stored and reported back but not applied to the signal.
    // Every request is logged with its result.
    class AudioControl
    {
    public:
        // GET request: schedules the response data. Returns false to stall the request.
        bool handleGetRequest(uint8_t rhport, const tusb_control_request_t* request);

        // SET request, called after the data stage. Returns false to stall the request.
        bool handleSetRequest(const tusb_control_request_t* request, const uint8_t* data);

    private:
        static constexpr size_t numFeatureChannels_ = RP_AUDIO_N_CHANNELS + 1; // +1 for master channel 0

        bool handleFeatureUnitGetRequest(uint8_t rhport, const audio_control_request_t* request);
        bool handleFeatureUnitSetRequest(const audio_control_request_t* request, const uint8_t* data);

        std::array<uint8_t, numFeatureChannels_> mute_{};
        std::array<int16_t, numFeatureChannels_> volume_{};
    };

} // namespace rp::usb
