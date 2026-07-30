#include "CameraController.h"

#include <Engine/RtPbrSurveyEngine.h>
#include <Scene/Scene.h>

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>

#include <algorithm>
#include <cmath>

namespace Tank::App
{

    CameraController::CameraController()
    {
        m_dirty.fill(false);
        m_fileLoaded.fill(false);
        m_cache.fill(std::nullopt);
    }

    Tank::Rendering::CameraSettings CameraController::CaptureSettings(
        const Engine::CameraState& camera, bool followTank) const
    {
        Tank::Rendering::CameraSettings settings;
        settings.position[0] = camera.pos.x;
        settings.position[1] = camera.pos.y;
        settings.position[2] = camera.pos.z;
        settings.gazePoint[0] = camera.gazePoint.x;
        settings.gazePoint[1] = camera.gazePoint.y;
        settings.gazePoint[2] = camera.gazePoint.z;
        settings.projection = static_cast<int>(camera.projection);
        settings.fovDegrees = camera.fov;
        settings.orthographicHeight = camera.orthographicHeight;
        settings.followTank = followTank;
        settings.followDistance = m_followDistance;
        settings.lookDownDegrees = m_lookDownDegrees;
        settings.positionSpeed = m_positionSpeed;
        settings.rotationSpeed = m_rotationSpeed;
        settings.damping = m_damping;
        settings.yawSpeedLimitDegrees = m_yawSpeedLimitDegrees;
        settings.yawDamping = m_yawDamping;
        return settings;
    }

    void CameraController::ApplySettings(
        const Tank::Rendering::CameraSettings& settings, bool smooth,
        Engine::CameraState& camera)
    {
        if (smooth && settings.followTank)
        {
            const DirectX::XMFLOAT3 currentPosition = camera.pos;
            const DirectX::XMFLOAT3 currentGazePoint = camera.gazePoint;
            const float currentFov = camera.fov;
            ApplySettings(settings, false, camera);
            camera.pos = currentPosition;
            camera.gazePoint = currentGazePoint;
            camera.fov = currentFov;
            m_fovTarget = std::clamp(settings.fovDegrees, 20.0f, 120.0f);
            m_fovVelocity = 0.0f;
            m_fovSpringActive =
                std::abs(camera.fov - m_fovTarget) > 0.001f;
            m_transitionActive = false;
            return;
        }
        if (smooth)
        {
            m_transitionStart = CaptureSettings(camera, m_followTank);
            m_transitionTarget = settings;
            m_transitionTime = 0.0f;
            m_transitionActive = true;
            m_followTank = false;
            return;
        }

        camera.pos = {
            settings.position[0],
            settings.position[1],
            settings.position[2] };
        camera.gazePoint = {
            settings.gazePoint[0],
            settings.gazePoint[1],
            settings.gazePoint[2] };
        camera.projection =
            settings.projection == static_cast<int>(Engine::CameraProjection::Orthographic)
            ? Engine::CameraProjection::Orthographic
            : Engine::CameraProjection::Perspective;
        camera.fov = std::clamp(settings.fovDegrees, 20.0f, 120.0f);
        m_fovTarget = camera.fov;
        m_fovVelocity = 0.0f;
        m_fovSpringActive = false;
        camera.orthographicHeight =
            std::clamp(settings.orthographicHeight, 1.0f, 50.0f);
        m_followTank = settings.followTank;
        m_followDistance = std::clamp(settings.followDistance, 4.0f, 250.0f);
        m_lookDownDegrees = std::clamp(settings.lookDownDegrees, 0.0f, 89.0f);
        m_positionSpeed = std::clamp(settings.positionSpeed, 0.5f, 20.0f);
        m_rotationSpeed = std::clamp(settings.rotationSpeed, 0.5f, 20.0f);
        m_damping = std::clamp(settings.damping, 0.1f, 2.0f);
        m_yawSpeedLimitDegrees = std::clamp(settings.yawSpeedLimitDegrees, 15.0f, 720.0f);
        m_yawDamping = std::clamp(settings.yawDamping, 0.5f, 30.0f);
        ResetFollowState();
    }

    bool CameraController::EnsureSlotLoaded(int slot)
    {
        const size_t slotIndex = static_cast<size_t>(std::clamp(slot, 0, 3));
        if (m_fileLoaded[slotIndex] && m_cache[slotIndex].has_value())
        {
            return true;
        }
        Tank::Rendering::CameraSettings settings;
        CameraSettingsStore store(static_cast<int>(slotIndex));
        if (!store.Read(settings, m_status))
        {
            return false;
        }
        m_cache[slotIndex] = settings;
        m_dirty[slotIndex] = false;
        m_fileLoaded[slotIndex] = true;
        return true;
    }

