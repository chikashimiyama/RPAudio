cmake_minimum_required(VERSION 3.21)

# Board / platform must be set before the SDK is imported.
set(PICO_BOARD pico2 CACHE STRING "Target board")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Target platform")

include(cmake/pico_sdk_import.cmake)

project(rpdsp
    VERSION 0.1.0
    LANGUAGES C CXX ASM
)

pico_sdk_init()

add_executable(rpdsp
    src/main.cpp
    src/dsp/RingModulator.cpp
    src/usb/AudioDevice.cpp
    src/usb/UsbDescriptors.cpp
)

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

target_link_libraries(rpdsp PRIVATE
    pico_stdlib
    pico_unique_id
    tinyusb_device
)

# USB is owned by the audio device, so stdio goes to UART (GP0/GP1)
pico_enable_stdio_usb(rpdsp 0)
pico_enable_stdio_uart(rpdsp 1)

pico_set_program_name(rpdsp "rpdsp")
pico_set_program_version(rpdsp "${PROJECT_VERSION}")

# Generate .uf2 / .bin / .hex / .map alongside the .elf
pico_add_extra_outputs(rpdsp)
