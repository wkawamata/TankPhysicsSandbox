#include "TrackedVehicleScenePresenter.h"

#include "Physics/TestObstacleLayout.h"

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>
#include <GltfLoader.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace DirectX;

namespace
{
    std::vector<uint8_t> CreateGroundGridTexture(uint32_t size)
    {
        constexpr uint8_t groundR = 98;
        constexpr uint8_t groundG = 91;
        constexpr uint8_t groundB = 72;
        constexpr uint8_t lineR = 165;
        constexpr uint8_t lineG = 158;
        constexpr uint8_t lineB = 132;
        constexpr uint32_t lineWidth = 2;

        std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
        for (uint32_t y = 0; y < size; ++y)
        {
            for (uint32_t x = 0; x < size; ++x)
            {
                const bool line =
                    x < lineWidth || y < lineWidth ||
                    x >= size - lineWidth || y >= size - lineWidth;
                const size_t pixel = (static_cast<size_t>(y) * size + x) * 4;
                pixels[pixel + 0] = line ? lineR : groundR;
                pixels[pixel + 1] = line ? lineG : groundG;
                pixels[pixel + 2] = line ? lineB : groundB;
                pixels[pixel + 3] = 255;
            }
        }
        return pixels;
    }

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

    struct TrackShoePose
    {
        float y = 0.0f;
        float z = 0.0f;
        float tangentY = 0.0f;
        float tangentZ = 1.0f;
    };

    struct TrackPathPoint
    {
        float y = 0.0f;
        float z = 0.0f;
        float tangentY = 0.0f;
        float tangentZ = 1.0f;
        float distanceFromStart = 0.0f;
    };

    struct TrackPathCandidate
    {
        float y = 0.0f;
        float z = 0.0f;
    };

    float TrackPathCross(
        const TrackPathCandidate& origin,
        const TrackPathCandidate& a,
        const TrackPathCandidate& b)
    {
        return
            (a.z - origin.z) * (b.y - origin.y) -
            (a.y - origin.y) * (b.z - origin.z);
    }

