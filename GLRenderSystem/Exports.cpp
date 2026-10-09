#include "Exports.h"
#include "GLRenderSystem.h"
#include "GLWindow.h"

#ifdef _WIN32
#include <glfw3.h>
#endif

extern "C" OGL_RENDER_SYSTEM_API IRenderSystem *createRenderSystem()
{
    return new GLRenderSystem();
}

extern "C" OGL_RENDER_SYSTEM_API IWindow *createWindow(const std::string &title,
                                                       uint32_t width,
                                                       uint32_t height)
{
    GLWindow *window = new GLWindow(title, width, height);
    if (!window->getGLFWHandle())
    {
        delete window;
        return nullptr;
    }
    return window;
}

extern "C" OGL_RENDER_SYSTEM_API void destroyWindow(IWindow *window)
{
    delete window;
}

extern "C" OGL_RENDER_SYSTEM_API void waitEvents()
{
#ifdef _WIN32
    glfwPollEvents();
#endif
}

extern "C" OGL_RENDER_SYSTEM_API void swapDisplayBuffers(IWindow *window)
{
#ifdef _WIN32
    if (!window)
        return;
    GLWindow *glWin = static_cast<GLWindow *>(window);
    if (!glWin->getGLFWHandle())
        return;
    glfwSwapBuffers(glWin->getGLFWHandle());
#endif
}

extern "C" OGL_RENDER_SYSTEM_API bool windowShouldClose(IWindow *window)
{
#ifdef _WIN32
    if (!window)
        return true;
    GLWindow *glWin = static_cast<GLWindow *>(window);
    if (!glWin->getGLFWHandle())
        return true;
    return glfwWindowShouldClose(glWin->getGLFWHandle());
#else
    return true;
#endif
}