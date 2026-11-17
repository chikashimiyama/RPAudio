#include "usb/AudioDevice.h"

#include <algorithm>
#include <array>
#include <utility>

#include "hardware/irq.h"
#include "hardware/structs/usb.h"
#include "pico/time.h"
#include "tusb.h"

namespace rp::usb
{

namespace
{

constexpr size_t maxSamplesPerTransfer = CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / sizeof(int16_t);
constexpr uint32_t sampleRate = RP_AUDIO_SAMPLE_RATE;
constexpr uint8_t rootHubPort = 0;

// Feature unit volume range in 1/256 dB: -50 dB .. 0 dB, 1 dB steps
constexpr int16_t volumeMin = -50 * 256;
constexpr int16_t volumeMax = 0;
constexpr int16_t volumeResolution = 256;
constexpr size_t numFeatureChannels = RP_AUDIO_N_CHANNELS + 1; // +1 for master channel 0

std::array<int16_t, maxSamplesPerTransfer> audioBuffer{};
AudioDevice* activeDevice = nullptr;

// Host-controlled speaker mute/volume. Stored and reported back; not applied to the signal.
std::array<uint8_t, numFeatureChannels> mute{};
std::array<int16_t, numFeatureChannels> volume{};

constexpr uint64_t endpointAbortTimeoutUs = 1000;

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

// TinyUSB on RP2xxx allocates ISO endpoints once and never closes them (TUP_DCD_EDPT_ISO_ALLOC). A buffer armed while
// streaming therefore stays AVAILABLE after the host selects alt 0, and the next alt 1 re-arms it, which makes the
// driver panic with "ep XX was already available". Abort the transfer and clear the buffer when streaming stops.
void abortIsoEndpoint(uint8_t endpointAddress)
{
    const uint32_t bit = endpointRegisterBit(endpointAddress);
    const bool wasIrqEnabled = irq_is_enabled(USBCTRL_IRQ);
    irq_set_enabled(USBCTRL_IRQ, false);

    hw_set_bits(&usb_hw->abort, bit);
    const uint64_t deadline = time_us_64() + endpointAbortTimeoutUs;
    while ((usb_hw->abort_done & bit) == 0 && time_us_64() < deadline)
    {
    }
    *endpointBufferControl(endpointAddress) = 0;
    hw_clear_bits(&usb_hw->abort, bit);
    usb_hw->abort_done = bit; // write 1 to clear
    usb_hw->buf_status = bit; // drop a completion that raced with the abort (write 1 to clear)

    irq_set_enabled(USBCTRL_IRQ, wasIrqEnabled);
}

bool handleFeatureUnitGetRequest(uint8_t rhport, const audio_control_request_t* request)
{
    const auto* controlRequest = reinterpret_cast<const tusb_control_request_t*>(request);
    const size_t channel = request->bChannelNumber;
    if (channel >= numFeatureChannels)
    {
        return false;
    }

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR)
    {
        audio_control_cur_1_t current{};
        current.bCur = static_cast<int8_t>(mute[channel]);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &current, sizeof(current));
    }
    if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME)
    {
        if (request->bRequest == AUDIO_CS_REQ_RANGE)
        {
            audio_control_range_2_n_t(1) range{};
            range.wNumSubRanges = tu_htole16(1);
            range.subrange[0].bMin = static_cast<int16_t>(tu_htole16(volumeMin));
            range.subrange[0].bMax = static_cast<int16_t>(tu_htole16(volumeMax));
            range.subrange[0].bRes = static_cast<int16_t>(tu_htole16(volumeResolution));
            return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &range, sizeof(range));
        }
        if (request->bRequest == AUDIO_CS_REQ_CUR)
        {
            audio_control_cur_2_t current{};
            current.bCur = static_cast<int16_t>(tu_htole16(volume[channel]));
            return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &current, sizeof(current));
        }
    }
    return false;
}

bool handleFeatureUnitSetRequest(const audio_control_request_t* request, const uint8_t* buffer)
{
    const size_t channel = request->bChannelNumber;
    if (channel >= numFeatureChannels || request->bRequest != AUDIO_CS_REQ_CUR)
    {
        return false;
    }

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->wLength == sizeof(audio_control_cur_1_t))
    {
        mute[channel] = reinterpret_cast<const audio_control_cur_1_t*>(buffer)->bCur;
        return true;
    }
    if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME && request->wLength == sizeof(audio_control_cur_2_t))
    {
        volume[channel] = reinterpret_cast<const audio_control_cur_2_t*>(buffer)->bCur;
        return true;
    }
    return false;
}