    std::vector<TrackPathPoint> BuildTrackPathFromCandidates(
        std::vector<TrackPathCandidate> candidates)
    {
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const TrackPathCandidate& a, const TrackPathCandidate& b)
            {
                return a.z != b.z ? a.z < b.z : a.y < b.y;
            });

        std::vector<TrackPathCandidate> hull;
        hull.reserve(candidates.size() + 1);
        for (const TrackPathCandidate& candidate : candidates)
        {
            while (hull.size() >= 2 &&
                TrackPathCross(hull[hull.size() - 2], hull.back(), candidate) <= 0.0f)
            {
                hull.pop_back();
            }
            hull.push_back(candidate);
        }

        const size_t lowerHullSize = hull.size();
        for (auto candidate = candidates.rbegin() + 1; candidate != candidates.rend(); ++candidate)
        {
            while (hull.size() > lowerHullSize &&
                TrackPathCross(hull[hull.size() - 2], hull.back(), *candidate) <= 0.0f)
            {
                hull.pop_back();
            }
            hull.push_back(*candidate);
        }

        if (hull.size() < 4)
        {
            return {};
        }
        hull.back() = hull.front();

        std::vector<TrackPathPoint> path;
        path.reserve(hull.size());
        for (const TrackPathCandidate& candidate : hull)
        {
            TrackPathPoint point;
            point.y = candidate.y;
            point.z = candidate.z;
            if (!path.empty())
            {
                const TrackPathPoint& previous = path.back();
                const float deltaY = point.y - previous.y;
                const float deltaZ = point.z - previous.z;
                const float segmentLength = std::sqrt(deltaY * deltaY + deltaZ * deltaZ);
                point.distanceFromStart = previous.distanceFromStart + segmentLength;
                if (segmentLength > 0.0f)
                {
                    path.back().tangentY = deltaY / segmentLength;
                    path.back().tangentZ = deltaZ / segmentLength;
                }
            }
            path.push_back(point);
        }
        path.back().tangentY = path.front().tangentY;
        path.back().tangentZ = path.front().tangentZ;
        return path;
    }

    std::vector<TrackPathPoint> BuildTrackPath(float chassisLength, float radius)
    {
        constexpr int kArcSegments = 8;
        const float halfStraight = (std::max)(0.1f, 0.5f * chassisLength - radius);
        std::vector<TrackPathPoint> path;
        path.reserve(2 * kArcSegments + 4);

        auto appendPoint = [&path](float y, float z)
        {
            TrackPathPoint point;
            point.y = y;
            point.z = z;
            if (!path.empty())
            {
                const TrackPathPoint& previous = path.back();
                const float deltaY = y - previous.y;
                const float deltaZ = z - previous.z;
                const float segmentLength = std::sqrt(deltaY * deltaY + deltaZ * deltaZ);
                point.distanceFromStart = previous.distanceFromStart + segmentLength;
                if (segmentLength > 0.0f)
                {
                    path.back().tangentY = deltaY / segmentLength;
                    path.back().tangentZ = deltaZ / segmentLength;
                }
            }
            path.push_back(point);
        };

        appendPoint(-radius, -halfStraight);
        appendPoint(-radius, halfStraight);
        for (int segment = 1; segment <= kArcSegments; ++segment)
        {
            const float angle =
                -0.5f * XM_PI +
                XM_PI * static_cast<float>(segment) / static_cast<float>(kArcSegments);
            appendPoint(
                radius * std::sin(angle),
                halfStraight + radius * std::cos(angle));
        }
        appendPoint(radius, -halfStraight);
        for (int segment = 1; segment <= kArcSegments; ++segment)
        {
            const float angle =
                0.5f * XM_PI +
                XM_PI * static_cast<float>(segment) / static_cast<float>(kArcSegments);
            appendPoint(
                radius * std::sin(angle),
                -halfStraight + radius * std::cos(angle));
        }

        if (path.size() >= 2)
        {
            path.back().tangentY = path.front().tangentY;
            path.back().tangentZ = path.front().tangentZ;
        }
        return path;
    }

    std::vector<TrackPathPoint> BuildTrackPathFromWheels(
        const Tank::Physics::TrackedVehicleTestState& state,
        const Tank::Physics::TankSettings& settings,
        int trackIndex,
        DirectX::FXMMATRIX inverseBodyTransform)
    {
        constexpr int kWheelPathSegments = 32;
        constexpr float kTrackClearanceM = 0.08f;
        std::vector<TrackPathCandidate> candidates;
        candidates.reserve(
            static_cast<size_t>(state.wheelCount) *
            static_cast<size_t>(kWheelPathSegments));

        const int wheelsPerSurface = settings.roadWheelCount + 2;
        for (int wheelIndex = 0; wheelIndex < state.wheelCount; ++wheelIndex)
        {
            const Tank::Physics::TrackedWheelState& wheel =
                state.wheels[static_cast<size_t>(wheelIndex)];
            if (wheel.trackIndex != trackIndex)
            {
                continue;
            }

            const XMVECTOR worldCenter = XMVectorSet(
                wheel.transform.position.x,
                wheel.transform.position.y,
                wheel.transform.position.z,
                1.0f);
            const XMVECTOR localCenter =
                XMVector3TransformCoord(worldCenter, inverseBodyTransform);
            const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
            const bool endWheel =
                wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
            const float wheelRadius = endWheel
                ? settings.endWheelRadiusM
                : settings.roadWheelRadiusM;
            const float pathRadius =
                (wheelRadius + kTrackClearanceM) /
                std::cos(XM_PI / static_cast<float>(kWheelPathSegments));

            for (int segment = 0; segment < kWheelPathSegments; ++segment)
            {
                const float angle =
                    2.0f * XM_PI * static_cast<float>(segment) /
                    static_cast<float>(kWheelPathSegments);
                candidates.push_back({
                    XMVectorGetY(localCenter) + pathRadius * std::sin(angle),
                    XMVectorGetZ(localCenter) + pathRadius * std::cos(angle) });
            }
        }

        if (candidates.empty())
        {
            return {};
        }
        return BuildTrackPathFromCandidates(std::move(candidates));
    }

    TrackShoePose CalculateTrackShoePose(
        const std::vector<TrackPathPoint>& path,
        float distance)
    {
        if (path.size() < 2 || path.back().distanceFromStart <= 0.0f)
        {
            return {};
        }

        const float pathLength = path.back().distanceFromStart;
        distance = std::fmod(distance, pathLength);
        if (distance < 0.0f)
        {
            distance += pathLength;
        }

        const auto end = std::upper_bound(
            path.begin(),
            path.end(),
            distance,
            [](float value, const TrackPathPoint& point)
            {
                return value < point.distanceFromStart;
            });
        const size_t endIndex = static_cast<size_t>(std::distance(path.begin(), end));
        const size_t nextIndex = (std::max)(size_t{ 1 }, endIndex);
        const TrackPathPoint& start = path[nextIndex - 1];
        const TrackPathPoint& next = path[nextIndex];
        const float segmentLength = next.distanceFromStart - start.distanceFromStart;
        const float t = segmentLength > 0.0f
            ? (distance - start.distanceFromStart) / segmentLength
            : 0.0f;
        const float tangentY = start.tangentY + (next.tangentY - start.tangentY) * t;
        const float tangentZ = start.tangentZ + (next.tangentZ - start.tangentZ) * t;
        const float tangentLength =
            std::sqrt(tangentY * tangentY + tangentZ * tangentZ);

        return {
            start.y + (next.y - start.y) * t,
            start.z + (next.z - start.z) * t,
            tangentLength > 0.0f ? tangentY / tangentLength : 0.0f,
            tangentLength > 0.0f ? tangentZ / tangentLength : 1.0f };
    }

    XMMATRIX MakeTrackShoeLocalTransform(
        float x,
        const TrackShoePose& pose,
        float width,
        float thickness,
        float length)
    {
        const XMMATRIX orientation = XMMatrixSet(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, pose.tangentZ, -pose.tangentY, 0.0f,
            0.0f, pose.tangentY, pose.tangentZ, 0.0f,
            x, pose.y, pose.z, 1.0f);
        return XMMatrixScaling(width, thickness, length) * orientation;
    }
}

