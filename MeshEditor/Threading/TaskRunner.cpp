#include "TaskRunner.h"

#include <algorithm>
#include <iostream>

TaskRunner::TaskRunner(unsigned threadCount, QObject *parent)
    : QObject(parent)
{
    if (threadCount == 0)
        threadCount = std::max(1u, std::thread::hardware_concurrency());

   
    connect(this, &TaskRunner::completed_, this,
            [](std::function<void()> fn) { fn(); },
            Qt::QueuedConnection);

    m_workers.reserve(threadCount);
    for (unsigned i = 0; i < threadCount; ++i)
        m_workers.emplace_back(&TaskRunner::workerLoop, this);

    std::cerr << "[TaskRunner] Started " << threadCount
              << " worker thread(s)." << std::endl;
}

TaskRunner::~TaskRunner()
{
    {
        std::lock_guard lock(m_mutex);
        m_shutdown = true;
    }
    m_cv.notify_all();
    for (auto &t : m_workers)
    {
        if (t.joinable())
            t.join();
    }
}

void TaskRunner::enqueue(std::function<void()> task)
{
    {
        std::lock_guard lock(m_mutex);
        m_tasks.push(std::move(task));
    }
    m_cv.notify_one();
}

void TaskRunner::workerLoop()
{
    for (;;)
    {
        std::function<void()> task;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_shutdown || !m_tasks.empty(); });
            if (m_shutdown && m_tasks.empty())
                return;
            task = std::move(m_tasks.front());
            m_tasks.pop();
        }
        task();
    }
}

// ---- Convenience overloads (void work) ------------------------------------

std::shared_ptr<TaskHandle>
TaskRunner::run(std::function<void(std::shared_ptr<TaskHandle>)> work,
                std::function<void()> onComplete)
{
    return run(
        [w = std::move(work)](std::shared_ptr<TaskHandle> h) -> int
        {
            w(std::move(h));
            return 0;
        },
        [cb = std::move(onComplete)](int) mutable
        {
            if (cb)
                cb();
        });
}

std::shared_ptr<TaskHandle>
TaskRunner::run(std::function<void()> work,
                std::function<void()> onComplete)
{
    return run(
        [w = std::move(work)](std::shared_ptr<TaskHandle>) -> int
        {
            w();
            return 0;
        },
        [cb = std::move(onComplete)](int) mutable
        {
            if (cb)
                cb();
        });
}