    void CameraController::SelectSlot(int slot, bool load)
    {
        const size_t currentSlot = static_cast<size_t>(m_selectedSlot);
        if (!m_cache[currentSlot].has_value())
        {
            // Cache current camera state; caller must update cache separately.
            m_dirty[currentSlot] = true;
        }
        else
        {
            // Re-cache handled by UpdateSlotCache in App
        }
        m_selectedSlot = std::clamp(slot, 0, 3);
        if (load)
        {
            EnsureSlotLoaded(m_selectedSlot);
        }
    }

    void CameraController::SetSlotSettings(
        int slot,
        const Tank::Rendering::CameraSettings& settings)
    {
        const size_t slotIndex = static_cast<size_t>(std::clamp(slot, 0, 3));
        m_cache[slotIndex] = settings;
        m_dirty[slotIndex] = false;
        m_fileLoaded[slotIndex] = true;
    }

    void CameraController::UpdateSlotCache(const Engine::CameraState& camera)
    {
        if (m_transitionActive)
        {
            return;
        }
        const size_t slot = static_cast<size_t>(m_selectedSlot);
        if (!m_cache[slot].has_value())
        {
            return;
        }
        Tank::Rendering::CameraSettings current = CaptureSettings(camera, m_followTank);
        if (current.followTank && m_cache[slot] && m_cache[slot]->followTank)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                current.position[axis] = m_cache[slot]->position[axis];
                current.gazePoint[axis] = m_cache[slot]->gazePoint[axis];
            }
        }
        if (!m_dirty[slot] &&
            Tank::Rendering::SerializeCameraSettings(current) !=
                Tank::Rendering::SerializeCameraSettings(*m_cache[slot]))
        {
            m_dirty[slot] = true;
        }
        if (m_dirty[slot])
        {
            m_cache[slot] = current;
        }
    }

    void CameraController::UpdateTransition(float dt, Engine::CameraState& camera)
    {
        if (!m_transitionActive)
        {
            return;
        }
        m_transitionTime += std::max(dt, 0.0f);
        const float normalizedTime = std::clamp(
            m_transitionTime / m_transitionDuration, 0.0f, 1.0f);
        const float t = normalizedTime * normalizedTime * (3.0f - 2.0f * normalizedTime);
        Tank::Rendering::CameraSettings blended = m_transitionStart;
        for (int axis = 0; axis < 3; ++axis)
        {
            blended.position[axis] = std::lerp(
                m_transitionStart.position[axis],
                m_transitionTarget.position[axis], t);
            blended.gazePoint[axis] = std::lerp(
                m_transitionStart.gazePoint[axis],
                m_transitionTarget.gazePoint[axis], t);
        }
        blended.fovDegrees = std::lerp(
            m_transitionStart.fovDegrees, m_transitionTarget.fovDegrees, t);
        blended.orthographicHeight = std::lerp(
            m_transitionStart.orthographicHeight,
            m_transitionTarget.orthographicHeight, t);
        blended.followDistance = std::lerp(
            m_transitionStart.followDistance,
            m_transitionTarget.followDistance, t);
        blended.lookDownDegrees = std::lerp(
            m_transitionStart.lookDownDegrees,
            m_transitionTarget.lookDownDegrees, t);
        blended.followTank = false;
        blended.projection = normalizedTime < 0.5f
            ? m_transitionStart.projection
            : m_transitionTarget.projection;
        ApplySettings(blended, false, camera);
        if (normalizedTime >= 1.0f)
        {
            m_transitionActive = false;
            ApplySettings(m_transitionTarget, false, camera);
        }
    }

    void CameraController::ResetFollowState()
    {
        m_followVelocity = {};
        m_fovVelocity = 0.0f;
        m_fovSpringActive = false;
        m_yawVelocity = 0.0f;
        m_orbitInitialized = false;
    }

    void CameraController::UpdateFollowCamera(
        const Tank::Physics::TrackedVehicleTestState& state,
        float dt, Engine::CameraState& camera)
    {
        if (!m_followTank || dt <= 0.0f)
        {
            return;
        }

        using namespace DirectX;

        const XMVECTOR bodyRotation = XMQuaternionNormalize(XMVectorSet(
            state.bodyRotation.x,
            state.bodyRotation.y,
            state.bodyRotation.z,
            state.bodyRotation.w));
        const XMVECTOR forward = XMVector3Normalize(
            XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), bodyRotation));
        const XMVECTOR pivot = XMVectorSet(
            state.bodyPosition.x,
            state.bodyPosition.y + 0.5f,
            state.bodyPosition.z,
            1.0f);
        XMVECTOR position = XMLoadFloat3(&camera.pos);
        XMVECTOR velocity = XMLoadFloat3(&m_followVelocity);
        const float positionSpeed = std::clamp(m_positionSpeed, 0.5f, 20.0f);
        const float damping = std::clamp(m_damping, 0.1f, 2.0f);
        const float safeDt = std::min(dt, 1.0f / 30.0f);
        const float rotationAlpha =
            1.0f - std::exp(-std::clamp(m_rotationSpeed, 0.5f, 20.0f) * safeDt / damping);

        DirectX::XMFLOAT3 forwardVector = {};
        XMStoreFloat3(&forwardVector, forward);
        const float desiredRearYaw =
            std::atan2(-forwardVector.x, -forwardVector.z);
        if (!m_orbitInitialized)
        {
            XMFLOAT3 pivotPosition = {};
            XMStoreFloat3(&pivotPosition, pivot);
            m_orbitYaw = std::atan2(
                camera.pos.x - pivotPosition.x,
                camera.pos.z - pivotPosition.z);
            m_yawVelocity = 0.0f;
            m_orbitInitialized = true;
        }
        const float yawDelta = std::remainder(
            desiredRearYaw - m_orbitYaw, XM_2PI);
        const float yawSpeedLimit =
            XMConvertToRadians(std::clamp(m_yawSpeedLimitDegrees, 15.0f, 720.0f));
        const float desiredYawVelocity = std::clamp(
            yawDelta * std::clamp(m_rotationSpeed, 0.5f, 20.0f),
            -yawSpeedLimit, yawSpeedLimit);
        const float yawVelocityAlpha =
            1.0f - std::exp(-std::clamp(m_yawDamping, 0.5f, 30.0f) * safeDt);
        m_yawVelocity += (desiredYawVelocity - m_yawVelocity) * yawVelocityAlpha;
        const float yawStep = m_yawVelocity * safeDt;
        if (std::abs(yawStep) >= std::abs(yawDelta))
        {
            m_orbitYaw = desiredRearYaw;
            m_yawVelocity = 0.0f;
        }
        else
        {
            m_orbitYaw += yawStep;
        }
        const float lookDownRadians =
            XMConvertToRadians(std::clamp(m_lookDownDegrees, 0.0f, 89.0f));
        const float horizontalDistance =
            std::cos(lookDownRadians) * m_followDistance;
        const float verticalDistance =
            std::sin(lookDownRadians) * m_followDistance;
        const XMVECTOR desiredPosition =
            pivot + XMVectorSet(
                std::sin(m_orbitYaw) * horizontalDistance,
                verticalDistance,
                std::cos(m_orbitYaw) * horizontalDistance, 0.0f);
        const XMVECTOR acceleration =
            (desiredPosition - position) * (positionSpeed * positionSpeed) -
            velocity * (2.0f * damping * positionSpeed);
        velocity += acceleration * safeDt;
        position += velocity * safeDt;
        XMStoreFloat3(&camera.pos, position);
        XMStoreFloat3(&m_followVelocity, velocity);
        if (m_fovSpringActive)
        {
            const float fovAcceleration =
                (m_fovTarget - camera.fov) * (positionSpeed * positionSpeed) -
                m_fovVelocity * (2.0f * damping * positionSpeed);
            m_fovVelocity += fovAcceleration * safeDt;
            camera.fov += m_fovVelocity * safeDt;
            if (std::abs(m_fovTarget - camera.fov) < 0.001f &&
                std::abs(m_fovVelocity) < 0.001f)
            {
                camera.fov = m_fovTarget;
                m_fovVelocity = 0.0f;
                m_fovSpringActive = false;
            }
        }
        const XMVECTOR desiredGaze = pivot;
        const XMVECTOR currentGaze = XMLoadFloat3(&camera.gazePoint);
        XMStoreFloat3(&camera.gazePoint,
            XMVectorLerp(currentGaze, desiredGaze, rotationAlpha));
    }

    void CameraController::ApplyCameraPreset(
        const DirectX::XMFLOAT3& offset,
        const Tank::Physics::TrackedVehicleTestState& state,
        Engine::CameraState& camera)
    {
        const DirectX::XMFLOAT3 pivot = {
            state.bodyPosition.x,
            state.bodyPosition.y + 0.5f,
            state.bodyPosition.z
        };
        camera.pos = {
            pivot.x + offset.x,
            pivot.y + offset.y,
            pivot.z + offset.z
        };
        camera.gazePoint = pivot;
    }

    void CameraController::OnButton4Pressed()
    {
        SelectSlot((m_selectedSlot + 1) % 3, true);
    }

    void CameraController::OnButton7Pressed()
    {
        SelectSlot((m_selectedSlot + 2) % 3, true);
    }

    void CameraController::UpdateButtonStates(
        bool button4Pressed,
        bool button7Pressed)
    {
        if (button4Pressed && !m_button4WasPressed)
        {
            OnButton4Pressed();
        }
        else if (button7Pressed && !m_button7WasPressed)
        {
            OnButton7Pressed();
        }
        m_button4WasPressed = button4Pressed;
        m_button7WasPressed = button7Pressed;
    }

}
