#pragma once

#include <cstdint>

#include "tusb.h"

namespace rp::usb
{

    // UAC2 class requests addressed to an audio control entity. The only entity with controls is the clock source
    // (fixed 48 kHz). Every request is logged with its result. Both return false to stall the request.

    // GET request: schedules the response data.
    bool handleAudioControlGetRequest(uint8_t rhport, const tusb_control_request_t* request);

    // SET request, called after the data stage.
    bool handleAudioControlSetRequest(const tusb_control_request_t* request, const uint8_t* data);

} // namespace rp::usb