bool handleClockSetRequest(const audio_control_request_t* request, const uint8_t* buffer)
{
    if (request->bControlSelector != AUDIO_CS_CTRL_SAM_FREQ || request->bRequest != AUDIO_CS_REQ_CUR
        || request->wLength != sizeof(audio_control_cur_4_t))
    {
        return false;
    }
    // Only 48 kHz is supported.
    const auto requestedRate = static_cast<uint32_t>(reinterpret_cast<const audio_control_cur_4_t*>(buffer)->bCur);
    return requestedRate == sampleRate;
}

bool handleClockGetRequest(uint8_t rhport, const audio_control_request_t* request)
{
    const auto* controlRequest = reinterpret_cast<const tusb_control_request_t*>(request);

    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ)
    {
        if (request->bRequest == AUDIO_CS_REQ_CUR)
        {
            audio_control_cur_4_t current{};
            current.bCur = static_cast<int32_t>(tu_htole32(sampleRate));
            return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &current, sizeof(current));
        }
        if (request->bRequest == AUDIO_CS_REQ_RANGE)
        {
            audio_control_range_4_n_t(1) range{};
            range.wNumSubRanges = tu_htole16(1);
            range.subrange[0].bMin = static_cast<int32_t>(sampleRate);
            range.subrange[0].bMax = static_cast<int32_t>(sampleRate);
            range.subrange[0].bRes = 0;
            return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &range, sizeof(range));
        }
    }
    else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR)
    {
        audio_control_cur_1_t valid{};
        valid.bCur = 1;
        return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &valid, sizeof(valid));
    }
    return false;
}

} // namespace

AudioDevice::AudioDevice(dsp::IAudioProcessorPtr processor)
    : processor_(std::move(processor))
{
    activeDevice = this;
    tud_init(rootHubPort);
}

AudioDevice::~AudioDevice()
{
    activeDevice = nullptr;
}

void AudioDevice::task()
{
    tud_task();
}

void AudioDevice::onAudioReceived(size_t numBytes)
{
    const size_t bytesToRead = std::min(numBytes, audioBuffer.size() * sizeof(int16_t));
    const uint16_t bytesRead = tud_audio_read(audioBuffer.data(), static_cast<uint16_t>(bytesToRead));
    const size_t numSamples = bytesRead / sizeof(int16_t);

    processor_->process(audioBuffer.data(), audioBuffer.data(), numSamples);

    // Queue for the IN endpoint. If the host is not recording, the FIFO fills up and further data is dropped.
    tud_audio_write(audioBuffer.data(), static_cast<uint16_t>(numSamples * sizeof(int16_t)));
}

} // namespace rp::usb

// ---- TinyUSB audio class callbacks (C linkage via tusb headers) ----

// Post-read: on RP2xxx TinyUSB uses a linear buffer, so the packet only reaches the EP OUT FIFO after the
// pre-read callback. Reading here processes the packet that was just received.
bool tud_audio_rx_done_post_read_cb(uint8_t /*rhport*/, uint16_t n_bytes_received, uint8_t /*func_id*/,
    uint8_t /*ep_out*/, uint8_t /*cur_alt_setting*/)
{
    if (rp::usb::activeDevice == nullptr)
    {
        return true;
    }
    rp::usb::activeDevice->onAudioReceived(n_bytes_received);
    return true;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t* p_request)
{
    const auto* request = reinterpret_cast<const audio_control_request_t*>(p_request);
    if (request->bEntityID == RP_UAC2_ENTITY_CLOCK)
    {
        return rp::usb::handleClockGetRequest(rhport, request);
    }
    if (request->bEntityID == RP_UAC2_ENTITY_SPK_FEATURE_UNIT)
    {
        return rp::usb::handleFeatureUnitGetRequest(rhport, request);
    }
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request, uint8_t* pBuff)
{
    const auto* request = reinterpret_cast<const audio_control_request_t*>(p_request);
    if (request->bEntityID == RP_UAC2_ENTITY_CLOCK)
    {
        return rp::usb::handleClockSetRequest(request, pBuff);
    }
    if (request->bEntityID == RP_UAC2_ENTITY_SPK_FEATURE_UNIT)
    {
        return rp::usb::handleFeatureUnitSetRequest(request, pBuff);
    }
    return false;
}

bool tud_audio_set_itf_cb(uint8_t /*rhport*/, const tusb_control_request_t* /*p_request*/)
{
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request)
{
    // Streaming on this interface stops (alt 0) or restarts: drop the buffer that is still armed in hardware.
    const uint8_t interfaceNumber = tu_u16_low(p_request->wIndex);
    if (interfaceNumber == RP_ITF_NUM_AUDIO_STREAMING_SPK)
    {
        rp::usb::abortIsoEndpoint(RP_EP_AUDIO_OUT);
    }
    else if (interfaceNumber == RP_ITF_NUM_AUDIO_STREAMING_MIC)
    {
        rp::usb::abortIsoEndpoint(RP_EP_AUDIO_IN);
    }
    return true;
}
