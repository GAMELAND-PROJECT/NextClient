#pragma once
#include <functional>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "../SynchronizationContextImplInterface.h"

namespace taskcoro
{
    class SynchronizationContextManual : public SynchronizationContextImplInterface
    {
        std::queue<std::function<void()>> callbacks_queue_{};
        std::recursive_mutex mutex_{};
        std::condition_variable_any work_available_{};
        bool wake_requested_{};

    public:
        void RunTask(std::function<void()> task) override;

        void Update();
        void WaitForWork();
        void Wake();
    };
}
