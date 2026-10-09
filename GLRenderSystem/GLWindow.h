#pragma once
#include <string>
#include <functional>
#include "../Interfaces/IWindow.h"

#ifdef _WIN32
#include <glad/glad.h>
#include <glfw3.h>
#else
struct GLFWwindow;
#endif

class GLWindow : public IWindow
{
public:
    GLWindow(const std::string &title, unsigned int width, unsigned int height);
    ~GLWindow() override;

    unsigned int getWidth() const override;
    unsigned int getHeight() const override;

    void setCursorCaptured(bool captured) override;

    void setKeyCallback(const KeyCallback &callback) override;
    void setCursorPosCallback(const CursorPosCallback &callback) override;
    void setMouseCallback(const MouseCallback &callback) override;
    void setScrollCallback(const ScrollCallback &callback) override;
    bool openFileDialog(std::string &path) override;
    bool saveFileDialog(std::string &path) override;

    GLFWwindow *getGLFWHandle() const;

private:
    GLFWwindow *m_Handle;
    unsigned int m_Width, m_Height;

    KeyCallback m_KeyCallback;
    CursorPosCallback m_CursorPosCallback;
    MouseCallback m_MouseCallback;
    ScrollCallback m_ScrollCallback;

    static void key_callback_static(GLFWwindow *window, int key, int scancode,
                                    int action, int mods);
    static void cursor_pos_callback_static(GLFWwindow *window, double xpos,
                                           double ypos);
    static void mouse_button_callback_static(GLFWwindow *window, int button,
                                             int action, int mods);
    static void scroll_callback_static(GLFWwindow *window, double xoffset,
                                       double yoffset);
    static std::string wide_to_utf8(const std::wstring &value);
    static bool show_file_dialog(bool saveDialog,
                                 const std::wstring &defaultName,
                                 std::string &path);
};