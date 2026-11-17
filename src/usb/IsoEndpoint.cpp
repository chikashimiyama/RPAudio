#include "usb/IsoEndpoint.h"

#include "hardware/irq.h"
#include "hardware/structs/usb.h"
#include "pico/time.h"
#include "tusb.h"

namespace rp::usb
{

    namespace
    {

        constexpr uint64_t abortTimeoutUs = 1000;

        // Bit of an endpoint in the BUF_STATUS / EP_ABORT / EP_ABORT_DONE registers: EP0_IN, EP0_OUT, EP1_IN, ...
        uint32_t endpointRegisterBit(uint8_t endpointAddress)
        {
            const uint32_t endpointNumber = tu_edpt_number(endpointAddress);
            const bool isIn = tu_edpt_dir(endpointAddress) == TUSB_DIR_IN;
            return 1u << (endpointNumber * 2u + (isIn ? 0u : 1u));
        }

        io_rw_32* endpointBufferControl(uint8_t endpointAddress)
        {
            const uint8_t endpointNumber = tu_edpt_number(endpointAddress);
            if (tu_edpt_dir(endpointAddress) == TUSB_DIR_IN)
            {
                return &usb_dpram->ep_buf_ctrl[endpointNumber].in;
            }
            return &usb_dpram->ep_buf_ctrl[endpointNumber].out;
        }

    } // namespace

    void abortIsoEndpoint(uint8_t endpointAddress)
    {
        const uint32_t bit = endpointRegisterBit(endpointAddress);
        const bool wasIrqEnabled = irq_is_enabled(USBCTRL_IRQ);
        irq_set_enabled(USBCTRL_IRQ, false);

        hw_set_bits(&usb_hw->abort, bit);
        const uint64_t deadline = time_us_64() + abortTimeoutUs;
        while ((usb_hw->abort_done & bit) == 0 && time_us_64() < deadline)
        {
        }
        *endpointBufferControl(endpointAddress) = 0;
        hw_clear_bits(&usb_hw->abort, bit);
        usb_hw->abort_done = bit; // write 1 to clear
        usb_hw->buf_status = bit; // drop a completion that raced with the abort (write 1 to clear)

        irq_set_enabled(USBCTRL_IRQ, wasIrqEnabled);
    }

} // namespace rp::usb
