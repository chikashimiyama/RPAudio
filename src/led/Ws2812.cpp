#include "led/Ws2812.h"

#include "hardware/clocks.h"
#include "ws2812.pio.h"

namespace rp::led
{

    namespace
    {

        constexpr float bitRateHz = 800000.0f;
        constexpr uint bitsPerPixel = 24;

    } // namespace

    Ws2812::Ws2812(uint pin)
    {
        const bool isClaimed = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio_, &stateMachine_,
                                                                                &programOffset_, pin, 1, true);
        hard_assert(isClaimed);

        pio_gpio_init(pio_, pin);
        pio_sm_set_consecutive_pindirs(pio_, stateMachine_, pin, 1, true);

        // The generated default config already sets wrap and side-set.
        pio_sm_config config = ws2812_program_get_default_config(programOffset_);
        sm_config_set_sideset_pins(&config, pin);
        sm_config_set_out_shift(&config, false, true, bitsPerPixel);
        sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

        constexpr uint cyclesPerBit = ws2812_T1 + ws2812_T2 + ws2812_T3;
        const float clockDivider = static_cast<float>(clock_get_hz(clk_sys)) / (bitRateHz * cyclesPerBit);
        sm_config_set_clkdiv(&config, clockDivider);

        pio_sm_init(pio_, stateMachine_, programOffset_, &config);
        pio_sm_set_enabled(pio_, stateMachine_, true);
    }

    Ws2812::~Ws2812()
    {
        pio_remove_program_and_unclaim_sm(&ws2812_program, pio_, stateMachine_, programOffset_);
    }

    void Ws2812::setColor(uint8_t red, uint8_t green, uint8_t blue)
    {
        // WS2812 expects GRB, MSB first, left-aligned in the 32-bit word.
        const uint32_t grb = (static_cast<uint32_t>(green) << 16) | (static_cast<uint32_t>(red) << 8) | blue;
        pio_sm_put_blocking(pio_, stateMachine_, grb << 8u);
    }

} // namespace rp::led
