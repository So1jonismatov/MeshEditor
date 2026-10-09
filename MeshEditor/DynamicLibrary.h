#pragma once

#include <string>

class DynamicLibrary
{
public:
    explicit DynamicLibrary(const std::string &name);
    ~DynamicLibrary();

    void *getSymbol(const std::string &symbolName) const;

    template <typename T> T getSymbol(const std::string &symbolName) const
    {
        return reinterpret_cast<T>(getSymbol(symbolName));
    }

private:
    void *instance = nullptr;
};