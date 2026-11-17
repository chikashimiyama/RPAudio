#pragma once

#include <cstdarg>

// Enabled with the CMake option RPDSP_USB_DEBUG_LOG. When disabled, the functions are empty inlines.

namespace rp::debug
{

#if RP_USB_DEBUG_LOG

// Appends a timestamped, formatted line to the in-memory log. Safe to call from interrupts and from inside
// TinyUSB callbacks: it never touches USB itself.
void log(const char* format, ...) __attribute__((format(printf, 1, 2)));

// Appends raw formatted text (no timestamp, no newline).
void logRawV(const char* format, va_list args);

// Sends buffered log text to the debug CDC serial port. Call from the main loop, outside tud_task().
void flush();

#else

inline void log(const char* /*format*/, ...)
{
}

inline void flush()
{
}

#endif

} // namespace rp::debug

#if RP_USB_DEBUG_LOG
// TinyUSB debug output (CFG_TUSB_DEBUG_PRINTF) ends up in the same log.
extern "C" int rp_debug_tusb_printf(const char* format, ...);
#endif