void TrackedVehicleScenePresenter::BuildScene(
    const Tank::Physics::PhysicsEnvironmentSettings& envSettings,
    const Tank::Rendering::TankVisualSettings& visualSettings,
    const Tank::Physics::TankSettings& tankSettings)
{
    m_sceneBuilder.Clear();

    uint32_t floorMaterial = 0;
    if (envSettings.gridEnabled)
    {
        constexpr uint32_t gridTextureSize = 128;
        const std::vector<uint8_t> gridPixels = CreateGroundGridTexture(gridTextureSize);
        const uint32_t gridTexture = m_sceneBuilder.AddTextureRGBA8(
            gridTextureSize,
            gridTextureSize,
            gridPixels);
        const float gridSpacingM = std::max(envSettings.gridSpacingM, 0.5f);
        const float gridRepeat = envSettings.floorSizeM / gridSpacingM;
        floorMaterial = m_sceneBuilder.AddTexturedMaterial(
            gridTexture,
            { gridRepeat, gridRepeat });
    }
    else
    {
        floorMaterial =
            m_sceneBuilder.AddSolidColorMaterial(80, 80, 80, 255);
    }

    auto addBodyMaterial = [this](const Tank::Rendering::BodyMaterialSettings& material)
    {
        const uint32_t materialId = m_sceneBuilder.AddSolidColorMaterial(
            static_cast<uint8_t>(std::clamp(material.albedo.r, 0.0f, 1.0f) * 255.0f),
            static_cast<uint8_t>(std::clamp(material.albedo.g, 0.0f, 1.0f) * 255.0f),
            static_cast<uint8_t>(std::clamp(material.albedo.b, 0.0f, 1.0f) * 255.0f),
            255);
        Engine::SceneMaterial& sceneMaterial =
            m_sceneBuilder.GetMesh().materials[materialId];
        sceneMaterial.roughnessFactor = std::clamp(material.roughness, 0.04f, 1.0f);
        sceneMaterial.metallicFactor = std::clamp(material.metallic, 0.0f, 1.0f);
        sceneMaterial.ambientOcclusionFactor =
            std::clamp(material.ambientOcclusion, 0.0f, 1.0f);
        sceneMaterial.emissiveScale = std::clamp(material.emissive, 0.0f, 4.0f);
        return materialId;
    };

    m_model.hullUpperMaterial = addBodyMaterial(visualSettings.hullUpper);
    m_model.hullLowerMaterial = addBodyMaterial(visualSettings.hullLower);
    m_model.structureUpperMaterial =
        addBodyMaterial(visualSettings.structureUpper);
    m_model.structureLowerMaterial =
        addBodyMaterial(visualSettings.structureLower);
    m_model.trackProxyMaterial =
        addBodyMaterial(visualSettings.trackProxies);
    m_model.forwardMarkerMaterial =
        addBodyMaterial(visualSettings.forwardMarker);
    m_model.wheelMaterial = addBodyMaterial(visualSettings.wheels);
    m_model.contactedWheelMaterial =
        addBodyMaterial(visualSettings.contactedWheels);
    m_model.trackShoeMaterial =
        addBodyMaterial(visualSettings.trackShoes);

    const uint32_t obstacleMaterial =
        m_sceneBuilder.AddSolidColorMaterial(70, 95, 135, 255);
    m_model.debugContactMaterial =
        m_sceneBuilder.AddSolidColorMaterial(60, 230, 90, 255);
    m_model.debugAirborneMaterial =
        m_sceneBuilder.AddSolidColorMaterial(255, 145, 35, 255);
    const uint32_t debugSuspensionMaterial =
        m_sceneBuilder.AddSolidColorMaterial(40, 210, 230, 255);
    const uint32_t debugNormalMaterial =
        m_sceneBuilder.AddSolidColorMaterial(255, 225, 45, 255);

    m_sceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    const Engine::SceneMeshId wheelMesh = m_sceneBuilder.AddCylinder(
        1.0f,
        1.0f,
        16,
        Engine::CylinderCapMode::Both);

    m_sceneBuilder.AddInstance(
        XMMatrixScaling(
            envSettings.floorSizeM,
            0.2f,
            envSettings.floorSizeM) *
            XMMatrixTranslation(0.0f, -0.1f, 0.0f),
        floorMaterial);

    m_model.hullUpper =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(2.16f, 0.25f, 3.5f) *
            XMMatrixTranslation(0.0f, 2.125f, 0.0f),
        m_model.hullUpperMaterial);
    m_model.hullLower =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(2.16f, 0.25f, 3.5f) *
            XMMatrixTranslation(0.0f, 1.875f, 0.0f),
        m_model.hullLowerMaterial);
    m_model.upperStructureUpper =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(1.44f, 0.125f, 2.0f) *
            XMMatrixTranslation(0.0f, 2.4375f, 0.3f),
        m_model.structureUpperMaterial);
    m_model.upperStructureLower =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(1.44f, 0.125f, 2.0f) *
            XMMatrixTranslation(0.0f, 2.3125f, 0.3f),
        m_model.structureLowerMaterial);
    m_model.lowerStructureUpper =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(1.44f, 0.125f, 2.0f) *
            XMMatrixTranslation(0.0f, 1.6875f, 0.3f),
        m_model.structureUpperMaterial);
    m_model.lowerStructureLower =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(1.44f, 0.125f, 2.0f) *
            XMMatrixTranslation(0.0f, 1.5625f, 0.3f),
        m_model.structureLowerMaterial);

    m_model.leftTrack =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(tankSettings.trackWidthM, 0.5f, 4.0f) *
        XMMatrixTranslation(-0.5f * tankSettings.trackSpacingM, 2.0f, 0.0f),
        m_model.trackProxyMaterial);

    m_model.rightTrack =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(tankSettings.trackWidthM, 0.5f, 4.0f) *
        XMMatrixTranslation(0.5f * tankSettings.trackSpacingM, 2.0f, 0.0f),
        m_model.trackProxyMaterial);

    m_model.forwardMarker =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(0.3f, 0.3f, 0.3f) * XMMatrixTranslation(0.0f, 2.0f, 2.5f),
        m_model.forwardMarkerMaterial);

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        m_model.wheels[static_cast<size_t>(i)] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            wheelMesh,
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            m_model.wheelMaterial);
    }

    for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
    {
        for (int shoe = 0; shoe < TrackedVehicleModel::kTrackShoeCountPerTrack; ++shoe)
        {
            m_model.trackShoes[static_cast<size_t>(track)]
                [static_cast<size_t>(shoe)] =
                m_sceneBuilder.GetScene().instances.size();
            m_sceneBuilder.AddInstance(
                XMMatrixScaling(0.0f, 0.0f, 0.0f),
                m_model.trackShoeMaterial);
        }
    }

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        const size_t wheelIndex = static_cast<size_t>(i);
        m_model.suspensionLines[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            debugSuspensionMaterial);
        m_model.contactMarkers[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            m_model.debugAirborneMaterial);
        m_model.contactNormalLines[wheelIndex] =
            m_sceneBuilder.GetScene().instances.size();
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(0.0f, 0.0f, 0.0f),
            debugNormalMaterial);
    }

    for (const Tank::Physics::TestObstaclePlacement& obstacle :
        Tank::Physics::GenerateTestObstacleLayout(envSettings))
    {
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(
                Tank::Physics::kPassengerCarWidthM,
                Tank::Physics::kPassengerCarHeightM,
                Tank::Physics::kPassengerCarLengthM) *
            XMMatrixRotationY(obstacle.yawRadians) *
            XMMatrixTranslation(
                obstacle.position.x,
                obstacle.position.y,
                obstacle.position.z),
            obstacleMaterial);
    }

    Engine::CameraState camera;
    camera.pos = { 0.0f, 10.0f, -16.0f };
    camera.gazePoint = { 0.0f, 0.8f, 0.0f };
    camera.fov = 35.0f;
    camera.nearZ = 0.1f;
    camera.farZ = 10000.0f;
    m_sceneBuilder.SetCamera(camera);

    m_trackShoeDistances.fill(0.0f);
    m_trackShoeLastTimeSeconds = 0.0f;
}

