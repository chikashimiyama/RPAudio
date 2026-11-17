#pragma once

#include <cstdint>
#include <functional>

namespace rp::util
{

    // Non-blocking periodic timer for the main loop: call update() repeatedly and the callback runs once per elapsed
    // period. The schedule advances by whole periods, so the interval does not drift with loop timing.
    class PeriodicTimer
    {
    public:
        using Callback = std::function<void()>;

        PeriodicTimer(uint64_t periodUs, Callback callback);

        void update();

    private:
        uint64_t periodUs_;
        uint64_t nextDueUs_;
        Callback callback_;
    };

} // namespace rp::util
