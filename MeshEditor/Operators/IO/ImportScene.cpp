#include "ImportScene.h"

#include "Application.h"
#include "View.h"
#include "Model/IO/SceneIO.h"
#include "Threading/TaskRunner.h"
#include "../Interfaces/IWindow.h"

#include <iostream>
#include <utility>

void ImportSceneOperator::onKeyboardInput(View &view, KeyCode key, Action action,
                                         Modifier mods)
{
    (void)key;
    (void)mods;

    if (action != Action::Press)
        return;

    IWindow *window = view.getWindow();
    if (!window)
        return;

    std::string path;
    if (!window->openFileDialog(path))
        return;

    Application *application = Application::getInstance();
    if (!application)
        return;

    std::cerr << "[Import] Importing \"" << path << "\" into active scene..."
              << std::endl;

    application->getTaskRunner().run(
        [path](std::shared_ptr<TaskHandle> handle) -> std::unique_ptr<Model>
        {
            (void)handle;
            return loadSceneModel(path);
        },
        [path](std::unique_ptr<Model> loadedModel)
        {
            if (!loadedModel)
            {
                std::cerr << "[Import] Failed to import \"" << path << "\"."
                          << std::endl;
                return;
            }

            Application *app = Application::getInstance();
            if (!app)
                return;

            std::cerr << "[Import] \"" << path << "\" imported into active scene."
                      << std::endl;

            app->importModelIntoCurrentScene(std::move(loadedModel));
        });
}
