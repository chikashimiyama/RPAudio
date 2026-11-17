#include "debug/DebugLog.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "hardware/sync.h"
#include "pico/time.h"
#include "tusb.h"

namespace rp::debug
{

namespace
{

constexpr size_t ringSize = 8192;
constexpr size_t lineSize = 160;

std::array<char, ringSize> ring{};
size_t readIndex = 0;
size_t writeIndex = 0;
size_t droppedBytes = 0;

size_t usedBytes()
{
    return (writeIndex + ringSize - readIndex) % ringSize;
}

// Copies text into the ring buffer, dropping what does not fit. Caller must hold interrupts off.
void pushLocked(const char* text, size_t length)
{
    const size_t freeBytes = ringSize - 1 - usedBytes();
    const size_t bytesToCopy = std::min(length, freeBytes);
    droppedBytes += length - bytesToCopy;
    for (size_t i = 0; i < bytesToCopy; ++i)
    {
        ring[writeIndex] = text[i];
        writeIndex = (writeIndex + 1) % ringSize;
    }
}

void push(const char* text, size_t length)
{
    const uint32_t interruptState = save_and_disable_interrupts();
    pushLocked(text, length);
    restore_interrupts(interruptState);
}

size_t clampedLength(int formattedLength, size_t bufferSize)
{
    if (formattedLength <= 0)
    {
        return 0;
    }
    return std::min(static_cast<size_t>(formattedLength), bufferSize - 1);
}

} // namespace

void log(const char* format, ...)
{
    std::array<char, lineSize> line{};
    const uint32_t timeMs = to_ms_since_boot(get_absolute_time());
    const int prefixLength = std::snprintf(line.data(), line.size(), "[%7lu] ", static_cast<unsigned long>(timeMs));
    size_t length = clampedLength(prefixLength, line.size());

    va_list args;
    va_start(args, format);
    const int bodyLength = std::vsnprintf(line.data() + length, line.size() - length, format, args);
    va_end(args);
    length += clampedLength(bodyLength, line.size() - length);

    // Always end with CRLF, truncating the body if needed.
    length = std::min(length, line.size() - 3);
    line[length++] = '\r';
    line[length++] = '\n';
    push(line.data(), length);
}

void logRawV(const char* format, va_list args)
{
    std::array<char, lineSize> text{};
    const int formattedLength = std::vsnprintf(text.data(), text.size(), format, args);
    push(text.data(), clampedLength(formattedLength, text.size()));
}

void flush()
{
    if (!tud_cdc_connected())
    {
        return;
    }

    if (droppedBytes > 0)
    {
        const uint32_t interruptState = save_and_disable_interrupts();
        const size_t dropped = droppedBytes;
        droppedBytes = 0;
        restore_interrupts(interruptState);
        log("(log overflow: %u bytes dropped)", static_cast<unsigned>(dropped));
    }

    bool hasWritten = false;
    while (true)
    {
        const uint32_t interruptState = save_and_disable_interrupts();
        const size_t contiguous = (writeIndex >= readIndex) ? writeIndex - readIndex : ringSize - readIndex;
        const size_t start = readIndex;
        restore_interrupts(interruptState);

        const size_t chunk = std::min<size_t>(contiguous, tud_cdc_write_available());
        if (chunk == 0)
        {
            break;
        }
        const uint32_t written = tud_cdc_write(ring.data() + start, static_cast<uint32_t>(chunk));
        readIndex = (start + written) % ringSize; // only flush() advances readIndex
        hasWritten = true;
        if (written < chunk)
        {
            break;
        }
    }

    if (hasWritten)
    {
        tud_cdc_write_flush();
    }
}

} // namespace rp::debug

extern "C" int rp_debug_tusb_printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    rp::debug::logRawV(format, args);
    va_end(args);
    return 0;
}
