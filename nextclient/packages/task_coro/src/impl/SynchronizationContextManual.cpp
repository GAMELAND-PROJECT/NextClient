#include <taskcoro/impl/SynchronizationContextManual.h>

#include <utility>

using namespace taskcoro;

void SynchronizationContextManual::RunTask(std::function<void()> task)
{
    {
        std::scoped_lock lock(mutex_);
        callbacks_queue_.emplace(std::move(task));
    }

    work_available_.notify_one();
}

void SynchronizationContextManual::WaitForWork()
{
    std::unique_lock lock(mutex_);
    work_available_.wait(lock, [this]
    {
        return wake_requested_ || !callbacks_queue_.empty();
    });
    wake_requested_ = false;
}

void SynchronizationContextManual::Wake()
{
    {
        std::scoped_lock lock(mutex_);
        wake_requested_ = true;
    }

    work_available_.notify_one();
}

void SynchronizationContextManual::Update()
{
    std::lock_guard lock_guard(mutex_);

    std::queue<std::function<void()>> callbacks_queue;
    callbacks_queue_.swap(callbacks_queue);

    while (!callbacks_queue.empty())
    {
        auto& callback = callbacks_queue.front();
        callback();

        callbacks_queue.pop();
    }
}
