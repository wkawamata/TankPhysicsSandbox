#include "PhysicsDebugOverlay.h"

#include "Scene/SceneBuilder.h"

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    void SetInstanceWorld(Engine::InstanceData& instance, FXMMATRIX world)
    {
        instance.prevWorld = instance.world;
        XMStoreFloat4x4(&instance.world, XMMatrixTranspose(world));
    }

    XMMATRIX MakeLineTransform(
        const Tank::Physics::Vec3& start,
        const Tank::Physics::Vec3& end,
        float thickness)
    {
        const XMVECTOR startVector = XMVectorSet(start.x, start.y, start.z, 1.0f);
        const XMVECTOR endVector = XMVectorSet(end.x, end.y, end.z, 1.0f);
        const XMVECTOR delta = XMVectorSubtract(endVector, startVector);
        const float length = XMVectorGetX(XMVector3Length(delta));
        if (length <= 0.0001f)
        {
            return XMMatrixScaling(0.0f, 0.0f, 0.0f);
        }

        const XMVECTOR forward = XMVectorScale(delta, 1.0f / length);
        const float forwardY = std::abs(XMVectorGetY(forward));
        const XMVECTOR referenceUp =
            forwardY < 0.99f
            ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
            : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(referenceUp, forward));
        const XMVECTOR up = XMVector3Cross(forward, right);
        const XMVECTOR midpoint = XMVectorScale(XMVectorAdd(startVector, endVector), 0.5f);

        XMFLOAT3 rightValues;
        XMFLOAT3 upValues;
        XMFLOAT3 forwardValues;
        XMFLOAT3 midpointValues;
        XMStoreFloat3(&rightValues, right);
        XMStoreFloat3(&upValues, up);
        XMStoreFloat3(&forwardValues, forward);
        XMStoreFloat3(&midpointValues, midpoint);
        const XMMATRIX orientation = XMMatrixSet(
            rightValues.x, rightValues.y, rightValues.z, 0.0f,
            upValues.x, upValues.y, upValues.z, 0.0f,
            forwardValues.x, forwardValues.y, forwardValues.z, 0.0f,
            midpointValues.x, midpointValues.y, midpointValues.z, 1.0f);
        return XMMatrixScaling(thickness, thickness, length) * orientation;
    }
}

PhysicsDebugOverlay::PhysicsDebugOverlay(Engine::SceneBuilder& sceneBuilder)
    : m_sceneBuilder(sceneBuilder)
{
}

void PhysicsDebugOverlay::BuildScene(uint32_t trackProxyMaterial)
{
    m_trackProxyMaterial = trackProxyMaterial;
    m_debugContactMaterial =
        m_sceneBuilder.AddSolidColorMaterial(60, 230, 90, 255);
    m_debugAirborneMaterial =
        m_sceneBuilder.AddSolidColorMaterial(255, 145, 35, 255);

    const uint32_t debugSuspensionMaterial =
        m_sceneBuilder.AddSolidColorMaterial(40, 210, 230, 255);
    const uint32_t debugNormalMaterial =
        m_sceneBuilder.AddSolidColorMaterial(255, 225, 45, 255);

    m_leftTrack = m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(0.0f, 0.0f, 0.0f),
        m_trackProxyMaterial);

    m_rightTrack = m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(0.0f, 0.0f, 0.0f),
        m_trackProxyMaterial);

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        const size_t wheelIndex = static_cast<size_t>(i);
        m_suspensionLines[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            debugSuspensionMaterial);
        m_contactMarkers[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            m_debugAirborneMaterial);
        m_contactNormalLines[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            debugNormalMaterial);
    }
}

