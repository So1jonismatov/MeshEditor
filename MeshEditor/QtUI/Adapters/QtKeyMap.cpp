#include "QtKeyMap.h"

KeyCode qtKeyToKeyCode(int qtKey)
{
    // Printable ASCII range: Qt uses the same uppercase-latin1 values as the
    // GLFW-style KeyCode enum (letters, digits, punctuation).
    if (qtKey >= 0x20 && qtKey <= 0x60)
        return static_cast<KeyCode>(qtKey);

    switch (qtKey)
    {
    case Qt::Key_Escape: return KeyCode::KeyEscape;
    case Qt::Key_Return:
    case Qt::Key_Enter: return KeyCode::KeyEnter;
    case Qt::Key_Tab: return KeyCode::KeyTab;
    case Qt::Key_Backspace: return KeyCode::KeyBackspace;
    case Qt::Key_Insert: return KeyCode::KeyInsert;
    case Qt::Key_Delete: return KeyCode::KeyDelete;
    case Qt::Key_Right: return KeyCode::KeyRight;
    case Qt::Key_Left: return KeyCode::KeyLeft;
    case Qt::Key_Down: return KeyCode::KeyDown;
    case Qt::Key_Up: return KeyCode::KeyUp;
    case Qt::Key_PageUp: return KeyCode::KeyPageUp;
    case Qt::Key_PageDown: return KeyCode::KeyPageDown;
    case Qt::Key_Home: return KeyCode::KeyHome;
    case Qt::Key_End: return KeyCode::KeyEnd;
    case Qt::Key_CapsLock: return KeyCode::KeyCapsLock;
    case Qt::Key_ScrollLock: return KeyCode::KeyScrollLock;
    case Qt::Key_NumLock: return KeyCode::KeyNumLock;
    case Qt::Key_Print: return KeyCode::KeyPrintScreen;
    case Qt::Key_Pause: return KeyCode::KeyPause;
    case Qt::Key_F1: return KeyCode::KeyF1;
    case Qt::Key_F2: return KeyCode::KeyF2;
    case Qt::Key_F3: return KeyCode::KeyF3;
    case Qt::Key_F4: return KeyCode::KeyF4;
    case Qt::Key_F5: return KeyCode::KeyF5;
    case Qt::Key_F6: return KeyCode::KeyF6;
    case Qt::Key_F7: return KeyCode::KeyF7;
    case Qt::Key_F8: return KeyCode::KeyF8;
    case Qt::Key_F9: return KeyCode::KeyF9;
    case Qt::Key_F10: return KeyCode::KeyF10;
    case Qt::Key_F11: return KeyCode::KeyF11;
    case Qt::Key_F12: return KeyCode::KeyF12;
    case Qt::Key_Shift: return KeyCode::KeyLeftShift;
    case Qt::Key_Control: return KeyCode::KeyLeftControl;
    case Qt::Key_Alt: return KeyCode::KeyLeftAlt;
    case Qt::Key_Meta: return KeyCode::KeyLeftSuper;
    case Qt::Key_Menu: return KeyCode::KeyMenu;
    default: return KeyCode::KeyUnknown;
    }
}

Modifier qtModifiersToModifier(Qt::KeyboardModifiers mods)
{
    int result = 0;
    if (mods & Qt::ShiftModifier)
        result |= static_cast<int>(Modifier::Shift);
    if (mods & Qt::ControlModifier)
        result |= static_cast<int>(Modifier::Control);
    if (mods & Qt::AltModifier)
        result |= static_cast<int>(Modifier::Alt);
    if (mods & Qt::MetaModifier)
        result |= static_cast<int>(Modifier::Super);
    return static_cast<Modifier>(result);
}

ButtonCode qtButtonToButtonCode(Qt::MouseButton button)
{
    switch (button)
    {
    case Qt::LeftButton: return ButtonCode::MouseButtonLeft;
    case Qt::RightButton: return ButtonCode::MouseButtonRight;
    case Qt::MiddleButton: return ButtonCode::MouseButtonMiddle;
    case Qt::BackButton: return ButtonCode::MouseButton4;
    case Qt::ForwardButton: return ButtonCode::MouseButton5;
    default: return ButtonCode::MouseButtonLast;
    }
}
