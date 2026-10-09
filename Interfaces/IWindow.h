#pragma once
#include <functional>
#include <cstdint>
#include <string>
#include "./Keys.h"

class IWindow
{
public:
    using KeyCallback = std::function<void(KeyCode, Action, Modifier)>;
    using CursorPosCallback = std::function<void(double, double)>;
    using MouseCallback =
        std::function<void(ButtonCode, Action, Modifier, double, double)>;
    using ScrollCallback = std::function<void(double, double)>;

    virtual ~IWindow();

    virtual unsigned int getWidth() const = 0;
    virtual unsigned int getHeight() const = 0;

    virtual void setCursorCaptured(bool captured) = 0;

    virtual void setKeyCallback(const KeyCallback &callback) = 0;
    virtual void setCursorPosCallback(const CursorPosCallback &callback) = 0;
    virtual void setMouseCallback(const MouseCallback &callback) = 0;
    virtual void setScrollCallback(const ScrollCallback &callback) = 0;

    virtual bool openFileDialog(std::string &path) = 0;
    virtual bool saveFileDialog(std::string &path) = 0;
};