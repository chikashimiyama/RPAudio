#include "led/Ws2812.h"

#include <array>

#include "hardware/clocks.h"

namespace rp::led
{

namespace
{

// WS2812 PIO program (pico-examples ws2812.pio, BSD-3-Clause), pre-assembled so the build does not need pioasm.
//
//   .side_set 1
//   bitloop:
//       out x, 1       side 0 [T3 - 1]
//       jmp !x do_zero side 1 [T1 - 1]
//   do_one:
//       jmp  bitloop   side 1 [T2 - 1]
//   do_zero:
//       nop            side 0 [T2 - 1]
constexpr uint t1 = 2;
constexpr uint t2 = 5;
constexpr uint t3 = 3;
constexpr uint wrapTarget = 0;
constexpr uint wrap = 3;
constexpr uint sideSetBits = 1;

const std::array<uint16_t, 4> programInstructions = {
    0x6221, // out x, 1       side 0 [2]
    0x1123, // jmp !x, 3      side 1 [1]
    0x1400, // jmp 0          side 1 [4]
    0xa442, // nop            side 0 [4]
};

const pio_program_t program = {
    programInstructions.data(),
    static_cast<uint8_t>(programInstructions.size()),
    -1, // origin: relocatable
    0,  // pio_version: runs on any PIO
#if PICO_PIO_VERSION > 0
    0, // used_gpio_ranges: none hard-coded (pins come from the state machine config)
#endif
};

constexpr float bitRateHz = 800000.0f;
constexpr uint bitsPerPixel = 24;

} // namespace

Ws2812::Ws2812(uint pin)
{
    const bool isClaimed =
        pio_claim_free_sm_and_add_program_for_gpio_range(&program, &pio_, &stateMachine_, &programOffset_, pin, 1, true);
    hard_assert(isClaimed);

    pio_gpio_init(pio_, pin);
    pio_sm_set_consecutive_pindirs(pio_, stateMachine_, pin, 1, true);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, programOffset_ + wrapTarget, programOffset_ + wrap);
    sm_config_set_sideset(&config, sideSetBits, false, false);
    sm_config_set_sideset_pins(&config, pin);
    sm_config_set_out_shift(&config, false, true, bitsPerPixel);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_TX);

    constexpr uint cyclesPerBit = t1 + t2 + t3;
    const float clockDivider = static_cast<float>(clock_get_hz(clk_sys)) / (bitRateHz * cyclesPerBit);
    sm_config_set_clkdiv(&config, clockDivider);

    pio_sm_init(pio_, stateMachine_, programOffset_, &config);
    pio_sm_set_enabled(pio_, stateMachine_, true);
}

Ws2812::~Ws2812()
{
    pio_remove_program_and_unclaim_sm(&program, pio_, stateMachine_, programOffset_);
}

void Ws2812::setColor(uint8_t red, uint8_t green, uint8_t blue)
{
    // WS2812 expects GRB, MSB first, left-aligned in the 32-bit word.
    const uint32_t grb = (static_cast<uint32_t>(green) << 16) | (static_cast<uint32_t>(red) << 8) | blue;
    pio_sm_put_blocking(pio_, stateMachine_, grb << 8u);
}

} // namespace rp::led
