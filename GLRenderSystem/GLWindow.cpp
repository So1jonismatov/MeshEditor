#include "GLWindow.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>

GLFWwindow *GLWindow::getGLFWHandle() const
{
    return m_Handle;
}

std::string GLWindow::wide_to_utf8(const std::wstring &value)
{
    if (value.empty())
        return {};

    const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1,
                                                 nullptr, 0, nullptr, nullptr);
    if (requiredSize <= 0)
        return {};

    std::string result(static_cast<size_t>(requiredSize), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(),
                        requiredSize, nullptr, nullptr);
    result.resize(static_cast<size_t>(requiredSize - 1));
    return result;
}

bool GLWindow::show_file_dialog(bool saveDialog,
                                const std::wstring &defaultName,
                                std::string &path)
{
    wchar_t fileBuffer[32768] = {};
    if (!defaultName.empty())
    {
        wcscpy_s(
            fileBuffer,
            static_cast<size_t>(sizeof(fileBuffer) / sizeof(fileBuffer[0])),
            defaultName.c_str());
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = fileBuffer;
    dialog.nMaxFile =
        static_cast<DWORD>(sizeof(fileBuffer) / sizeof(fileBuffer[0]));
    dialog.lpstrFilter =
        L"Scene files\0*.stl;*.dae;*.gltf;*.glb\0STL files\0*.stl\0Collada "
        L"files\0*.dae\0glTF files\0*.gltf;*.glb\0All files\0*.*\0\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrTitle = saveDialog ? L"Save Scene File" : L"Open Scene File";
    dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                   (saveDialog ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

    const BOOL result =
        saveDialog ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (!result)
        return false;

    path = wide_to_utf8(fileBuffer);
    return !path.empty();
}
#endif

#ifdef _WIN32
GLWindow::GLWindow(const std::string &title, unsigned int width,
                   unsigned int height)
    : m_Handle(nullptr), m_Width(width), m_Height(height)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_REFRESH_RATE, 60);

    m_Handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_Handle)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return;
    }

    glfwMakeContextCurrent(m_Handle);
    glfwSwapInterval(1);

    static bool initGLAD = false;
    if (!initGLAD)
    {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
        }
        initGLAD = true;
    }

    glfwSetWindowUserPointer(m_Handle, this);

    glfwSetKeyCallback(m_Handle, key_callback_static);
    glfwSetCursorPosCallback(m_Handle, cursor_pos_callback_static);
    glfwSetMouseButtonCallback(m_Handle, mouse_button_callback_static);
    glfwSetScrollCallback(m_Handle, scroll_callback_static);

    glfwShowWindow(m_Handle);
}

GLWindow::~GLWindow()
{
    if (m_Handle)
    {
        glfwDestroyWindow(m_Handle);
    }
    glfwTerminate();
}

unsigned int GLWindow::getWidth() const
{
    return m_Width;
}

unsigned int GLWindow::getHeight() const
{
    return m_Height;
}

void GLWindow::setCursorCaptured(bool captured)
{
    glfwSetInputMode(m_Handle, GLFW_CURSOR,
                     captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void GLWindow::setKeyCallback(const KeyCallback &callback)
{
    m_KeyCallback = callback;
}

void GLWindow::setCursorPosCallback(const CursorPosCallback &callback)
{
    m_CursorPosCallback = callback;
}

void GLWindow::setMouseCallback(const MouseCallback &callback)
{
    m_MouseCallback = callback;
}

void GLWindow::setScrollCallback(const ScrollCallback &callback)
{
    m_ScrollCallback = callback;
}

bool GLWindow::openFileDialog(std::string &path)
{
    return show_file_dialog(false, {}, path);
}

bool GLWindow::saveFileDialog(std::string &path)
{
    return show_file_dialog(true, L"scene.stl", path);
}

void GLWindow::key_callback_static(GLFWwindow *window, int key, int scancode,
                                   int action, int mods)
{
    auto *instance = (GLWindow *)(glfwGetWindowUserPointer(window));
    if (instance && instance->m_KeyCallback)
    {
        instance->m_KeyCallback((KeyCode)key, (Action)action, (Modifier)mods);
    }
}

void GLWindow::cursor_pos_callback_static(GLFWwindow *window, double xpos,
                                          double ypos)
{
    auto *instance = (GLWindow *)(glfwGetWindowUserPointer(window));
    if (instance && instance->m_CursorPosCallback)
    {
        instance->m_CursorPosCallback(xpos, ypos);
    }
}

void GLWindow::mouse_button_callback_static(GLFWwindow *window, int button,
                                            int action, int mods)
{
    auto *instance = (GLWindow *)(glfwGetWindowUserPointer(window));
    if (instance && instance->m_MouseCallback)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        instance->m_MouseCallback((ButtonCode)button, (Action)action,
                                  (Modifier)mods, x, y);
    }
}

void GLWindow::scroll_callback_static(GLFWwindow *window, double xoffset,
                                      double yoffset)
{
    auto *instance = (GLWindow *)(glfwGetWindowUserPointer(window));
    if (instance && instance->m_ScrollCallback)
    {
        instance->m_ScrollCallback(xoffset, yoffset);
    }
}
#else
GLWindow::GLWindow(const std::string &title, unsigned int width, unsigned int height)
    : m_Handle(nullptr), m_Width(width), m_Height(height) {}
GLWindow::~GLWindow() {}
unsigned int GLWindow::getWidth() const { return m_Width; }
unsigned int GLWindow::getHeight() const { return m_Height; }
void GLWindow::setCursorCaptured(bool) {}
void GLWindow::setKeyCallback(const KeyCallback &) {}
void GLWindow::setCursorPosCallback(const CursorPosCallback &) {}
void GLWindow::setMouseCallback(const MouseCallback &) {}
void GLWindow::setScrollCallback(const ScrollCallback &) {}
bool GLWindow::openFileDialog(std::string &) { return false; }
bool GLWindow::saveFileDialog(std::string &) { return false; }
GLFWwindow *GLWindow::getGLFWHandle() const { return nullptr; }
#endif