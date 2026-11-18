#include "usb/AudioDevice.h"

#include <algorithm>
#include <array>
#include <utility>

#include "debug/DebugLog.h"
#include "tusb.h"
#include "usb/AudioControl.h"
#include "usb/IsoEndpoint.h"

namespace rp::usb
{

    namespace
    {

        constexpr uint8_t rootHubPort = 0;
        constexpr uint8_t speakerFifoIndex = 0;
        constexpr size_t maxSamplesPerTransfer = CFG_TUD_AUDIO_FUNC_1_RX_SUPP_SW_FIFO_SZ / sizeof(int16_t);
        constexpr uint16_t bytesPerMillisecond =
            RP_AUDIO_SAMPLE_RATE / 1000 * RP_AUDIO_BYTES_PER_SAMPLE * RP_AUDIO_N_CHANNELS;

        static_assert(maxSamplesPerTransfer <= multicore::DspPipeline::maxBlockSamples,
                      "a speaker transfer must fit in one DSP block");

        // Static rather than members: keeps them off the main stack, where AudioDevice lives.
        std::array<int16_t, maxSamplesPerTransfer> audioBuffer{};
        const std::array<uint8_t, bytesPerMillisecond> silence{};

        AudioDevice* activeDevice = nullptr;

    } // namespace

    AudioDevice::AudioDevice(dsp::IAudioProcessorPtr processor)
        : dspCore_(std::move(processor))
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
        collectProcessedAudio();
    }

    void AudioDevice::logStats()
    {
        streamMonitor_.logStats();
    }

    bool AudioDevice::isSpeakerStreaming() const
    {
        return streamMonitor_.isSpeakerStreaming();
    }

    bool AudioDevice::isMicrophoneStreaming() const
    {
        return streamMonitor_.isMicrophoneStreaming();
    }

    void AudioDevice::onSpeakerPacketReceived(size_t numBytes)
    {
        streamMonitor_.onSpeakerPacket(numBytes);

        const size_t bytesToRead = std::min(numBytes, audioBuffer.size() * sizeof(int16_t));
        const uint16_t bytesRead =
            tud_audio_read_support_ff(speakerFifoIndex, audioBuffer.data(), static_cast<uint16_t>(bytesToRead));
        const size_t numSamples = bytesRead / sizeof(int16_t);

        // Processed on core 1; the result is collected by collectProcessedAudio().
        if (!dspCore_.submit(audioBuffer.data(), numSamples))
        {
            streamMonitor_.onBlockDropped();
        }
    }

    void AudioDevice::collectProcessedAudio()
    {
        // Queue for the IN endpoint. If the host is not recording, the FIFO fills up and further data is dropped.
        dspCore_.drain([](const int16_t* samples, size_t numSamples)
                       { tud_audio_write(samples, static_cast<uint16_t>(numSamples * sizeof(int16_t))); });
    }

    // A real microphone always delivers samples. Without speaker input there is nothing to process, so the IN
    // endpoint would only send zero-length packets; hosts that wait for capture data before rendering (e.g. Pd with
    // MME) then never start sending speaker data. Keep the microphone running with silence while the speaker is idle.
    void AudioDevice::onMicrophonePacketLoading()
    {
        // Last chance for blocks core 1 has just finished to make it into this packet.
        collectProcessedAudio();

        if (!streamMonitor_.isSpeakerIdle())
        {
            return;
        }
        const uint16_t queuedBytes = tu_fifo_count(tud_audio_get_ep_in_ff());
        if (queuedBytes >= bytesPerMillisecond)
        {
            return;
        }
        tud_audio_write(silence.data(), static_cast<uint16_t>(bytesPerMillisecond - queuedBytes));
    }

    void AudioDevice::onMicrophonePacketSent(size_t numBytes)
    {
        streamMonitor_.onMicrophonePacket(numBytes);
    }

    void AudioDevice::onStreamClosing(uint8_t interfaceNumber)
    {
        // Drop the buffer that is still armed in hardware (see abortIsoEndpoint()).
        if (interfaceNumber == RP_ITF_NUM_AUDIO_STREAMING_SPK)
        {
            abortIsoEndpoint(RP_EP_AUDIO_OUT);
        }
        else if (interfaceNumber == RP_ITF_NUM_AUDIO_STREAMING_MIC)
        {
            abortIsoEndpoint(RP_EP_AUDIO_IN);
        }
    }

} // namespace rp::usb

