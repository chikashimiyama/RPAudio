#pragma once

// TinyUSB configuration: UAC2 device, mono 16-bit 48 kHz in (speaker) and out (microphone).

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#define CFG_TUSB_OS OPT_OS_PICO
#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

#define CFG_TUD_ENABLED 1
#define CFG_TUD_MAX_SPEED OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE 64

#if RP_USB_DEBUG_LOG
#define CFG_TUD_CDC 1 // debug log serial port
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 1024
#define CFG_TUD_CDC_EP_BUFSIZE 64
#else
#define CFG_TUD_CDC 0
#endif
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_AUDIO 1
#define CFG_TUD_VENDOR 0

// --- Audio function 1 ---
#define RP_AUDIO_SAMPLE_RATE 48000
#define RP_AUDIO_BYTES_PER_SAMPLE 2
#define RP_AUDIO_RESOLUTION 16
#define RP_AUDIO_N_CHANNELS 1

// Descriptor length is defined in UsbDescriptors.h
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN RP_AUDIO_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 2
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64
#define CFG_TUD_AUDIO_FUNC_1_N_FORMATS 1
#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE RP_AUDIO_SAMPLE_RATE

#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX RP_AUDIO_N_CHANNELS
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_TX RP_AUDIO_BYTES_PER_SAMPLE
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_TX RP_AUDIO_RESOLUTION
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX RP_AUDIO_N_CHANNELS
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_N_BYTES_PER_SAMPLE_RX RP_AUDIO_BYTES_PER_SAMPLE
#define CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX RP_AUDIO_RESOLUTION

// Device -> host (microphone)
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX \
    TUD_AUDIO_EP_SIZE(RP_AUDIO_SAMPLE_RATE, RP_AUDIO_BYTES_PER_SAMPLE, RP_AUDIO_N_CHANNELS)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX * 4)

// Host -> device (speaker)
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 1
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX \
    TUD_AUDIO_EP_SIZE(RP_AUDIO_SAMPLE_RATE, RP_AUDIO_BYTES_PER_SAMPLE, RP_AUDIO_N_CHANNELS)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ (CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX * 4)

// Use TinyUSB's decoding path with a support FIFO for OUT. The plain EP OUT FIFO path stops re-arming the endpoint
// after a zero-length packet (TU_VERIFY on tu_fifo_write_n() returning 0), which Windows usbaudio2 sends when a
// stream starts. The decoding path handles zero-length packets and always re-arms.
#define CFG_TUD_AUDIO_ENABLE_DECODING 1
#define CFG_TUD_AUDIO_ENABLE_TYPE_I_DECODING 1
#define CFG_TUD_AUDIO_FUNC_1_CHANNEL_PER_FIFO_RX RP_AUDIO_N_CHANNELS
#define CFG_TUD_AUDIO_FUNC_1_N_RX_SUPP_SW_FIFO 1
// Multiple of 4 bytes: 16-bit mono is decoded in 32-bit words.
#define CFG_TUD_AUDIO_FUNC_1_RX_SUPP_SW_FIFO_SZ (CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX * 4)

#ifdef __cplusplus
}
#endif

#include "UsbDescriptors.h"
