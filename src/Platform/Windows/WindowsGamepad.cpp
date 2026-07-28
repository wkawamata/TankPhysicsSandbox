#include "WindowsGamepad.h"

#include <GameInput.h>
#include <wrl/client.h>

#include <algorithm>
#include <string>

using Microsoft::WRL::ComPtr;

extern "C" HRESULT WINAPI GameInputInitialize(
    REFIID interfaceId,
    void** gameInput);

namespace Tank::Platform::Windows
{
    namespace
    {
        std::string CopyGameInputString(const GameInputString* value)
        {
            if (value == nullptr || value->data == nullptr || value->sizeInBytes == 0)
            {
                return {};
            }

            std::string result(value->data, value->sizeInBytes);
            while (!result.empty() && result.back() == '\0')
            {
                result.pop_back();
            }
            return result;
        }
    }

    struct WindowsGamepad::Impl
    {
        ComPtr<IGameInput> gameInput;
        ComPtr<IGameInputDevice> device;
        Input::GamepadState state;
        bool available = false;
    };

    WindowsGamepad::WindowsGamepad()
        : m_impl(std::make_unique<Impl>())
    {
    }

    WindowsGamepad::~WindowsGamepad() = default;

    bool WindowsGamepad::Initialize()
    {
        m_impl->available = SUCCEEDED(GameInputInitialize(
            __uuidof(IGameInput),
            reinterpret_cast<void**>(m_impl->gameInput.ReleaseAndGetAddressOf())));
        return m_impl->available;
    }

    void WindowsGamepad::Poll()
    {
        m_impl->state = {};
        if (!m_impl->available)
        {
            return;
        }

        constexpr GameInputKind inputKinds =
            static_cast<GameInputKind>(GameInputKindGamepad | GameInputKindController);

        ComPtr<IGameInputReading> reading;
        HRESULT result =
            m_impl->gameInput->GetCurrentReading(inputKinds, m_impl->device.Get(), &reading);
        if (FAILED(result) && m_impl->device)
        {
            m_impl->device.Reset();
            result = m_impl->gameInput->GetCurrentReading(inputKinds, nullptr, &reading);
        }
        if (FAILED(result))
        {
            return;
        }

        if (!m_impl->device)
        {
            IGameInputDevice* device = nullptr;
            reading->GetDevice(&device);
            m_impl->device.Attach(device);
        }

        Input::GamepadState& state = m_impl->state;
        state.connected = true;
        state.buttonCount = reading->GetControllerButtonCount();
        state.axisCount = reading->GetControllerAxisCount();
        state.switchCount = reading->GetControllerSwitchCount();
        const std::uint32_t rawAxisCount =
            (std::min)(state.axisCount, static_cast<std::uint32_t>(state.rawAxes.size()));
        const std::uint32_t rawButtonCount =
            (std::min)(state.buttonCount, static_cast<std::uint32_t>(state.rawButtons.size()));
        const std::uint32_t rawSwitchCount =
            (std::min)(state.switchCount, static_cast<std::uint32_t>(state.rawSwitches.size()));
        reading->GetControllerAxisState(rawAxisCount, state.rawAxes.data());
        reading->GetControllerButtonState(rawButtonCount, state.rawButtons.data());
        std::array<GameInputSwitchPosition, Input::GamepadState::MaxRawSwitches> rawSwitches = {};
        reading->GetControllerSwitchState(rawSwitchCount, rawSwitches.data());
        for (std::uint32_t index = 0; index < rawSwitchCount; ++index)
        {
            state.rawSwitches[index] = static_cast<std::uint32_t>(rawSwitches[index]);
        }

        if (m_impl->device)
        {
            const GameInputDeviceInfo* deviceInfo = m_impl->device->GetDeviceInfo();
            state.deviceName = CopyGameInputString(deviceInfo->displayName);
            state.vendorId = deviceInfo->vendorId;
            state.productId = deviceInfo->productId;
        }

        GameInputGamepadState gamepadState = {};
        state.hasGamepadMapping = reading->GetGamepadState(&gamepadState);
        if (state.hasGamepadMapping)
        {
            state.leftStickX = gamepadState.leftThumbstickX;
            state.leftStickY = gamepadState.leftThumbstickY;
            state.brakePressed = (gamepadState.buttons & GameInputGamepadA) != 0;
        }
        else
        {
            if (rawAxisCount >= 2)
            {
                state.leftStickX = state.rawAxes[0];
                state.leftStickY = -state.rawAxes[1];
            }
            constexpr std::uint32_t brakeButtonIndex = 3;
            if (rawButtonCount > brakeButtonIndex)
            {
                state.brakePressed = state.rawButtons[brakeButtonIndex];
            }
        }
    }

    bool WindowsGamepad::IsAvailable() const
    {
        return m_impl->available;
    }

    const Input::GamepadState& WindowsGamepad::State() const
    {
        return m_impl->state;
    }
}
