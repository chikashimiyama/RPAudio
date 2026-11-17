#include "usb/AudioControl.h"

#include "debug/DebugLog.h"

namespace rp::usb
{

    namespace
    {

        constexpr uint32_t sampleRate = RP_AUDIO_SAMPLE_RATE;

        const char* requestName(uint8_t request)
        {
            switch (request)
            {
                case AUDIO_CS_REQ_CUR:
                    return "CUR";
                case AUDIO_CS_REQ_RANGE:
                    return "RANGE";
                default:
                    return "?";
            }
        }

        void logRequest(const char* kind, const tusb_control_request_t* request, bool isAccepted)
        {
            const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);
            rp::debug::log("%s %s entity=0x%02x cs=%u ch=%u len=%u -> %s", kind, requestName(audioRequest->bRequest),
                           audioRequest->bEntityID, audioRequest->bControlSelector, audioRequest->bChannelNumber,
                           request->wLength, isAccepted ? "OK" : "STALL");
        }

        bool handleClockGetRequest(uint8_t rhport, const tusb_control_request_t* request)
        {
            const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);

            if (audioRequest->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ)
            {
                if (audioRequest->bRequest == AUDIO_CS_REQ_CUR)
                {
                    audio_control_cur_4_t current{};
                    current.bCur = static_cast<int32_t>(tu_htole32(sampleRate));
                    return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &current, sizeof(current));
                }
                if (audioRequest->bRequest == AUDIO_CS_REQ_RANGE)
                {
                    audio_control_range_4_n_t(1) range{};
                    range.wNumSubRanges = tu_htole16(1);
                    range.subrange[0].bMin = static_cast<int32_t>(sampleRate);
                    range.subrange[0].bMax = static_cast<int32_t>(sampleRate);
                    range.subrange[0].bRes = 0;
                    return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &range, sizeof(range));
                }
            }
            else if (audioRequest->bControlSelector == AUDIO_CS_CTRL_CLK_VALID &&
                     audioRequest->bRequest == AUDIO_CS_REQ_CUR)
            {
                audio_control_cur_1_t valid{};
                valid.bCur = 1;
                return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &valid, sizeof(valid));
            }
            return false;
        }

        bool handleClockSetRequest(const tusb_control_request_t* request, const uint8_t* data)
        {
            const auto* audioRequest = reinterpret_cast<const audio_control_request_t*>(request);
            if (audioRequest->bControlSelector != AUDIO_CS_CTRL_SAM_FREQ ||
                audioRequest->bRequest != AUDIO_CS_REQ_CUR || request->wLength != sizeof(audio_control_cur_4_t))
            {
                return false;
            }
            // Only 48 kHz is supported.
            const auto requestedRate =
                static_cast<uint32_t>(reinterpret_cast<const audio_control_cur_4_t*>(data)->bCur);
            return requestedRate == sampleRate;
        }

        bool isClockRequest(const tusb_control_request_t* request)
        {
            return reinterpret_cast<const audio_control_request_t*>(request)->bEntityID == RP_UAC2_ENTITY_CLOCK;
        }

    } // namespace

    bool handleAudioControlGetRequest(uint8_t rhport, const tusb_control_request_t* request)
    {
        const bool isAccepted = isClockRequest(request) && handleClockGetRequest(rhport, request);
        logRequest("GET", request, isAccepted);
        return isAccepted;
    }

    bool handleAudioControlSetRequest(const tusb_control_request_t* request, const uint8_t* data)
    {
        const bool isAccepted = isClockRequest(request) && handleClockSetRequest(request, data);
        logRequest("SET", request, isAccepted);
        return isAccepted;
    }

} // namespace rp::usb
