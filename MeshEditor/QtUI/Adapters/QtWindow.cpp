#include "QtWindow.h"
#include "Widgets/ViewportWidget.h"

#include <QFileDialog>
#include <QString>

QtWindow::QtWindow(ViewportWidget *widget) : m_widget(widget) {}

unsigned int QtWindow::getWidth() const
{
    return m_widget ? static_cast<unsigned int>(m_widget->width()) : 0;
}

unsigned int QtWindow::getHeight() const
{
    return m_widget ? static_cast<unsigned int>(m_widget->height()) : 0;
}

void QtWindow::setCursorCaptured(bool captured)
{
    if (m_widget)
    {
        m_widget->setCursorCaptured(captured);
    }
}

void QtWindow::setKeyCallback(const KeyCallback &callback)
{
    m_keyCallback = callback;
}

void QtWindow::setCursorPosCallback(const CursorPosCallback &callback)
{
    m_cursorPosCallback = callback;
}

void QtWindow::setMouseCallback(const MouseCallback &callback)
{
    m_mouseCallback = callback;
}

void QtWindow::setScrollCallback(const ScrollCallback &callback)
{
    m_scrollCallback = callback;
}

static const char *sceneFileFilter()
{
    return "Scene files (*.stl *.dae *.gltf *.glb);;STL files (*.stl);;Collada "
           "files "
           "(*.dae);;glTF files (*.gltf *.glb);;All files (*.*)";
}

bool QtWindow::openFileDialog(std::string &path)
{
    QWidget *parent = m_widget ? m_widget->window() : nullptr;
    const QString file = QFileDialog::getOpenFileName(
        parent, "Open Scene File", QString(),
        sceneFileFilter(), nullptr, QFileDialog::DontUseNativeDialog);
    if (file.isEmpty())
        return false;
    path = file.toStdString();
    return true;
}

bool QtWindow::saveFileDialog(std::string &path)
{
    QWidget *parent = m_widget ? m_widget->window() : nullptr;
    const QString file = QFileDialog::getSaveFileName(
        parent, "Save Scene File", QString(),
        sceneFileFilter(), nullptr, QFileDialog::DontUseNativeDialog);
    if (file.isEmpty())
        return false;
    path = file.toStdString();
    return true;
}

void QtWindow::injectKey(KeyCode key, Action action, Modifier mods)
{
    if (m_keyCallback)
        m_keyCallback(key, action, mods);
}

void QtWindow::injectCursorPos(double x, double y)
{
    if (m_cursorPosCallback)
        m_cursorPosCallback(x, y);
}

void QtWindow::injectMouse(ButtonCode button, Action action, Modifier mods,
                           double x, double y)
{
    if (m_mouseCallback)
        m_mouseCallback(button, action, mods, x, y);
}

void QtWindow::injectScroll(double xoffset, double yoffset)
{
    if (m_scrollCallback)
        m_scrollCallback(xoffset, yoffset);
}
