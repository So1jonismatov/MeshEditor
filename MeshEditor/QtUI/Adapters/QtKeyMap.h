#pragma once

#include "../../Interfaces/Keys.h"

#include <Qt>

// Translates Qt input codes into the engine's GLFW-style codes so the
// existing OperatorDispatcher bindings keep working unchanged.

KeyCode qtKeyToKeyCode(int qtKey);
Modifier qtModifiersToModifier(Qt::KeyboardModifiers mods);
ButtonCode qtButtonToButtonCode(Qt::MouseButton button);
