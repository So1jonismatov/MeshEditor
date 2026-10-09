#pragma once
#include <string>
#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef OGL_RENDER_SYSTEM_EXPORT
#define OGL_RENDER_SYSTEM_API __declspec(dllexport)
#else
#define OGL_RENDER_SYSTEM_API __declspec(dllimport)
#endif
#else
#define OGL_RENDER_SYSTEM_API __attribute__((visibility("default")))
#endif

class IRenderSystem;
class IWindow;

extern "C"
{
    OGL_RENDER_SYSTEM_API IRenderSystem *createRenderSystem();
    OGL_RENDER_SYSTEM_API IWindow *createWindow(const std::string &title,
                                                uint32_t width,
                                                uint32_t height);
    // Frees a window returned by createWindow. Must be used instead of a
    // plain `delete` in the executable: the window was allocated on the
    // DLL's heap and has to be released there too.
    OGL_RENDER_SYSTEM_API void destroyWindow(IWindow *window);
    OGL_RENDER_SYSTEM_API void waitEvents();
    OGL_RENDER_SYSTEM_API void swapDisplayBuffers(IWindow *window);
    OGL_RENDER_SYSTEM_API bool windowShouldClose(IWindow *window);
}