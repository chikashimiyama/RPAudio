#pragma once

#include <cstdint>

namespace rp::usb
{

    // TinyUSB on RP2xxx allocates ISO endpoints once and never closes them (TUP_DCD_EDPT_ISO_ALLOC). A buffer armed
    // while streaming therefore stays AVAILABLE after the host selects alt 0, and the next alt 1 re-arms it, which
    // makes the driver panic with "ep XX was already available". Call this when streaming on the endpoint stops to
    // abort the transfer and clear the buffer.
    void abortIsoEndpoint(uint8_t endpointAddress);

} // namespace rp::usb
