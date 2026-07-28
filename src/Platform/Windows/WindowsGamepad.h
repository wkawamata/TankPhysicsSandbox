#pragma once

#include "Input/GamepadState.h"

#include <memory>

namespace Tank::Platform::Windows
{
    class WindowsGamepad
    {
    public:
        WindowsGamepad();
        ~WindowsGamepad();

        WindowsGamepad(const WindowsGamepad&) = delete;
        WindowsGamepad& operator=(const WindowsGamepad&) = delete;

        bool Initialize();
        void Poll();

        bool IsAvailable() const;
        const Input::GamepadState& State() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
