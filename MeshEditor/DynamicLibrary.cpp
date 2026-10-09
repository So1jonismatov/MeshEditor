#include "DynamicLibrary.h"

#include <filesystem>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#if !defined(NOMINMAX) && defined(_MSC_VER)
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
std::string executableDirectory()
{
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return {};

    return std::filesystem::path(buffer).parent_path().string();
#else
    try
    {
        return std::filesystem::canonical("/proc/self/exe").parent_path().string();
    }
    catch (...)
    {
        return {};
    }
#endif
}
} // namespace

DynamicLibrary::DynamicLibrary(const std::string &name) : instance(nullptr)
{
#ifdef _WIN32
    instance = (void *)LoadLibraryA(name.c_str());
    if (!instance && name.find_first_of("\\/") == std::string::npos)
    {
        const std::string baseDir = executableDirectory();
        if (!baseDir.empty())
        {
            const std::filesystem::path fullPath =
                std::filesystem::path(baseDir) / name;
            instance = (void *)LoadLibraryA(fullPath.string().c_str());
        }
    }
#else
    instance = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!instance && name.find_first_of("/") == std::string::npos)
    {
        const std::string baseDir = executableDirectory();
        if (!baseDir.empty())
        {
            const std::filesystem::path fullPath =
                std::filesystem::path(baseDir) / name;
            instance = dlopen(fullPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        }
    }
    if (!instance)
    {
        const char *err = dlerror();
        if (err)
        {
            std::cerr << "[DynamicLibrary] dlopen error: " << err << std::endl;
        }
    }
#endif
}

DynamicLibrary::~DynamicLibrary()
{
#ifdef _WIN32
    if (instance)
        FreeLibrary((HMODULE)instance);
#else
    if (instance)
        dlclose(instance);
#endif
}

void *DynamicLibrary::getSymbol(const std::string &symbolName) const
{
#ifdef _WIN32
    if (!instance)
        return nullptr;
    return (void *)GetProcAddress((HMODULE)instance, symbolName.c_str());
#else
    if (!instance)
        return nullptr;
    return dlsym(instance, symbolName.c_str());
#endif
}
