#include "TogglePbrOperator.h"
#include "Application.h"
#include "IRenderSystem.h"
#include <iostream>

void TogglePbrOperator::onKeyboardInput(View &view, KeyCode key, Action action,
                                        Modifier mods)
{
    if (action != Action::Press)
        return;

    IRenderSystem *rs = Application::getInstance() ? Application::getInstance()->getRenderSystem() : nullptr;
    if (!rs)
        return;

    ShadingMode current = rs->getShadingMode();
    if (current == ShadingMode::Pbr)
    {
        rs->setShadingMode(ShadingMode::Standard);
        std::cout << "[Shading] Switched to Standard (Phong / Face Shading)" << std::endl;
    }
    else
    {
        rs->setShadingMode(ShadingMode::Pbr);
        std::cout << "[Shading] Switched to PBR (Physically Based Rendering)" << std::endl;
    }
}
