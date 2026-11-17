#include "usb/UsbDescriptors.h"

#include <algorithm>
#include <cstring>

#include "pico/unique_id.h"
#include "pico/usb_reset.h"
#include "tusb.h"

namespace
{

    constexpr uint8_t epAudioOut = RP_EP_AUDIO_OUT;
    constexpr uint8_t epAudioIn = RP_EP_AUDIO_IN;

#if RP_USB_DEBUG_LOG
    constexpr uint16_t productId = 0x4011; // separate idProduct: the interface set differs (extra CDC port)
    constexpr uint16_t debugCdcDescriptorLength = TUD_CDC_DESC_LEN;
    constexpr uint8_t debugCdcNotifySize = 8;
    constexpr uint8_t debugCdcBulkSize = 64;
#else
    constexpr uint16_t productId = 0x4010;
    constexpr uint16_t debugCdcDescriptorLength = 0;
#endif

    enum StringIndex : uint8_t
    {
        LangId = 0,
        Manufacturer,
        Product,
        Serial,
        SpeakerInterface,
        MicrophoneInterface,
        ResetInterface,
#if RP_USB_DEBUG_LOG
        DebugInterface,
#endif
        Count
    };

    const tusb_desc_device_t deviceDescriptor = {
        sizeof(tusb_desc_device_t), // bLength
        TUSB_DESC_DEVICE, // bDescriptorType
        0x0210, // bcdUSB (2.1 for BOS / MS OS 2.0 descriptor)
        TUSB_CLASS_MISC, // bDeviceClass (IAD)
        MISC_SUBCLASS_COMMON, // bDeviceSubClass
        MISC_PROTOCOL_IAD, // bDeviceProtocol
        CFG_TUD_ENDPOINT0_SIZE, // bMaxPacketSize0
        0xCAFE, // idVendor  (TinyUSB test VID - replace for production)
        productId, // idProduct
        0x0102, // bcdDevice (bump when the descriptor layout changes)
        StringIndex::Manufacturer, // iManufacturer
        StringIndex::Product, // iProduct
        StringIndex::Serial, // iSerialNumber
        1 // bNumConfigurations
    };

    constexpr uint16_t configTotalLength =
        TUD_CONFIG_DESC_LEN + RP_AUDIO_DESC_LEN + TUD_RPI_RESET_DESC_LEN + debugCdcDescriptorLength;

    constexpr uint16_t audioEpSize =
        TUD_AUDIO_EP_SIZE(RP_AUDIO_SAMPLE_RATE, RP_AUDIO_BYTES_PER_SAMPLE, RP_AUDIO_N_CHANNELS);

    const uint8_t configurationDescriptor[] = {
        TUD_CONFIG_DESCRIPTOR(1, RP_ITF_NUM_TOTAL, 0, configTotalLength, 0x00, 100),

        // Interface Association Descriptor (audio interfaces only)
        TUD_AUDIO_DESC_IAD(RP_ITF_NUM_AUDIO_CONTROL, RP_ITF_NUM_AUDIO_TOTAL, 0x00),
        // Standard AC interface (no interrupt EP)
        TUD_AUDIO_DESC_STD_AC(RP_ITF_NUM_AUDIO_CONTROL, 0x00, StringIndex::Product),
        // Class-specific AC interface header
        TUD_AUDIO_DESC_CS_AC(0x0200, AUDIO_FUNC_HEADSET,
                             TUD_AUDIO_DESC_CLK_SRC_LEN + 2 * TUD_AUDIO_DESC_INPUT_TERM_LEN +
                                 2 * TUD_AUDIO_DESC_OUTPUT_TERM_LEN,
                             AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),
        // Internal programmable clock: frequency read/write, validity read (same as TinyUSB uac2_headset)
        TUD_AUDIO_DESC_CLK_SRC(RP_UAC2_ENTITY_CLOCK, AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK,
                               (AUDIO_CTRL_RW << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS) |
                                   (AUDIO_CTRL_R << AUDIO_CLOCK_SOURCE_CTRL_CLK_VAL_POS),
                               0x00, 0x00),

        // Speaker path: USB streaming -> speaker (no feature unit: the host applies volume/mute in software)
        TUD_AUDIO_DESC_INPUT_TERM(RP_UAC2_ENTITY_SPK_INPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 0x00,
                                  RP_UAC2_ENTITY_CLOCK, RP_AUDIO_N_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, 0x00,
                                  0x0000, 0x00),
        TUD_AUDIO_DESC_OUTPUT_TERM(RP_UAC2_ENTITY_SPK_OUTPUT_TERM, AUDIO_TERM_TYPE_OUT_GENERIC_SPEAKER, 0x00,
                                   RP_UAC2_ENTITY_SPK_INPUT_TERM, RP_UAC2_ENTITY_CLOCK, 0x0000, 0x00),

        // Microphone path: microphone -> USB streaming
        TUD_AUDIO_DESC_INPUT_TERM(RP_UAC2_ENTITY_MIC_INPUT_TERM, AUDIO_TERM_TYPE_IN_GENERIC_MIC, 0x00,
                                  RP_UAC2_ENTITY_CLOCK, RP_AUDIO_N_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, 0x00,
                                  0x0000, 0x00),
        TUD_AUDIO_DESC_OUTPUT_TERM(RP_UAC2_ENTITY_MIC_OUTPUT_TERM, AUDIO_TERM_TYPE_USB_STREAMING, 0x00,
                                   RP_UAC2_ENTITY_MIC_INPUT_TERM, RP_UAC2_ENTITY_CLOCK, 0x0000, 0x00),

        // Speaker streaming interface: alt 0 (zero bandwidth), alt 1 (streaming)
        TUD_AUDIO_DESC_STD_AS_INT(RP_ITF_NUM_AUDIO_STREAMING_SPK, 0x00, 0x00, StringIndex::SpeakerInterface),
        TUD_AUDIO_DESC_STD_AS_INT(RP_ITF_NUM_AUDIO_STREAMING_SPK, 0x01, 0x01, StringIndex::SpeakerInterface),
        TUD_AUDIO_DESC_CS_AS_INT(RP_UAC2_ENTITY_SPK_INPUT_TERM, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I,
                                 AUDIO_DATA_FORMAT_TYPE_I_PCM, RP_AUDIO_N_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
                                 0x00),
        TUD_AUDIO_DESC_TYPE_I_FORMAT(RP_AUDIO_BYTES_PER_SAMPLE, RP_AUDIO_RESOLUTION),
        TUD_AUDIO_DESC_STD_AS_ISO_EP(
            epAudioOut, (uint8_t) (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ADAPTIVE | TUSB_ISO_EP_ATT_DATA),
            audioEpSize, 0x01),
        TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO_CTRL_NONE,
                                    AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, 0x0001),

