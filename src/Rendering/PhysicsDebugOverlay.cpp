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
    constexpr size_t kMaximumShapesPerTank = 17;
    constexpr size_t kMaximumLinesPerShape = 64;

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

    Tank::Physics::Vec3 TransformPoint(const Tank::Physics::Vec3& point,
        FXMMATRIX transform)
    {
        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3TransformCoord(
            XMVectorSet(point.x, point.y, point.z, 1.0f), transform));
        return {result.x, result.y, result.z};
    }

    void AppendShapeLines(const Tank::Physics::TankShapeSettings& shape,
        FXMMATRIX bodyTransform,
        std::vector<std::pair<Tank::Physics::Vec3,
            Tank::Physics::Vec3>>& lines)
    {
        constexpr float radians = XM_PI / 180.0f;
        const XMMATRIX local =
            XMMatrixRotationRollPitchYaw(
                shape.localRotationDegrees.x * radians,
                shape.localRotationDegrees.y * radians,
                shape.localRotationDegrees.z * radians) *
            XMMatrixTranslation(shape.localPosition.x,
                shape.localPosition.y, shape.localPosition.z) * bodyTransform;
        const auto add = [&](Tank::Physics::Vec3 a, Tank::Physics::Vec3 b)
        {
            lines.emplace_back(TransformPoint(a, local),
                TransformPoint(b, local));
        };
        if (shape.kind == Tank::Physics::TankShapeKind::Box)
        {
            const float x = 0.5f * shape.sizeMeters.x;
            const float y = 0.5f * shape.sizeMeters.y;
            const float z = 0.5f * shape.sizeMeters.z;
            for (const float sy : {-y, y})
                for (const float sz : {-z, z}) add({-x, sy, sz}, {x, sy, sz});
            for (const float sx : {-x, x})
                for (const float sz : {-z, z}) add({sx, -y, sz}, {sx, y, sz});
            for (const float sx : {-x, x})
                for (const float sy : {-y, y}) add({sx, sy, -z}, {sx, sy, z});
            return;
        }

        const float radiusX = 0.5f * shape.sizeMeters.x;
        const float radiusY = 0.5f * shape.sizeMeters.y;
        const float halfCylinder = 0.5f * shape.sizeMeters.z - radiusY;
        constexpr int ringSegments = 8;
        for (const float z : {-halfCylinder, halfCylinder})
        {
            for (int segment = 0; segment < ringSegments; ++segment)
            {
                const float a = XM_2PI * segment / ringSegments;
                const float b = XM_2PI * (segment + 1) / ringSegments;
                add({radiusX * std::cos(a), radiusY * std::sin(a), z},
                    {radiusX * std::cos(b), radiusY * std::sin(b), z});
            }
        }
        for (int segment = 0; segment < 4; ++segment)
        {
            const float angle = XM_PIDIV2 * segment;
            const float x = radiusX * std::cos(angle);
            const float y = radiusY * std::sin(angle);
            add({x, y, -halfCylinder}, {x, y, halfCylinder});
        }
        constexpr int arcSegments = 4;
        for (int plane = 0; plane < 2; ++plane)
            for (const float end : {-1.0f, 1.0f})
                for (int side = -1; side <= 1; side += 2)
                    for (int segment = 0; segment < arcSegments; ++segment)
                    {
                        const float a = XM_PIDIV2 * segment / arcSegments;
                        const float b = XM_PIDIV2 * (segment + 1) / arcSegments;
                        const auto point = [&](float angle)
                        {
                            const float radialRadius =
                                plane == 0 ? radiusX : radiusY;
                            const float radial = side * radialRadius *
                                std::cos(angle);
                            const float z = end * (halfCylinder +
                                radiusY * std::sin(angle));
                            return plane == 0
                                ? Tank::Physics::Vec3{radial, 0, z}
                                : Tank::Physics::Vec3{0, radial, z};
                        };
                        add(point(a), point(b));
                    }
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
    m_movementShapeMaterial =
        m_sceneBuilder.AddSolidColorMaterial(40, 220, 235, 255);
    m_hitRegionMaterial =
        m_sceneBuilder.AddSolidColorMaterial(255, 55, 190, 255);

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
    EnsureTankShapeCapacity(0);
}

void PhysicsDebugOverlay::EnsureTankShapeCapacity(size_t enemyCount)
{
    const size_t required = (enemyCount + 1) * kMaximumShapesPerTank *
        kMaximumLinesPerShape;
    while (m_tankShapeCapacity < required)
    {
        m_tankShapeLines.push_back(m_sceneBuilder.GetScene().instances.size());
        m_sceneBuilder.AddInstance(XMMatrixScaling(0, 0, 0),
            m_hitRegionMaterial);
        ++m_tankShapeCapacity;
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

    std::vector<std::pair<Tank::Physics::Vec3, Tank::Physics::Vec3>> shapeLines;
    std::vector<uint32_t> shapeMaterials;
    const auto appendProfile = [&](const Tank::Physics::TankCollisionProfile& profile,
        FXMMATRIX transform)
    {
        AppendShapeLines(profile.movementShape, transform, shapeLines);
        shapeMaterials.resize(shapeLines.size(), m_movementShapeMaterial);
        for (const auto& region : profile.hitRegions)
        {
            const size_t regionBefore = shapeLines.size();
            AppendShapeLines(region.shape, transform, shapeLines);
            shapeMaterials.resize(shapeLines.size(), m_hitRegionMaterial);
            for (size_t i = regionBefore; i < shapeLines.size(); ++i)
                shapeMaterials[i] = m_hitRegionMaterial;
        }
    };
    if (visible)
    {
        appendProfile(tankSettings.collisionProfile, bodyTransform);
        for (const auto& enemy : state.fixedTurrets)
        {
            if (!enemy.target.active) continue;
            const XMMATRIX enemyTransform = XMMatrixRotationQuaternion(XMVectorSet(
                enemy.rotation.x, enemy.rotation.y, enemy.rotation.z,
                enemy.rotation.w)) * XMMatrixTranslation(enemy.position.x,
                enemy.position.y, enemy.position.z);
            appendProfile(enemy.unitType.collisionProfile, enemyTransform);
        }
    }
    for (size_t index = 0; index < m_tankShapeLines.size(); ++index)
    {
        auto& instance = scene.instances[m_tankShapeLines[index]];
        if (index < shapeLines.size())
        {
            SetInstanceWorld(instance, MakeLineTransform(shapeLines[index].first,
                shapeLines[index].second, 0.025f));
            instance.materialId = shapeMaterials[index];
        }
        else
        {
            SetInstanceWorld(instance, XMMatrixScaling(0, 0, 0));
        }
    }

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
    m_tankShapeLines.clear();
    m_tankShapeCapacity = 0;
}
