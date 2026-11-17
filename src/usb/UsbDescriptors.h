#pragma once

// Included from tusb_config.h, so this header must not include tusb.h itself.

// UAC2 entity IDs (arbitrary, unique)
#define RP_UAC2_ENTITY_CLOCK 0x04
#define RP_UAC2_ENTITY_SPK_INPUT_TERM 0x01
#define RP_UAC2_ENTITY_SPK_FEATURE_UNIT 0x02
#define RP_UAC2_ENTITY_SPK_OUTPUT_TERM 0x03
#define RP_UAC2_ENTITY_MIC_INPUT_TERM 0x11
#define RP_UAC2_ENTITY_MIC_OUTPUT_TERM 0x13

#define RP_ITF_NUM_AUDIO_CONTROL 0
#define RP_ITF_NUM_AUDIO_STREAMING_SPK 1
#define RP_ITF_NUM_AUDIO_STREAMING_MIC 2
#define RP_ITF_NUM_AUDIO_TOTAL 3
#define RP_ITF_NUM_RESET 3 // picotool reset interface (outside the audio IAD)
#if RP_USB_DEBUG_LOG
#define RP_ITF_NUM_DEBUG_CDC 4 // debug log serial port (2 interfaces)
#define RP_ITF_NUM_TOTAL 6
#else
#define RP_ITF_NUM_TOTAL 4
#endif

#define RP_EP_AUDIO_OUT 0x01 // speaker stream (host -> device)
#define RP_EP_AUDIO_IN 0x81 // microphone stream (device -> host)
#define RP_EP_DEBUG_CDC_NOTIFY 0x82
#define RP_EP_DEBUG_CDC_OUT 0x03
#define RP_EP_DEBUG_CDC_IN 0x83

// clang-format off: one descriptor per line, grouped like the configuration descriptor in UsbDescriptors.cpp
#define RP_AUDIO_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN \
    + TUD_AUDIO_DESC_STD_AC_LEN \
    + TUD_AUDIO_DESC_CS_AC_LEN \
    + TUD_AUDIO_DESC_CLK_SRC_LEN \
    + TUD_AUDIO_DESC_INPUT_TERM_LEN \
    + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN \
    + TUD_AUDIO_DESC_OUTPUT_TERM_LEN \
    + TUD_AUDIO_DESC_INPUT_TERM_LEN \
    + TUD_AUDIO_DESC_OUTPUT_TERM_LEN \
    /* Speaker AS: alt 0, alt 1 */ \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_CS_AS_INT_LEN \
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN \
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN \
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN \
    /* Microphone AS: alt 0, alt 1 */ \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_STD_AS_INT_LEN \
    + TUD_AUDIO_DESC_CS_AS_INT_LEN \
    + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN \
    + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN \
    + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN)
// clang-format on
