#include "util/PeriodicTimer.h"

#include <utility>

#include "pico/time.h"

namespace rp::util
{

    PeriodicTimer::PeriodicTimer(uint64_t periodUs, Callback callback)
        : periodUs_(periodUs)
        , nextDueUs_(time_us_64() + periodUs)
        , callback_(std::move(callback))
    {
    }

    void PeriodicTimer::update()
    {
        if (time_us_64() < nextDueUs_)
        {
            return;
        }
        nextDueUs_ += periodUs_;
        callback_();
    }

} // namespace rp::util
