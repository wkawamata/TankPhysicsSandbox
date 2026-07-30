#pragma once

#include "CameraSettingsStore.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/CameraSettings.h"

#include <DirectXMath.h>
#include <array>
#include <optional>
#include <string>

namespace Engine
{
    struct CameraState;
    class Scene;
}

namespace Tank::App
{

    class CameraController
    {
    public:
        static constexpr int kSlotCount = 4;

        CameraController();

        // Slot management.
        int SelectedSlot() const { return m_selectedSlot; }
        void SetAutoLoad(bool enabled) { m_autoLoad = enabled; }
        bool AutoLoad() const { return m_autoLoad; }
        const std::string& Status() const { return m_status; }

        bool EnsureSlotLoaded(int slot, CameraSettingsStore& store);
        void SelectSlot(int slot, bool load, CameraSettingsStore& store);
        void UpdateSlotCache(const Engine::CameraState& camera);

        // Camera capture/apply.
        Tank::Rendering::CameraSettings CaptureSettings(
            const Engine::CameraState& camera, bool followTank) const;
        void ApplySettings(
            const Tank::Rendering::CameraSettings& settings, bool smooth,
            Engine::CameraState& camera);

        // Transition.
        void UpdateTransition(float dt, Engine::CameraState& camera);
        bool IsTransitioning() const { return m_transitionActive; }

        // Follow camera.
        bool FollowEnabled() const { return m_followTank; }
        void SetFollowEnabled(bool enabled) { m_followTank = enabled; }
        float FollowDistance() const { return m_followDistance; }
        void SetFollowDistance(float d) { m_followDistance = d; }
        float LookDownDegrees() const { return m_lookDownDegrees; }
        void SetLookDownDegrees(float d) { m_lookDownDegrees = d; }
        float PositionSpeed() const { return m_positionSpeed; }
        void SetPositionSpeed(float s) { m_positionSpeed = s; }
        float RotationSpeed() const { return m_rotationSpeed; }
        void SetRotationSpeed(float s) { m_rotationSpeed = s; }
        float Damping() const { return m_damping; }
        void SetDamping(float d) { m_damping = d; }
        float YawSpeedLimitDegrees() const { return m_yawSpeedLimitDegrees; }
        void SetYawSpeedLimitDegrees(float d) { m_yawSpeedLimitDegrees = d; }
        float YawDamping() const { return m_yawDamping; }
        void SetYawDamping(float d) { m_yawDamping = d; }

        void ResetFollowState();
        void UpdateFollowCamera(
            const Tank::Physics::TrackedVehicleTestState& state,
            float dt, Engine::CameraState& camera);

        // Presets.
        void ApplyCameraPreset(
            const DirectX::XMFLOAT3& offset,
            const Tank::Physics::TrackedVehicleTestState& state,
            Engine::CameraState& camera);

        // Button cycle.
        void OnButton4Pressed(CameraSettingsStore& store);
        void OnButton7Pressed(CameraSettingsStore& store);
        void UpdateButtonStates(bool button4Pressed, bool button7Pressed,
            CameraSettingsStore& store);

    // File loaded tracking (for cache).
    bool IsSlotFileLoaded(int slot) const
    {
        return m_fileLoaded[static_cast<size_t>(slot)];
    }
    bool IsSlotDirty(int slot) const
    {
        return m_dirty[static_cast<size_t>(slot)];
    }

    // Access cached settings for current slot (after EnsureSlotLoaded).
    const Tank::Rendering::CameraSettings* GetCachedSettings() const
    {
        const size_t slotIndex = static_cast<size_t>(m_selectedSlot);
        if (m_cache[slotIndex].has_value())
        {
            return &*m_cache[slotIndex];
        }
        return nullptr;
    }

    private:
        int m_selectedSlot = 0;
        bool m_autoLoad = true;
        std::string m_status;

        std::array<std::optional<Tank::Rendering::CameraSettings>, kSlotCount> m_cache;
        std::array<bool, kSlotCount> m_dirty;
        std::array<bool, kSlotCount> m_fileLoaded;

        bool m_followTank = false;
        float m_followDistance = 16.0f;
        float m_lookDownDegrees = 25.0f;
        float m_positionSpeed = 5.0f;
        float m_rotationSpeed = 8.0f;
        float m_damping = 1.0f;
        float m_yawSpeedLimitDegrees = 180.0f;
        float m_yawDamping = 8.0f;

        DirectX::XMFLOAT3 m_followVelocity = {};
        float m_fovTarget = 35.0f;
        float m_fovVelocity = 0.0f;
        bool m_fovSpringActive = false;
        float m_orbitYaw = 0.0f;
        float m_yawVelocity = 0.0f;
        bool m_orbitInitialized = false;

        bool m_transitionActive = false;
        float m_transitionTime = 0.0f;
        float m_transitionDuration = 0.75f;
        Tank::Rendering::CameraSettings m_transitionStart = {};
        Tank::Rendering::CameraSettings m_transitionTarget = {};

        bool m_button4WasPressed = false;
        bool m_button7WasPressed = false;
    };

}