void PhysicsDebugOverlay::UpdateScene(
    const Tank::Physics::TrackedVehicleTestState& state,
    const Tank::Physics::TankSettings& tankSettings,
    bool showTrackProxies,
    bool visible)
{
    Engine::Scene& scene = m_sceneBuilder.GetScene();

    const XMVECTOR bodyRotation = XMVectorSet(
        state.bodyRotation.x, state.bodyRotation.y, state.bodyRotation.z, state.bodyRotation.w);
    const XMMATRIX bodyTransform =
        XMMatrixRotationQuaternion(bodyRotation) *
        XMMatrixTranslation(state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);

    const float chassisLength = tankSettings.chassisLengthM;

    Engine::InstanceData& leftTrackInst = scene.instances[m_leftTrack];
    Engine::InstanceData& rightTrackInst = scene.instances[m_rightTrack];
    if (showTrackProxies)
    {
        const XMMATRIX leftLocal = XMMatrixScaling(
            tankSettings.trackWidthM, 0.5f, chassisLength) *
            XMMatrixTranslation(-0.5f * tankSettings.trackSpacingM, 0.0f, 0.0f);
        SetInstanceWorld(leftTrackInst, leftLocal * bodyTransform);
        const XMMATRIX rightLocal = XMMatrixScaling(
            tankSettings.trackWidthM, 0.5f, chassisLength) *
            XMMatrixTranslation(0.5f * tankSettings.trackSpacingM, 0.0f, 0.0f);
        SetInstanceWorld(rightTrackInst, rightLocal * bodyTransform);
    }
    else
    {
        SetInstanceWorld(leftTrackInst, XMMatrixScaling(0.0f, 0.0f, 0.0f));
        SetInstanceWorld(rightTrackInst, XMMatrixScaling(0.0f, 0.0f, 0.0f));
    }

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        const size_t wheelIndex = static_cast<size_t>(i);
        Engine::InstanceData& suspensionLine =
            scene.instances[m_suspensionLines[wheelIndex]];
        Engine::InstanceData& contactMarker =
            scene.instances[m_contactMarkers[wheelIndex]];
        Engine::InstanceData& contactNormalLine =
            scene.instances[m_contactNormalLines[wheelIndex]];

        if (!visible || i >= state.wheelCount)
        {
            const XMMATRIX hidden = XMMatrixScaling(0.0f, 0.0f, 0.0f);
            SetInstanceWorld(suspensionLine, hidden);
            SetInstanceWorld(contactMarker, hidden);
            SetInstanceWorld(contactNormalLine, hidden);
            continue;
        }

        const Tank::Physics::TrackedWheelState& wheel = state.wheels[wheelIndex];
        const int wheelsPerSurface = tankSettings.roadWheelCount + 2;
        const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
        const bool endWheel =
            wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
        const float wheelRadius = endWheel
            ? tankSettings.endWheelRadiusM
            : tankSettings.roadWheelRadiusM;
        const Tank::Physics::Vec3 suspensionEnd = {
            wheel.suspensionOrigin.x +
                wheel.suspensionDirection.x *
                    (wheel.suspensionLength + wheelRadius),
            wheel.suspensionOrigin.y +
                wheel.suspensionDirection.y *
                    (wheel.suspensionLength + wheelRadius),
            wheel.suspensionOrigin.z +
                wheel.suspensionDirection.z *
                    (wheel.suspensionLength + wheelRadius) };
        SetInstanceWorld(
            suspensionLine,
            MakeLineTransform(wheel.suspensionOrigin, suspensionEnd, 0.06f));

        const Tank::Physics::Vec3 markerPosition = wheel.hasContact
            ? Tank::Physics::Vec3 {
                wheel.contactPosition.x + wheel.contactNormal.x * 0.08f,
                wheel.contactPosition.y + wheel.contactNormal.y * 0.08f,
                wheel.contactPosition.z + wheel.contactNormal.z * 0.08f }
            : wheel.transform.position;
        SetInstanceWorld(
            contactMarker,
            XMMatrixScaling(0.22f, 0.22f, 0.22f) *
            XMMatrixTranslation(markerPosition.x, markerPosition.y, markerPosition.z));
        contactMarker.materialId = wheel.hasContact
            ? m_debugContactMaterial
            : m_debugAirborneMaterial;

        if (wheel.hasContact)
        {
            const Tank::Physics::Vec3 normalStart = {
                wheel.contactPosition.x + wheel.contactNormal.x * 0.08f,
                wheel.contactPosition.y + wheel.contactNormal.y * 0.08f,
                wheel.contactPosition.z + wheel.contactNormal.z * 0.08f };
            const Tank::Physics::Vec3 normalEnd = {
                normalStart.x + wheel.contactNormal.x * 2.0f,
                normalStart.y + wheel.contactNormal.y * 2.0f,
                normalStart.z + wheel.contactNormal.z * 2.0f };
            SetInstanceWorld(
                contactNormalLine,
                MakeLineTransform(normalStart, normalEnd, 0.06f));
        }
        else
        {
            SetInstanceWorld(
                contactNormalLine,
                XMMatrixScaling(0.0f, 0.0f, 0.0f));
        }
    }
}

void PhysicsDebugOverlay::Clear()
{
}