void TrackedVehicleScenePresenter::UpdateScene(
    const Tank::Physics::TrackedVehicleTestState& state,
    const Tank::Physics::TankSettings& tankSettings,
    const Tank::Rendering::TankVisualSettings& visualSettings,
    bool showTrackShoes,
    bool showTrackProxies,
    bool physicsDebugOverlay)
{
    Engine::Scene& scene = m_sceneBuilder.GetScene();
    const XMVECTOR rotation = XMVectorSet(
        state.bodyRotation.x, state.bodyRotation.y, state.bodyRotation.z, state.bodyRotation.w);
    const XMMATRIX bodyTransform =
        XMMatrixRotationQuaternion(rotation) *
        XMMatrixTranslation(state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
    const float chassisWidth = tankSettings.chassisWidthM;
    const float chassisLength = tankSettings.chassisLengthM;

    struct Part { size_t index; XMMATRIX localTransform; };
    const Part parts[] = {
        { m_model.hullUpper,
            XMMatrixScaling(0.9f * chassisWidth, 0.25f, 0.875f * chassisLength) *
                XMMatrixTranslation(0.0f, 0.125f, 0.0f) },
        { m_model.hullLower,
            XMMatrixScaling(0.9f * chassisWidth, 0.25f, 0.875f * chassisLength) *
                XMMatrixTranslation(0.0f, -0.125f, 0.0f) },
        { m_model.upperStructureUpper,
            XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
                XMMatrixTranslation(0.0f, 0.4375f, 0.075f * chassisLength) },
        { m_model.upperStructureLower,
            XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
                XMMatrixTranslation(0.0f, 0.3125f, 0.075f * chassisLength) },
        { m_model.lowerStructureUpper,
            XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
                XMMatrixTranslation(0.0f, -0.3125f, 0.075f * chassisLength) },
        { m_model.lowerStructureLower,
            XMMatrixScaling(0.6f * chassisWidth, 0.125f, 0.5f * chassisLength) *
                XMMatrixTranslation(0.0f, -0.4375f, 0.075f * chassisLength) },
        { m_model.leftTrack,
            showTrackProxies
                ? XMMatrixScaling(tankSettings.trackWidthM, 0.5f, chassisLength) *
                    XMMatrixTranslation(
                        -0.5f * tankSettings.trackSpacingM,
                        0.0f,
                        0.0f)
                : XMMatrixScaling(0.0f, 0.0f, 0.0f) },
        { m_model.rightTrack,
            showTrackProxies
                ? XMMatrixScaling(tankSettings.trackWidthM, 0.5f, chassisLength) *
                    XMMatrixTranslation(
                        0.5f * tankSettings.trackSpacingM,
                        0.0f,
                        0.0f)
                : XMMatrixScaling(0.0f, 0.0f, 0.0f) },
        { m_model.forwardMarker,
            XMMatrixScaling(0.3f, 0.3f, 0.3f) *
                XMMatrixTranslation(0.0f, 0.0f, 0.625f * chassisLength) },
    };
    for (const Part& part : parts)
    {
        Engine::InstanceData& inst = scene.instances[part.index];
        inst.prevWorld = inst.world;
        const XMMATRIX world = part.localTransform * bodyTransform;
        XMStoreFloat4x4(&inst.world, XMMatrixTranspose(world));
    }

    const float trackRadius = (std::max)(
        tankSettings.endWheelRadiusM,
        tankSettings.roadWheelRadiusM);
    const float halfTrackSpacing = 0.5f * tankSettings.trackSpacingM;
    const XMMATRIX inverseBodyTransform = XMMatrixInverse(nullptr, bodyTransform);
    std::array<std::vector<TrackPathPoint>, Tank::Physics::kTankTrackCount> trackPaths;
    std::array<float, Tank::Physics::kTankTrackCount> trackPerimeters = {};
    for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
    {
        trackPaths[static_cast<size_t>(track)] = BuildTrackPathFromWheels(
            state,
            tankSettings,
            track,
            inverseBodyTransform);
        if (trackPaths[static_cast<size_t>(track)].size() < 2)
        {
            trackPaths[static_cast<size_t>(track)] =
                BuildTrackPath(chassisLength, trackRadius);
        }
        trackPerimeters[static_cast<size_t>(track)] =
            trackPaths[static_cast<size_t>(track)].back().distanceFromStart;
    }
    const float trackDeltaTime = std::clamp(
        state.timeSeconds - m_trackShoeLastTimeSeconds,
        0.0f,
        0.1f);
    m_trackShoeLastTimeSeconds = state.timeSeconds;

    const XMVECTOR bodyRotation = XMVectorSet(
        state.bodyRotation.x,
        state.bodyRotation.y,
        state.bodyRotation.z,
        state.bodyRotation.w);
    const XMVECTOR bodyForward =
        XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), bodyRotation);
    const XMVECTOR bodyUp =
        XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), bodyRotation);
    const XMVECTOR linearVelocity = XMVectorSet(
        state.linearVelocity.x,
        state.linearVelocity.y,
        state.linearVelocity.z,
        0.0f);
    const XMVECTOR angularVelocity = XMVectorSet(
        state.angularVelocity.x,
        state.angularVelocity.y,
        state.angularVelocity.z,
        0.0f);
    const float forwardSpeed =
        XMVectorGetX(XMVector3Dot(linearVelocity, bodyForward));
    const float yawSpeed =
        XMVectorGetX(XMVector3Dot(angularVelocity, bodyUp));
    const std::array<float, Tank::Physics::kTankTrackCount> trackSpeeds = {
        forwardSpeed + yawSpeed * halfTrackSpacing,
        forwardSpeed - yawSpeed * halfTrackSpacing };
    for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
    {
        m_trackShoeDistances[static_cast<size_t>(track)] -=
            trackSpeeds[static_cast<size_t>(track)] * trackDeltaTime;
        m_trackShoeDistances[static_cast<size_t>(track)] = std::fmod(
            m_trackShoeDistances[static_cast<size_t>(track)],
            trackPerimeters[static_cast<size_t>(track)]);
    }

    for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
    {
        const std::vector<TrackPathPoint>& trackPath =
            trackPaths[static_cast<size_t>(track)];
        const float trackPerimeter = trackPerimeters[static_cast<size_t>(track)];
        const float shoeLength =
            0.82f * trackPerimeter /
            static_cast<float>(TrackedVehicleModel::kTrackShoeCountPerTrack);
        const float trackX = track == 0 ? -halfTrackSpacing : halfTrackSpacing;
        for (int shoe = 0; shoe < TrackedVehicleModel::kTrackShoeCountPerTrack; ++shoe)
        {
            Engine::InstanceData& instance =
                scene.instances[m_model.trackShoes[static_cast<size_t>(track)]
                    [static_cast<size_t>(shoe)]];
            if (!showTrackShoes)
            {
                SetInstanceWorld(instance, XMMatrixScaling(0.0f, 0.0f, 0.0f));
                continue;
            }

            const float shoeDistance =
                (static_cast<float>(shoe) + 0.5f) * trackPerimeter /
                    static_cast<float>(TrackedVehicleModel::kTrackShoeCountPerTrack) +
                m_trackShoeDistances[static_cast<size_t>(track)];
            const TrackShoePose pose =
                CalculateTrackShoePose(trackPath, shoeDistance);
            const XMMATRIX shoeLocal = MakeTrackShoeLocalTransform(
                trackX,
                pose,
                tankSettings.trackWidthM + 0.08f,
                0.08f,
                shoeLength);
            SetInstanceWorld(instance, shoeLocal * bodyTransform);
        }
    }

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        Engine::InstanceData& inst =
            scene.instances[m_model.wheels[static_cast<size_t>(i)]];
        inst.prevWorld = inst.world;

        if (i < state.wheelCount)
        {
            const Tank::Physics::TrackedWheelState& wheel = state.wheels[static_cast<size_t>(i)];
            SetInstanceWorld(inst, XMMatrixScaling(0.0f, 0.0f, 0.0f));

            const XMVECTOR wheelRotation = XMVectorSet(
                wheel.transform.rotation.x,
                wheel.transform.rotation.y,
                wheel.transform.rotation.z,
                wheel.transform.rotation.w);
            const int wheelsPerSurface = tankSettings.roadWheelCount + 2;
            const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
            const bool endWheel =
                wheelOnSurface == 0 || wheelOnSurface == wheelsPerSurface - 1;
            const float radius = endWheel
                ? tankSettings.endWheelRadiusM
                : tankSettings.roadWheelRadiusM;
            const XMMATRIX wheelWorld =
                XMMatrixScaling(
                    radius,
                    tankSettings.trackWidthM,
                    radius) *
                XMMatrixRotationQuaternion(wheelRotation) *
                XMMatrixTranslation(
                    wheel.transform.position.x,
                    wheel.transform.position.y,
                    wheel.transform.position.z);
            SetInstanceWorld(inst, wheelWorld);
            inst.materialId =
                visualSettings.colorWheelsByContact && wheel.hasContact
                ? m_model.contactedWheelMaterial
                : m_model.wheelMaterial;
        }
        else
        {
            SetInstanceWorld(inst, XMMatrixScaling(0.0f, 0.0f, 0.0f));
            inst.materialId = m_model.wheelMaterial;
        }
    }

    for (int i = 0; i < Tank::Physics::kTankWheelCount; ++i)
    {
        const size_t wheelIndex = static_cast<size_t>(i);
        Engine::InstanceData& suspensionLine =
            scene.instances[m_model.suspensionLines[wheelIndex]];
        Engine::InstanceData& contactMarker =
            scene.instances[m_model.contactMarkers[wheelIndex]];
        Engine::InstanceData& contactNormalLine =
            scene.instances[m_model.contactNormalLines[wheelIndex]];

        if (!physicsDebugOverlay || i >= state.wheelCount)
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
            ? m_model.debugContactMaterial
            : m_model.debugAirborneMaterial;

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

void TrackedVehicleScenePresenter::ApplyMaterials(
    const Tank::Rendering::TankVisualSettings& visualSettings)
{
    Engine::SceneMesh& mesh = m_sceneBuilder.GetMesh();
    auto applyColor = [&mesh](
        uint32_t materialId,
        const Tank::Rendering::BodyMaterialSettings& settings)
    {
        if (materialId >= mesh.materials.size())
        {
            return;
        }
        const int textureIndex = mesh.materials[materialId].albedoTexIndex;
        if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= mesh.textures.size())
        {
            return;
        }
        Engine::SceneTexture& texture = mesh.textures[static_cast<size_t>(textureIndex)];
        if (texture.pixels.size() < 4)
        {
            return;
        }
        texture.pixels[0] = static_cast<uint8_t>(
            std::clamp(settings.albedo.r, 0.0f, 1.0f) * 255.0f);
        texture.pixels[1] = static_cast<uint8_t>(
            std::clamp(settings.albedo.g, 0.0f, 1.0f) * 255.0f);
        texture.pixels[2] = static_cast<uint8_t>(
            std::clamp(settings.albedo.b, 0.0f, 1.0f) * 255.0f);
        texture.pixels[3] = 255;
        Engine::SceneMaterial& material = mesh.materials[materialId];
        material.roughnessFactor = std::clamp(settings.roughness, 0.04f, 1.0f);
        material.metallicFactor = std::clamp(settings.metallic, 0.0f, 1.0f);
        material.ambientOcclusionFactor =
            std::clamp(settings.ambientOcclusion, 0.0f, 1.0f);
        material.emissiveScale = std::clamp(settings.emissive, 0.0f, 4.0f);
    };

    applyColor(m_model.hullUpperMaterial, visualSettings.hullUpper);
    applyColor(m_model.hullLowerMaterial, visualSettings.hullLower);
    applyColor(
        m_model.structureUpperMaterial,
        visualSettings.structureUpper);
    applyColor(
        m_model.structureLowerMaterial,
        visualSettings.structureLower);
    applyColor(m_model.wheelMaterial, visualSettings.wheels);
    applyColor(
        m_model.contactedWheelMaterial,
        visualSettings.contactedWheels);
    applyColor(m_model.trackShoeMaterial, visualSettings.trackShoes);
    applyColor(m_model.trackProxyMaterial, visualSettings.trackProxies);
    applyColor(
        m_model.forwardMarkerMaterial,
        visualSettings.forwardMarker);
}

void TrackedVehicleScenePresenter::Clear()
{
    m_sceneBuilder.Clear();
}

Engine::SceneBuilder& TrackedVehicleScenePresenter::SceneBuilder()
{
    return m_sceneBuilder;
}

Engine::Scene& TrackedVehicleScenePresenter::GetScene()
{
    return m_sceneBuilder.GetScene();
}

const Engine::Scene& TrackedVehicleScenePresenter::GetScene() const
{
    return m_sceneBuilder.GetScene();
}

TrackedVehicleScenePresenter::TrackedVehicleModel& TrackedVehicleScenePresenter::Model()
{
    return m_model;
}
