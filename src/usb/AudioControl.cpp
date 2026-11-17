#include "usb/AudioControl.h"

#include "debug/DebugLog.h"

namespace rp::usb
{

    namespace
    {

        constexpr uint32_t sampleRate = RP_AUDIO_SAMPLE_RATE;

        // Feature unit volume range in 1/256 dB: -50 dB .. 0 dB, 1 dB steps
        constexpr int16_t volumeMin = -50 * 256;
        constexpr int16_t volumeMax = 0;
        constexpr int16_t volumeResolution = 256;

        const char* requestName(uint8_t request)
        {
            switch (request)
            {
                case AUDIO_CS_REQ_CUR:
                    return "CUR";
                case AUDIO_CS_REQ_RANGE:
                    return "RANGE";
                case AUDIO_CS_REQ_MEM:
                    return "MEM";
                default:
                    return "?";
            }
        }

        void logRequest(const char* kind, const tusb_control_request_t* request, bool isAccepted)
        {
            const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);
            rp::debug::log("%s %s entity=0x%02x itf=%u cs=%u ch=%u len=%u -> %s", kind,
                           requestName(audioRequest->bRequest), audioRequest->bEntityID, audioRequest->bInterface,
                           audioRequest->bControlSelector, audioRequest->bChannelNumber, request->wLength,
                           isAccepted ? "OK" : "STALL");
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
                    return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &current,
                                                                      sizeof(current));
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

        bool handleClockSetRequest(const audio_control_request_t* request, const uint8_t* data)
        {
            if (request->bControlSelector != AUDIO_CS_CTRL_SAM_FREQ || request->bRequest != AUDIO_CS_REQ_CUR ||
                request->wLength != sizeof(audio_control_cur_4_t))
            {
                return false;
            }
            // Only 48 kHz is supported.
            const auto requestedRate =
                static_cast<uint32_t>(reinterpret_cast<const audio_control_cur_4_t*>(data)->bCur);
            return requestedRate == sampleRate;
        }

    } // namespace

    bool AudioControl::handleGetRequest(uint8_t rhport, const tusb_control_request_t* request)
    {
        const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);
        bool isAccepted = false;
        if (audioRequest->bEntityID == RP_UAC2_ENTITY_CLOCK)
        {
            isAccepted = handleClockGetRequest(rhport, audioRequest);
        }
        else if (audioRequest->bEntityID == RP_UAC2_ENTITY_SPK_FEATURE_UNIT)
        {
            isAccepted = handleFeatureUnitGetRequest(rhport, audioRequest);
        }
        logRequest("GET", request, isAccepted);
        return isAccepted;
    }

    bool AudioControl::handleSetRequest(const tusb_control_request_t* request, const uint8_t* data)
    {
        const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);
        bool isAccepted = false;
        if (audioRequest->bEntityID == RP_UAC2_ENTITY_CLOCK)
        {
            isAccepted = handleClockSetRequest(audioRequest, data);
        }
        else if (audioRequest->bEntityID == RP_UAC2_ENTITY_SPK_FEATURE_UNIT)
        {
            isAccepted = handleFeatureUnitSetRequest(audioRequest, data);
        }
        logRequest("SET", request, isAccepted);
        return isAccepted;
    }

    bool AudioControl::handleFeatureUnitGetRequest(uint8_t rhport, const audio_control_request_t* request)
    {
        const auto* controlRequest = reinterpret_cast<const tusb_control_request_t*>(request);
        const size_t channel = request->bChannelNumber;
        if (channel >= numFeatureChannels_)
        {
            return false;
        }

        if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR)
        {
            audio_control_cur_1_t current{};
            current.bCur = static_cast<int8_t>(mute_[channel]);
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
                current.bCur = static_cast<int16_t>(tu_htole16(volume_[channel]));
                return tud_audio_buffer_and_schedule_control_xfer(rhport, controlRequest, &current, sizeof(current));
            }
        }
        return false;
    }

    bool AudioControl::handleFeatureUnitSetRequest(const audio_control_request_t* request, const uint8_t* data)
    {
        const size_t channel = request->bChannelNumber;
        if (channel >= numFeatureChannels_ || request->bRequest != AUDIO_CS_REQ_CUR)
        {
            return false;
        }

        if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->wLength == sizeof(audio_control_cur_1_t))
        {
            mute_[channel] = reinterpret_cast<const audio_control_cur_1_t*>(data)->bCur;
            return true;
        }
        if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME && request->wLength == sizeof(audio_control_cur_2_t))
        {
            volume_[channel] = reinterpret_cast<const audio_control_cur_2_t*>(data)->bCur;
            return true;
        }
        return false;
    }

} // namespace rp::usb
