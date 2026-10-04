#include "WindowsGamepad.h"

#define DIRECTINPUT_VERSION 0x0800
#include <GameInput.h>
#include <dinput.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

using Microsoft::WRL::ComPtr;
using namespace GameInput::v3;

namespace Tank::Platform::Windows
{
    namespace
    {
        std::string CopyGameInputString(const char* value)
        {
            return value != nullptr ? value : "";
        }

        std::string WideToUtf8(const wchar_t* value)
        {
            if (value == nullptr || value[0] == L'\0')
            {
                return {};
            }

            const int byteCount = WideCharToMultiByte(
                CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
            if (byteCount <= 1)
            {
                return {};
            }

            std::string result(static_cast<size_t>(byteCount), '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, value, -1, result.data(), byteCount, nullptr, nullptr);
            result.pop_back();
            return result;
        }

        float NormalizeDirectInputAxis(LONG value)
        {
            return std::clamp(static_cast<float>(value) / 65535.0f, 0.0f, 1.0f);
        }

        float NormalizeMappedAxis(float value)
        {
            return std::clamp(value * 0.5f + 0.5f, 0.0f, 1.0f);
        }

        float NormalizeMappedVerticalAxis(float value)
        {
            return std::clamp(0.5f - value * 0.5f, 0.0f, 1.0f);
        }

        GameInputSwitchPosition ConvertPov(DWORD pov)
        {
            if ((LOWORD(pov) == 0xFFFF))
            {
                return GameInputSwitchCenter;
            }

            const DWORD direction = ((pov + 2250) / 4500) % 8;
            static constexpr std::array<GameInputSwitchPosition, 8> Directions = {
                GameInputSwitchUp,
                GameInputSwitchUpRight,
                GameInputSwitchRight,
                GameInputSwitchDownRight,
                GameInputSwitchDown,
                GameInputSwitchDownLeft,
                GameInputSwitchLeft,
                GameInputSwitchUpLeft,
            };
            return Directions[direction];
        }

        void ApplyDpad(Input::GamepadState& state, GameInputSwitchPosition dpad)
        {
            state.dpadUp = dpad == GameInputSwitchUp ||
                dpad == GameInputSwitchUpRight ||
                dpad == GameInputSwitchUpLeft;
            state.dpadDown = dpad == GameInputSwitchDown ||
                dpad == GameInputSwitchDownRight ||
                dpad == GameInputSwitchDownLeft;
            state.dpadLeft = dpad == GameInputSwitchLeft ||
                dpad == GameInputSwitchUpLeft ||
                dpad == GameInputSwitchDownLeft;
            state.dpadRight = dpad == GameInputSwitchRight ||
                dpad == GameInputSwitchUpRight ||
                dpad == GameInputSwitchDownRight;
        }
    }

    struct WindowsGamepad::Impl
    {
        static BOOL CALLBACK EnumerateDevice(
            const DIDEVICEINSTANCEW* instance,
            void* context)
        {
            Impl& impl = *static_cast<Impl*>(context);
            ComPtr<IDirectInputDevice8W> candidate;
            if (FAILED(impl.directInput->CreateDevice(
                    instance->guidInstance,
                    candidate.ReleaseAndGetAddressOf(),
                    nullptr)))
            {
                return DIENUM_CONTINUE;
            }

            DIDEVCAPS caps = {};
            caps.dwSize = sizeof(caps);
            if (FAILED(candidate->GetCapabilities(&caps)) || caps.dwAxes < 4)
            {
                return DIENUM_CONTINUE;
            }

            impl.directInputDevice = candidate;
            impl.directInputCaps = caps;
            impl.directInputName = WideToUtf8(instance->tszProductName);
            impl.directInputVendorId = LOWORD(instance->guidProduct.Data1);
            impl.directInputProductId = HIWORD(instance->guidProduct.Data1);
            return DIENUM_STOP;
        }

        static BOOL CALLBACK ConfigureAxis(
            const DIDEVICEOBJECTINSTANCEW* object,
            void* context)
        {
            IDirectInputDevice8W* device = static_cast<IDirectInputDevice8W*>(context);
            DIPROPRANGE range = {};
            range.diph.dwSize = sizeof(range);
            range.diph.dwHeaderSize = sizeof(range.diph);
            range.diph.dwHow = DIPH_BYID;
            range.diph.dwObj = object->dwType;
            range.lMin = 0;
            range.lMax = 65535;
            device->SetProperty(DIPROP_RANGE, &range.diph);
            return DIENUM_CONTINUE;
        }

        bool InitializeDirectInput(HWND windowHandle)
        {
            if (windowHandle == nullptr || FAILED(DirectInput8Create(
                    GetModuleHandleW(nullptr),
                    DIRECTINPUT_VERSION,
                    IID_IDirectInput8W,
                    reinterpret_cast<void**>(directInput.ReleaseAndGetAddressOf()),
                    nullptr)))
            {
                return false;
            }

            directInput->EnumDevices(
                DI8DEVCLASS_GAMECTRL,
                &Impl::EnumerateDevice,
                this,
                DIEDFL_ATTACHEDONLY);
            if (!directInputDevice)
            {
                return false;
            }
            if (FAILED(directInputDevice->SetDataFormat(&c_dfDIJoystick2)) ||
                FAILED(directInputDevice->SetCooperativeLevel(
                    windowHandle,
                    DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
            {
                directInputDevice.Reset();
                return false;
            }

            directInputDevice->EnumObjects(
                &Impl::ConfigureAxis,
                directInputDevice.Get(),
                DIDFT_AXIS);
            directInputDevice->Acquire();
            return true;
        }

        bool PollDirectInput()
        {
            if (!directInputDevice)
            {
                return false;
            }

            HRESULT result = directInputDevice->Poll();
            if (FAILED(result))
            {
                result = directInputDevice->Acquire();
                while (result == DIERR_INPUTLOST)
                {
                    result = directInputDevice->Acquire();
                }
                if (FAILED(result))
                {
                    return false;
                }
                directInputDevice->Poll();
            }

            DIJOYSTATE2 joystick = {};
            if (FAILED(directInputDevice->GetDeviceState(sizeof(joystick), &joystick)))
            {
                return false;
            }

            state.connected = true;
            state.deviceName = directInputName;
            state.vendorId = directInputVendorId;
            state.productId = directInputProductId;
            state.axisCount = 4;
            state.rawAxes[0] = NormalizeDirectInputAxis(joystick.lX);
            state.rawAxes[1] = NormalizeDirectInputAxis(joystick.lY);
            state.rawAxes[2] = NormalizeDirectInputAxis(joystick.lZ);
            state.rawAxes[3] = NormalizeDirectInputAxis(joystick.lRz);

            state.buttonCount = (std::min)(
                static_cast<std::uint32_t>(directInputCaps.dwButtons),
                static_cast<std::uint32_t>(state.rawButtons.size()));
            for (std::uint32_t index = 0; index < state.buttonCount; ++index)
            {
                state.rawButtons[index] = (joystick.rgbButtons[index] & 0x80) != 0;
            }

            if (directInputCaps.dwPOVs > 0)
            {
                const GameInputSwitchPosition dpad = ConvertPov(joystick.rgdwPOV[0]);
                state.switchCount = 1;
                state.rawSwitches[0] = static_cast<std::uint32_t>(dpad);
                ApplyDpad(state, dpad);
            }

            state.leftStickX = state.rawAxes[0];
            state.leftStickY = -state.rawAxes[1];
            return true;
        }

        bool PollGameInput()
        {
            if (!gameInput)
            {
                return false;
            }

            ComPtr<IGameInputReading> reading;
            if (FAILED(gameInput->GetCurrentReading(
                    GameInputKindGamepad,
                    nullptr,
                    reading.ReleaseAndGetAddressOf())) ||
                !reading)
            {
                return false;
            }

            GameInputGamepadState gamepad = {};
            if (!reading->GetGamepadState(&gamepad))
            {
                return false;
            }

            Input::GamepadState mappedState;
            mappedState.connected = true;
            mappedState.hasGamepadMapping = true;

            IGameInputDevice* rawDevice = nullptr;
            reading->GetDevice(&rawDevice);
            ComPtr<IGameInputDevice> device;
            device.Attach(rawDevice);
            const GameInputDeviceInfo* deviceInfo = nullptr;
            if (device && SUCCEEDED(device->GetDeviceInfo(&deviceInfo)) && deviceInfo != nullptr)
            {
                mappedState.deviceName = CopyGameInputString(deviceInfo->displayName);
                mappedState.vendorId = deviceInfo->vendorId;
                mappedState.productId = deviceInfo->productId;
            }

            mappedState.leftStickX = gamepad.leftThumbstickX;
            mappedState.leftStickY = gamepad.leftThumbstickY;
            mappedState.rightTrigger = gamepad.rightTrigger;
            mappedState.dpadUp = (gamepad.buttons & GameInputGamepadDPadUp) != 0;
            mappedState.dpadDown = (gamepad.buttons & GameInputGamepadDPadDown) != 0;
            mappedState.dpadLeft = (gamepad.buttons & GameInputGamepadDPadLeft) != 0;
            mappedState.dpadRight = (gamepad.buttons & GameInputGamepadDPadRight) != 0;
            mappedState.brakePressed = (gamepad.buttons & GameInputGamepadA) != 0;

            // Preserve the existing two-lever profile contract when GameInput
            // supplies a standardized controller mapping. DirectInput exposes
            // these axes in the order left X/Y, right X/Y with a [0, 1] range.
            mappedState.axisCount = 4;
            mappedState.rawAxes[0] = NormalizeMappedAxis(gamepad.leftThumbstickX);
            mappedState.rawAxes[1] = NormalizeMappedVerticalAxis(gamepad.leftThumbstickY);
            mappedState.rawAxes[2] = NormalizeMappedAxis(gamepad.rightThumbstickX);
            mappedState.rawAxes[3] = NormalizeMappedVerticalAxis(gamepad.rightThumbstickY);

            mappedState.buttonCount = 12;
            mappedState.rawButtons[0] = (gamepad.buttons & GameInputGamepadX) != 0;
            mappedState.rawButtons[1] = (gamepad.buttons & GameInputGamepadA) != 0;
            mappedState.rawButtons[2] = (gamepad.buttons & GameInputGamepadB) != 0;
            mappedState.rawButtons[3] = (gamepad.buttons & GameInputGamepadY) != 0;
            mappedState.rawButtons[4] =
                (gamepad.buttons & GameInputGamepadLeftShoulder) != 0;
            mappedState.rawButtons[5] =
                (gamepad.buttons & GameInputGamepadRightShoulder) != 0;
            mappedState.rawButtons[6] =
                (gamepad.buttons & GameInputGamepadLeftTriggerButton) != 0;
            mappedState.rawButtons[7] =
                (gamepad.buttons & GameInputGamepadRightTriggerButton) != 0;
            mappedState.rawButtons[8] = (gamepad.buttons & GameInputGamepadView) != 0;
            mappedState.rawButtons[9] = (gamepad.buttons & GameInputGamepadMenu) != 0;
            mappedState.rawButtons[10] =
                (gamepad.buttons & GameInputGamepadLeftThumbstick) != 0;
            mappedState.rawButtons[11] =
                (gamepad.buttons & GameInputGamepadRightThumbstick) != 0;

            GameInputSwitchPosition dpad = GameInputSwitchCenter;
            if (mappedState.dpadUp && mappedState.dpadRight) dpad = GameInputSwitchUpRight;
            else if (mappedState.dpadDown && mappedState.dpadRight) dpad = GameInputSwitchDownRight;
            else if (mappedState.dpadDown && mappedState.dpadLeft) dpad = GameInputSwitchDownLeft;
            else if (mappedState.dpadUp && mappedState.dpadLeft) dpad = GameInputSwitchUpLeft;
            else if (mappedState.dpadUp) dpad = GameInputSwitchUp;
            else if (mappedState.dpadDown) dpad = GameInputSwitchDown;
            else if (mappedState.dpadLeft) dpad = GameInputSwitchLeft;
            else if (mappedState.dpadRight) dpad = GameInputSwitchRight;
            mappedState.switchCount = 1;
            mappedState.rawSwitches[0] = static_cast<std::uint32_t>(dpad);
            state = std::move(mappedState);
            return true;
        }

        ComPtr<IGameInput> gameInput;
        ComPtr<IDirectInput8W> directInput;
        ComPtr<IDirectInputDevice8W> directInputDevice;
        Input::GamepadState state;
        DIDEVCAPS directInputCaps = {};
        std::string directInputName;
        std::uint16_t directInputVendorId = 0;
        std::uint16_t directInputProductId = 0;
        bool available = false;
    };

    WindowsGamepad::WindowsGamepad()
        : m_impl(std::make_unique<Impl>())
    {
    }

    WindowsGamepad::~WindowsGamepad() = default;

    bool WindowsGamepad::Initialize(HWND windowHandle)
    {
        const HRESULT result = GameInputInitialize(
            __uuidof(IGameInput),
            reinterpret_cast<void**>(m_impl->gameInput.ReleaseAndGetAddressOf()));
        const bool gameInputAvailable = SUCCEEDED(result) && m_impl->gameInput != nullptr;
        const bool directInputAvailable = m_impl->InitializeDirectInput(windowHandle);
        m_impl->available = gameInputAvailable || directInputAvailable;
        return m_impl->available;
    }

    void WindowsGamepad::Poll()
    {
        m_impl->state = {};
        if (m_impl->PollGameInput())
        {
            return;
        }
        if (m_impl->PollDirectInput())
        {
            return;
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
