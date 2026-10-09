#include "Application.h"
#include "Widgets/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QSurfaceFormat>

#include <iostream>


#ifdef _WIN32
#include <windows.h>
#include <ole2.h>
extern "C"
{
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    // Ensure COM/OLE is initialized in Single-Threaded Apartment (STA) mode
    OleInitialize(nullptr);
#endif

    QSurfaceFormat format;
    format.setVersion(4, 6);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication qtApp(argc, argv);

    QDir::setCurrent(QCoreApplication::applicationDirPath());

    std::string filename;
    if (argc > 1)
    {
        filename = argv[1];
    }

    Application app(filename);
    if (!app.getRenderSystem())
    {
        std::cerr << "Failed to load GLRenderSystem.dll" << std::endl;
#ifdef _WIN32
        OleUninitialize();
#endif
        return 1;
    }

    MainWindow window(app);
    window.showMaximized();

    const int result = qtApp.exec();

#ifdef _WIN32
    OleUninitialize();
#endif

    return result;
}
