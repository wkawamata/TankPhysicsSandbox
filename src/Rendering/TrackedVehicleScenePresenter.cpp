#include "TrackedVehicleScenePresenter.h"

#include "Physics/TestObstacleLayout.h"
#include "Rendering/PhysicsDebugOverlay.h"

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
    Engine::SceneMeshId AddTriangularPrismMesh(Engine::SceneBuilder& builder)
    {
        Engine::SceneMesh& mesh = builder.GetMesh();
        Engine::SceneMesh::Range range;
        range.firstVertex = static_cast<uint32_t>(mesh.vertices.size());
        range.firstIndex = static_cast<uint32_t>(mesh.indices.size());

        const std::array<XMFLOAT3, 6> points = {
            XMFLOAT3 { -0.5f, -0.5f, -0.5f },
            XMFLOAT3 { 0.5f, -0.5f, -0.5f },
            XMFLOAT3 { -0.5f, -0.5f, 0.5f },
            XMFLOAT3 { 0.5f, -0.5f, 0.5f },
            XMFLOAT3 { -0.5f, 0.5f, 0.5f },
            XMFLOAT3 { 0.5f, 0.5f, 0.5f } };

        const auto addTriangle = [&mesh, &points](uint32_t a, uint32_t b, uint32_t c)
        {
            const XMVECTOR va = XMLoadFloat3(&points[a]);
            const XMVECTOR vb = XMLoadFloat3(&points[b]);
            const XMVECTOR vc = XMLoadFloat3(&points[c]);
            XMFLOAT3 normal;
            XMStoreFloat3(
                &normal,
                XMVector3Normalize(XMVector3Cross(vb - va, vc - va)));
            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({ points[a], { 0.0f, 1.0f }, normal });
            mesh.vertices.push_back({ points[b], { 1.0f, 1.0f }, normal });
            mesh.vertices.push_back({ points[c], { 0.5f, 0.0f }, normal });
            mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
        };

        addTriangle(0, 1, 3);
        addTriangle(0, 3, 2);
        addTriangle(2, 3, 5);
        addTriangle(2, 5, 4);
        addTriangle(0, 4, 5);
        addTriangle(0, 5, 1);
        addTriangle(0, 2, 4);
        addTriangle(1, 5, 3);

        range.vertexCount = static_cast<uint32_t>(mesh.vertices.size()) - range.firstVertex;
        range.indexCount = static_cast<uint32_t>(mesh.indices.size()) - range.firstIndex;
        mesh.ranges.push_back(range);
        return static_cast<Engine::SceneMeshId>(mesh.ranges.size() - 1);
    }

    Engine::SceneMeshId AddHeightFieldMesh(
        Engine::SceneBuilder& builder,
        const Tank::Physics::MapPrimitive& primitive)
    {
        Engine::SceneMesh& mesh = builder.GetMesh();
        Engine::SceneMesh::Range range;
        range.firstVertex = static_cast<uint32_t>(mesh.vertices.size());
        range.firstIndex = static_cast<uint32_t>(mesh.indices.size());
        const uint32_t count = primitive.heightFieldSampleCount;
        const float halfSpan =
            0.5f * primitive.heightFieldCellSizeM * static_cast<float>(count - 1);

        const auto point = [&primitive, count, halfSpan](uint32_t x, uint32_t z)
        {
            return XMFLOAT3 {
                static_cast<float>(x) * primitive.heightFieldCellSizeM - halfSpan,
                primitive.heightFieldHeights[static_cast<size_t>(z) * count + x],
                static_cast<float>(z) * primitive.heightFieldCellSizeM - halfSpan };
        };
        const auto addTriangle = [&mesh](
            const XMFLOAT3& a,
            const XMFLOAT3& b,
            const XMFLOAT3& c)
        {
            const XMVECTOR va = XMLoadFloat3(&a);
            const XMVECTOR vb = XMLoadFloat3(&b);
            const XMVECTOR vc = XMLoadFloat3(&c);
            XMFLOAT3 normal;
            XMStoreFloat3(
                &normal,
                XMVector3Normalize(XMVector3Cross(vb - va, vc - va)));
            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({ a, { 0.0f, 0.0f }, normal });
            mesh.vertices.push_back({ b, { 0.0f, 1.0f }, normal });
            mesh.vertices.push_back({ c, { 1.0f, 0.0f }, normal });
            mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
        };

        for (uint32_t z = 0; z + 1 < count; ++z)
        {
            for (uint32_t x = 0; x + 1 < count; ++x)
            {
                const XMFLOAT3 p00 = point(x, z);
                const XMFLOAT3 p10 = point(x + 1, z);
                const XMFLOAT3 p01 = point(x, z + 1);
                const XMFLOAT3 p11 = point(x + 1, z + 1);
                addTriangle(p00, p01, p10);
                addTriangle(p10, p01, p11);
            }
        }

        range.vertexCount = static_cast<uint32_t>(mesh.vertices.size()) - range.firstVertex;
        range.indexCount = static_cast<uint32_t>(mesh.indices.size()) - range.firstIndex;
        mesh.ranges.push_back(range);
        return static_cast<Engine::SceneMeshId>(mesh.ranges.size() - 1);
    }

    std::vector<uint8_t> CreateGroundGridTexture(
        uint32_t size,
        const Tank::Physics::ColorRgb& groundColor,
        const Tank::Physics::ColorRgb& lineColor)
    {
        const uint8_t groundR = static_cast<uint8_t>(
            std::clamp(groundColor.r, 0.0f, 1.0f) * 255.0f);
        const uint8_t groundG = static_cast<uint8_t>(
            std::clamp(groundColor.g, 0.0f, 1.0f) * 255.0f);
        const uint8_t groundB = static_cast<uint8_t>(
            std::clamp(groundColor.b, 0.0f, 1.0f) * 255.0f);
        const uint8_t lineR = static_cast<uint8_t>(
            std::clamp(lineColor.r, 0.0f, 1.0f) * 255.0f);
        const uint8_t lineG = static_cast<uint8_t>(
            std::clamp(lineColor.g, 0.0f, 1.0f) * 255.0f);
        const uint8_t lineB = static_cast<uint8_t>(
            std::clamp(lineColor.b, 0.0f, 1.0f) * 255.0f);
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
    const std::vector<Tank::Physics::MapPrimitive>& mapPrimitives,
    const Tank::Rendering::TankVisualSettings& visualSettings,
    const Tank::Physics::TankSettings&,
    const Engine::GltfSceneAsset* tankModelAsset,
    bool showDefaultFloor)
{
    m_sceneBuilder.Clear();
    m_model = {};

    uint32_t floorMaterial = 0;
    if (envSettings.gridEnabled)
    {
        constexpr uint32_t gridTextureSize = 128;
        const std::vector<uint8_t> gridPixels = CreateGroundGridTexture(
            gridTextureSize,
            envSettings.groundColor,
            envSettings.gridLineColor);
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
        const auto toByte = [](float channel)
        {
            return static_cast<uint8_t>(
                std::clamp(channel, 0.0f, 1.0f) * 255.0f);
        };
        floorMaterial = m_sceneBuilder.AddSolidColorMaterial(
            toByte(envSettings.groundColor.r),
            toByte(envSettings.groundColor.g),
            toByte(envSettings.groundColor.b),
            255);
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

    const std::array<uint32_t, 3> frictionMaterials = {
        m_sceneBuilder.AddSolidColorMaterial(50, 120, 210, 255),
        m_sceneBuilder.AddSolidColorMaterial(70, 145, 85, 255),
        m_sceneBuilder.AddSolidColorMaterial(205, 85, 55, 255) };
    m_sceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    const Engine::SceneMeshId wheelMesh = m_sceneBuilder.AddCylinder(
        1.0f,
        1.0f,
        16,
        Engine::CylinderCapMode::Both);
    const Engine::SceneMeshId triangularPrismMesh =
        AddTriangularPrismMesh(m_sceneBuilder);

    if (showDefaultFloor)
    {
        m_sceneBuilder.AddInstance(
            XMMatrixScaling(
                envSettings.floorSizeM,
                0.2f,
                envSettings.floorSizeM) *
                XMMatrixTranslation(0.0f, -0.1f, 0.0f),
            floorMaterial);
    }

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

    m_model.forwardMarker =
        m_sceneBuilder.GetScene().instances.size();
    m_sceneBuilder.AddInstance(
        XMMatrixScaling(0.3f, 0.3f, 0.3f) * XMMatrixTranslation(0.0f, 2.0f, 2.5f),
        m_model.forwardMarkerMaterial);

    if (tankModelAsset != nullptr)
    {
        const std::vector<std::string> nodeNames =
            Engine::GetGltfMeshNodeNames(*tankModelAsset);
        const std::string bodyNodeName = nodeNames.size() == 1 ?
            nodeNames.front() : "Body";
        const Engine::GltfNodeMeshAddResult body =
            m_sceneBuilder.AddGltfNodeMesh(*tankModelAsset, bodyNodeName);
        if (body)
        {
            m_model.gltfBody = m_sceneBuilder.GetScene().instances.size();
            m_sceneBuilder.AddInstance(*body.meshId, XMMatrixIdentity(), 0);
            m_model.hasGltfBody = true;
        }
        if (nodeNames.size() != 1)
        {
            const Engine::GltfNodeMeshAddResult cannon =
                m_sceneBuilder.AddGltfNodeMesh(*tankModelAsset, "Cannon");
            const Engine::GltfNodeMeshAddResult side =
                m_sceneBuilder.AddGltfNodeMesh(*tankModelAsset, "Side");
            if (cannon)
            {
                m_model.gltfCannon = m_sceneBuilder.GetScene().instances.size();
                m_sceneBuilder.AddInstance(*cannon.meshId, XMMatrixIdentity(), 0);
                m_model.hasGltfCannon = true;
            }
            if (side)
            {
                m_model.gltfSide = m_sceneBuilder.GetScene().instances.size();
                m_sceneBuilder.AddInstance(*side.meshId, XMMatrixIdentity(), 0);
                m_model.hasGltfSide = true;
            }
        }
    }

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

    m_physicsDebugOverlay.BuildScene(m_model.trackProxyMaterial);

    for (const Tank::Physics::MapPrimitive& primitive : mapPrimitives)
    {
        const size_t frictionBand = primitive.friction < 0.45f ? 0 :
            (primitive.friction < 0.8f ? 1 : 2);
        Engine::SceneMeshId meshId = 0;
        XMMATRIX localScale = XMMatrixScaling(
            primitive.size.x,
            primitive.size.y,
            primitive.size.z);
        if (primitive.type == Tank::Physics::MapPrimitiveType::TriangularPrism)
        {
            meshId = triangularPrismMesh;
        }
        else if (primitive.type == Tank::Physics::MapPrimitiveType::HeightField)
        {
            meshId = AddHeightFieldMesh(m_sceneBuilder, primitive);
            localScale = XMMatrixIdentity();
        }
        m_sceneBuilder.AddInstance(
            meshId,
            localScale *
            XMMatrixRotationY(primitive.yawRadians) *
            XMMatrixTranslation(
                primitive.position.x,
                primitive.position.y,
                primitive.position.z),
            frictionMaterials[frictionBand]);
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
    bool physicsDebugOverlay,
    bool showDummyModel,
    bool showDummyWheels,
    bool showGltfBody,
    bool showGltfCannon,
    bool showGltfSide)
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
        { m_model.forwardMarker,
            XMMatrixScaling(0.3f, 0.3f, 0.3f) *
                XMMatrixTranslation(0.0f, 0.0f, 0.625f * chassisLength) },
    };
    for (const Part& part : parts)
    {
        Engine::InstanceData& inst = scene.instances[part.index];
        inst.prevWorld = inst.world;
        const XMMATRIX world = showDummyModel
            ? part.localTransform * bodyTransform
            : XMMatrixScaling(0.0f, 0.0f, 0.0f);
        XMStoreFloat4x4(&inst.world, XMMatrixTranspose(world));
    }

    const struct { size_t index; bool available; bool visible; } gltfParts[] = {
        { m_model.gltfBody, m_model.hasGltfBody, showGltfBody },
        { m_model.gltfCannon, m_model.hasGltfCannon, showGltfCannon },
        { m_model.gltfSide, m_model.hasGltfSide, showGltfSide },
    };
    for (const auto& part : gltfParts)
    {
        if (part.available)
        {
            SetInstanceWorld(
                scene.instances[part.index],
                part.visible
                    ? XMMatrixScaling(
                        visualSettings.gltfModelScale,
                        visualSettings.gltfModelScale,
                        visualSettings.gltfModelScale) * bodyTransform
                    : XMMatrixScaling(0.0f, 0.0f, 0.0f));
        }
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

        if (showDummyWheels && i < state.wheelCount)
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

    m_physicsDebugOverlay.UpdateScene(
        state, tankSettings, showTrackProxies, physicsDebugOverlay);
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

const TrackedVehicleScenePresenter::TrackedVehicleModel&
TrackedVehicleScenePresenter::Model() const
{
    return m_model;
}