        // Microphone streaming interface: alt 0 (zero bandwidth), alt 1 (streaming)
        TUD_AUDIO_DESC_STD_AS_INT(RP_ITF_NUM_AUDIO_STREAMING_MIC, 0x00, 0x00, StringIndex::MicrophoneInterface),
        TUD_AUDIO_DESC_STD_AS_INT(RP_ITF_NUM_AUDIO_STREAMING_MIC, 0x01, 0x01, StringIndex::MicrophoneInterface),
        TUD_AUDIO_DESC_CS_AS_INT(RP_UAC2_ENTITY_MIC_OUTPUT_TERM, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I,
                                 AUDIO_DATA_FORMAT_TYPE_I_PCM, RP_AUDIO_N_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED,
                                 0x00),
        TUD_AUDIO_DESC_TYPE_I_FORMAT(RP_AUDIO_BYTES_PER_SAMPLE, RP_AUDIO_RESOLUTION),
        TUD_AUDIO_DESC_STD_AS_ISO_EP(
            epAudioIn, (uint8_t) (TUSB_XFER_ISOCHRONOUS | TUSB_ISO_EP_ATT_ASYNCHRONOUS | TUSB_ISO_EP_ATT_DATA),
            audioEpSize, 0x01),
        TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO_CTRL_NONE,
                                    AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, 0x0000),

        // Vendor reset interface so `picotool reboot -f` / `picotool load -f` can enter BOOTSEL
        TUD_RPI_RESET_DESCRIPTOR(RP_ITF_NUM_RESET, StringIndex::ResetInterface),

#if RP_USB_DEBUG_LOG
        // Debug log serial port (CDC ACM, has its own IAD)
        TUD_CDC_DESCRIPTOR(RP_ITF_NUM_DEBUG_CDC, StringIndex::DebugInterface, RP_EP_DEBUG_CDC_NOTIFY,
                           debugCdcNotifySize, RP_EP_DEBUG_CDC_OUT, RP_EP_DEBUG_CDC_IN, debugCdcBulkSize),
#endif
    };

    static_assert(sizeof(configurationDescriptor) == configTotalLength, "Configuration descriptor length mismatch");

    const char* const strings[StringIndex::Count] = {
        nullptr, // LangId (handled separately)
        "rp", // Manufacturer
        "rpdsp", // Product
        nullptr, // Serial (unique board ID)
        "rpdsp Input", // Speaker interface (host -> device)
        "rpdsp Output", // Microphone interface (device -> host)
        "Reset", // picotool reset interface
#if RP_USB_DEBUG_LOG
        "rpdsp Debug", // debug log serial port
#endif
    };

    constexpr size_t maxStringChars = 32;
    uint16_t stringBuffer[maxStringChars + 1];

    size_t copyAsciiToUtf16(const char* text, uint16_t* destination, size_t maxChars)
    {
        const size_t length = std::min(std::strlen(text), maxChars);
        for (size_t i = 0; i < length; ++i)
        {
            destination[i] = static_cast<uint8_t>(text[i]);
        }
        return length;
    }

} // namespace

// ---- TinyUSB descriptor callbacks (C linkage via tusb headers) ----

const uint8_t* tud_descriptor_device_cb(void)
{
    return reinterpret_cast<const uint8_t*>(&deviceDescriptor);
}

const uint8_t* tud_descriptor_configuration_cb(uint8_t /*index*/)
{
    return configurationDescriptor;
}

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t /*langid*/)
{
    size_t charCount = 0;

    if (index == StringIndex::LangId)
    {
        stringBuffer[1] = 0x0409; // English (US)
        charCount = 1;
    }
    else if (index == StringIndex::Serial)
    {
        char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
        pico_get_unique_board_id_string(serial, sizeof(serial));
        charCount = copyAsciiToUtf16(serial, stringBuffer + 1, maxStringChars);
    }
    else if (index < StringIndex::Count)
    {
        charCount = copyAsciiToUtf16(strings[index], stringBuffer + 1, maxStringChars);
    }
    else
    {
        return nullptr;
    }

    stringBuffer[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * charCount + 2));
    return stringBuffer;
}
