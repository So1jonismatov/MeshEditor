#include "Operator.h"
#include "Application.h"
#include "Threading/TaskRunner.h"
#include "View/View.h"
#include "Model/Graph/Model.h"

#include <mutex>

Operator::~Operator() {}

void Operator::onEnter(View &) {}
void Operator::onExit(View &) {}
void Operator::onUpdate(View &) {}
void Operator::onMouseMove(View &view, double x, double y)
{
    (void)view;
    (void)x;
    (void)y;
}
void Operator::onMouseInput(View &view, ButtonCode button, Action action,
                            Modifier mods, double x, double y)
{
    (void)view;
    (void)button;
    (void)action;
    (void)mods;
    (void)x;
    (void)y;
}
void Operator::onKeyboardInput(View &view, KeyCode key, Action action,
                               Modifier mods)
{
    (void)view;
    (void)key;
    (void)action;
    (void)mods;
}

bool Operator::isExclusiveTool() const
{
    return false;
}

bool Operator::consumesKey(KeyCode key) const
{
    (void)key;
    return false;
}

bool Operator::consumesMouseInput(ButtonCode button) const
{
    (void)button;
    return false;
}

void Operator::runAsync(View &view, std::function<void()> backgroundWork, std::function<void()> mainThreadCallback)
{
    Model *model = view.getModel();
    Application *app = Application::getInstance();
    if (!model || !app) return;

    auto asyncCtx = model->getAsyncContext();
    app->getTaskRunner().run(
        [model, asyncCtx, work = std::move(backgroundWork)]()
        {
            if (asyncCtx->alive.load(std::memory_order_acquire))
                model->setBusy(true);
            else
                return;
            // The work function is responsible for its own brief locking
            // (snapshot under lock, compute without lock, adopt under lock).
            // Do NOT hold asyncCtx->mutex for the entire duration — that
            // blocks the render thread and can cause frame drops or deadlocks.
            work();
            if (asyncCtx->alive.load(std::memory_order_acquire))
                model->setBusy(false);
        },
        [asyncCtx, cb = std::move(mainThreadCallback)]()
        {
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;
            if (cb) cb();
        });
}