// ---- TinyUSB audio class callbacks (C linkage via tusb headers): forward to the active AudioDevice ----

// Post-read: the packet is decoded into the support FIFO between the pre- and post-read callbacks.
bool tud_audio_rx_done_post_read_cb(uint8_t /*rhport*/, uint16_t n_bytes_received, uint8_t /*func_id*/,
                                    uint8_t /*ep_out*/, uint8_t /*cur_alt_setting*/)
{
    if (rp::usb::activeDevice != nullptr)
    {
        rp::usb::activeDevice->onSpeakerPacketReceived(n_bytes_received);
    }
    return true;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t /*rhport*/, uint8_t /*func_id*/, uint8_t /*ep_in*/,
                                   uint8_t /*cur_alt_setting*/)
{
    if (rp::usb::activeDevice != nullptr)
    {
        rp::usb::activeDevice->onMicrophonePacketLoading();
    }
    return true;
}

bool tud_audio_tx_done_post_load_cb(uint8_t /*rhport*/, uint16_t n_bytes_copied, uint8_t /*func_id*/, uint8_t /*ep_in*/,
                                    uint8_t /*cur_alt_setting*/)
{
    if (rp::usb::activeDevice != nullptr)
    {
        rp::usb::activeDevice->onMicrophonePacketSent(n_bytes_copied);
    }
    return true;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, const tusb_control_request_t* p_request)
{
    return rp::usb::handleAudioControlGetRequest(rhport, p_request);
}

bool tud_audio_set_req_entity_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request, uint8_t* pBuff)
{
    return rp::usb::handleAudioControlSetRequest(p_request, pBuff);
}

// Requests addressed to an endpoint or an interface are not supported; log them so they are visible.
bool tud_audio_get_req_ep_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request)
{
    rp::debug::log("GET ep req=0x%02x wValue=0x%04x wIndex=0x%04x len=%u -> STALL", p_request->bRequest,
                   p_request->wValue, p_request->wIndex, p_request->wLength);
    return false;
}

bool tud_audio_set_req_ep_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request, uint8_t* /*pBuff*/)
{
    rp::debug::log("SET ep req=0x%02x wValue=0x%04x wIndex=0x%04x len=%u -> STALL", p_request->bRequest,
                   p_request->wValue, p_request->wIndex, p_request->wLength);
    return false;
}

bool tud_audio_get_req_itf_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request)
{
    rp::debug::log("GET itf req=0x%02x wValue=0x%04x wIndex=0x%04x len=%u -> STALL", p_request->bRequest,
                   p_request->wValue, p_request->wIndex, p_request->wLength);
    return false;
}

bool tud_audio_set_req_itf_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request, uint8_t* /*pBuff*/)
{
    rp::debug::log("SET itf req=0x%02x wValue=0x%04x wIndex=0x%04x len=%u -> STALL", p_request->bRequest,
                   p_request->wValue, p_request->wIndex, p_request->wLength);
    return false;
}

bool tud_audio_set_itf_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request)
{
    rp::debug::log("SET_INTERFACE itf=%u alt=%u -> OK", tu_u16_low(p_request->wIndex), tu_u16_low(p_request->wValue));
    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t /*rhport*/, const tusb_control_request_t* p_request)
{
    const uint8_t interfaceNumber = tu_u16_low(p_request->wIndex);
    rp::debug::log("close EP itf=%u (new alt=%u)", interfaceNumber, tu_u16_low(p_request->wValue));
    if (rp::usb::activeDevice != nullptr)
    {
        rp::usb::activeDevice->onStreamClosing(interfaceNumber);
    }
    return true;
}

// ---- TinyUSB device state callbacks ----

void tud_mount_cb(void)
{
    rp::debug::log("mounted");
}

void tud_umount_cb(void)
{
    rp::debug::log("unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    rp::debug::log("suspended (remote wakeup %s)", remote_wakeup_en ? "on" : "off");
}

void tud_resume_cb(void)
{
    rp::debug::log("resumed");
}
