#pragma once

#include <QObject>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>


class TaskHandle
{
public:
    void cancel() { m_cancelled.store(true, std::memory_order_release); }
    bool isCancelled() const { return m_cancelled.load(std::memory_order_acquire); }
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }
    bool isDone() const { return m_done.load(std::memory_order_acquire); }

    float progress() const { return m_progress.load(std::memory_order_acquire); }
    void setProgress(float p) { m_progress.store(p, std::memory_order_release); }

private:
    friend class TaskRunner;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_done{false};
    std::atomic<float> m_progress{0.0f};
};

namespace detail
{
    template <typename W, bool AcceptsHandle = std::is_invocable_v<W, std::shared_ptr<TaskHandle>>>
    struct WorkInvoker
    {
        using Result = std::invoke_result_t<W, std::shared_ptr<TaskHandle>>;
        static Result call(W &w, std::shared_ptr<TaskHandle> h)
        {
            return w(h);
        }
    };

    template <typename W>
    struct WorkInvoker<W, false>
    {
        using Result = std::invoke_result_t<W>;
        static Result call(W &w, std::shared_ptr<TaskHandle> /*h*/)
        {
            return w();
        }
    };
} // namespace detail


class TaskRunner : public QObject
{
    Q_OBJECT
public:
    explicit TaskRunner(unsigned threadCount = 0, QObject *parent = nullptr);
    ~TaskRunner() override;

    // ---- Main API -----------------------------------------------------------

    
    template <typename WorkFn, typename CompleteFn>
    auto run(WorkFn &&work, CompleteFn &&onComplete)
        -> std::shared_ptr<TaskHandle>;

    /// Convenience: void work + void callback.
    std::shared_ptr<TaskHandle> run(std::function<void(std::shared_ptr<TaskHandle>)> work,
                                    std::function<void()> onComplete = {});

    /// Convenience: void work (no handle) + void callback.
    std::shared_ptr<TaskHandle> run(std::function<void()> work,
                                    std::function<void()> onComplete = {});

signals:
    /// Internal signal marshalling completion callbacks to the main thread.
    void completed_(std::function<void()> fn);

private:
    void workerLoop();
    void enqueue(std::function<void()> task);

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_shutdown = false;
};

// ---------------------------------------------------------------------------
// Template implementation (must be in the header)
// ---------------------------------------------------------------------------
template <typename WorkFn, typename CompleteFn>
auto TaskRunner::run(WorkFn &&work, CompleteFn &&onComplete)
    -> std::shared_ptr<TaskHandle>
{
    auto handle = std::make_shared<TaskHandle>();

    // Capture work & onComplete by value (moved in).
    auto workCopy = std::forward<WorkFn>(work);
    auto completeCopy = std::forward<CompleteFn>(onComplete);

    enqueue([this, handle,
             w = std::move(workCopy),
             cb = std::move(completeCopy)]() mutable
    {
        if (handle->isCancelled())
        {
            handle->m_done.store(true, std::memory_order_release);
            return;
        }

        handle->m_running.store(true, std::memory_order_release);

        using Invoker = detail::WorkInvoker<decltype(w)>;
        using WorkResult = typename Invoker::Result;

        if constexpr (std::is_void_v<WorkResult>)
        {
            Invoker::call(w, handle);

            handle->m_running.store(false, std::memory_order_release);
            handle->m_done.store(true, std::memory_order_release);

            if constexpr (!std::is_same_v<std::decay_t<CompleteFn>, std::nullptr_t>)
            {
                emit completed_([cb = std::move(cb)]() mutable { cb(); });
            }
        }
        else
        {
            WorkResult result = Invoker::call(w, handle);

            handle->m_running.store(false, std::memory_order_release);
            handle->m_done.store(true, std::memory_order_release);

            if constexpr (!std::is_same_v<std::decay_t<CompleteFn>, std::nullptr_t>)
            {
                auto resultPtr = std::make_shared<WorkResult>(std::move(result));
                emit completed_([cb = std::move(cb), resultPtr]() mutable {
                    cb(std::move(*resultPtr));
                });
            }
        }
    });

    return handle;
}
