#include "SaveScene.h"

#include "Application.h"
#include "View.h"
#include "Threading/TaskRunner.h"
#include "utils/ColladaParser.h"
#include "../Interfaces/IWindow.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace
{
bool hasControlModifier(Modifier mods)
{
    return (static_cast<int>(mods) & static_cast<int>(Modifier::Control)) != 0;
}

void orbitDown(View &view)
{
    Camera &camera = view.getViewport().getCamera();
    camera.rotate(camera.getTarget(), camera.calcRight(), glm::radians(4.0f));
}
} // namespace

void SaveSceneOperator::onKeyboardInput(View &view, KeyCode key, Action action,
                                        Modifier mods)
{
    (void)key;

    if (hasControlModifier(mods))
    {
        if (action == Action::Press || action == Action::Repeat)
            orbitDown(view);
        return;
    }

    if (action != Action::Press)
        return;

    Model *model = view.getModel();
    if (!model)
        return;

    IWindow *window = view.getWindow();
    if (!window)
        return;

    std::string path;
    if (!window->saveFileDialog(path))
        return; // cancelled

    Application *application = Application::getInstance();
    if (!application)
        return;

    std::cerr << "[Save] Saving scene model to \"" << path << "\" asynchronously..."
              << std::endl;

    auto asyncCtx = model->getAsyncContext();

    // Offload COLLADA XML export / serialization to a worker thread.
    application->getTaskRunner().run(
        [model, path, asyncCtx]()
        {
            std::unique_lock<std::mutex> lock(asyncCtx->mutex);
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;
            saveModel(*model, path);
        },
        [application, path, asyncCtx]()
        {
            if (!asyncCtx->alive.load(std::memory_order_acquire)) return;
            std::cerr << "[Save] Model saved to \"" << path << "\"." << std::endl;
            application->setActiveFilename(path);
        });
}
