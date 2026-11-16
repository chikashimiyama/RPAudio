#include "usb/AudioDevice.h"

#include <algorithm>
#include <array>
#include <utility>

#include "tusb.h"

namespace rp::usb
{

namespace
{

constexpr size_t maxSamplesPerTransfer = CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / sizeof(int16_t);
constexpr uint32_t sampleRate = RP_AUDIO_SAMPLE_RATE;
constexpr uint8_t rootHubPort = 0;

std::array<int16_t, maxSamplesPerTransfer> audioBuffer{};
AudioDevice* activeDevice = nullptr;

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

bool tud_audio_rx_done_pre_read_cb(uint8_t /*rhport*/, uint16_t n_bytes_received, uint8_t /*func_id*/,
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
    if (request->bEntityID != RP_UAC2_ENTITY_CLOCK)
    {
        return false;
    }
    return rp::usb::handleClockGetRequest(rhport, request);
}

bool tud_audio_set_req_entity_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request, uint8_t* /*pBuff*/)
{
    // The clock is fixed at 48 kHz; accept a SET CUR sample-rate request only if it matches.
    const auto* request = reinterpret_cast<const audio_control_request_t*>(p_request);
    return request->bEntityID == RP_UAC2_ENTITY_CLOCK && request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ
        && request->bRequest == AUDIO_CS_REQ_CUR;
}

bool tud_audio_set_itf_cb(uint8_t /*rhport*/, const tusb_control_request_t* /*p_request*/)
{
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t /*rhport*/, const tusb_control_request_t* /*p_request*/)
{
    return true;
}
