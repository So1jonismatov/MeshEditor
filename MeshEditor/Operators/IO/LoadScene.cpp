#include "LoadScene.h"

#include "Application.h"
#include "View.h"
#include "Model/IO/SceneIO.h"
#include "View/ViewUtils.h"
#include "Threading/TaskRunner.h"
#include "../Interfaces/IWindow.h"

#include <iostream>
#include <utility>

void LoadSceneOperator::onKeyboardInput(View &view, KeyCode key, Action action,
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

    std::cerr << "[Load] Loading \"" << path << "\" asynchronously..."
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
                std::cerr << "[Load] Failed to load \"" << path << "\"."
                          << std::endl;
                return;
            }

            Application *app = Application::getInstance();
            if (!app)
                return;

            std::cerr << "[Load] \"" << path << "\" loaded, creating document tab."
                      << std::endl;

            app->addDocument(std::move(loadedModel), path);
        });
}
