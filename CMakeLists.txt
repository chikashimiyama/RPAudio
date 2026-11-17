cmake_minimum_required(VERSION 3.21)

# Board / platform must be set before the SDK is imported.
set(PICO_BOARD sparkfun_promicro_rp2350 CACHE STRING "Target board")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Target platform")

# USB debug log: adds a CDC serial port ("rpdsp Debug") that carries control requests, stream statistics and
# TinyUSB warnings. Changes the USB interface set, so the device uses a different idProduct when enabled.
option(RPDSP_USB_DEBUG_LOG "Expose a USB serial port with the firmware debug log" OFF)

# TinyUSB debug logging level (picked up by TinyUSB's rp2040 family.cmake). Level 1 reports failed asserts and
# warnings; it goes to the in-memory debug log (see CFG_TUSB_DEBUG_PRINTF below), so it needs the USB debug log.
if(RPDSP_USB_DEBUG_LOG)
    set(LOG 1)
else()
    set(LOG 0)
endif()

include(cmake/pico_sdk_import.cmake)

project(rpdsp
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

pico_sdk_init()

add_executable(rpdsp
    src/main.cpp
    src/dsp/BiquadFilter.cpp
    src/dsp/FirFilter.cpp
    src/dsp/RingModulator.cpp
    src/usb/AudioDevice.cpp
    src/usb/AudioControl.cpp
    src/usb/StreamMonitor.cpp
    src/usb/IsoEndpoint.cpp
    src/usb/UsbDescriptors.cpp
    src/led/Ws2812.cpp
    src/led/Heartbeat.cpp
    src/util/PeriodicTimer.cpp
)

if(RPDSP_USB_DEBUG_LOG)
    target_sources(rpdsp PRIVATE src/debug/DebugLog.cpp)
    target_compile_definitions(rpdsp PRIVATE
        RP_USB_DEBUG_LOG=1
        # Route TinyUSB debug messages into the debug log ring buffer
        CFG_TUSB_DEBUG_PRINTF=rp_debug_tusb_printf
    )
else()
    target_compile_definitions(rpdsp PRIVATE RP_USB_DEBUG_LOG=0)
endif()

target_include_directories(rpdsp PRIVATE
    src
    src/usb # tusb_config.h must be on the include path for TinyUSB
)

target_compile_features(rpdsp PRIVATE cxx_std_17)

target_compile_options(rpdsp PRIVATE
    -Wall
    -Wextra
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
)

pico_generate_pio_header(rpdsp ${CMAKE_CURRENT_LIST_DIR}/src/led/ws2812.pio)

target_link_libraries(rpdsp PRIVATE
    pico_stdlib
    pico_unique_id
    pico_usb_reset
    tinyusb_device
    hardware_pio
)

# picotool reset interface (vendor class + MS OS 2.0 descriptor so Windows binds WinUSB to it)
target_compile_definitions(rpdsp PRIVATE
    PICO_ENABLE_USB_RESET_VIA_VENDOR_INTERFACE=1
    PICO_USB_RESET_INCLUDE_DEFAULT_APP_DRIVER_CB=1
    PICO_USB_RESET_SUPPORT_MS_OS_20_DESCRIPTOR=1
    PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=3
)

# USB is owned by the audio device, so stdio goes to UART (GP0/GP1)
pico_enable_stdio_usb(rpdsp 0)
pico_enable_stdio_uart(rpdsp 1)

pico_set_program_name(rpdsp "rpdsp")
pico_set_program_version(rpdsp "${PROJECT_VERSION}")

# Generate .uf2 / .bin / .hex / .map alongside the .elf
pico_add_extra_outputs(rpdsp)
