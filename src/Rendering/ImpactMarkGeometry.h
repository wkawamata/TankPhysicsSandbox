#pragma once

#include "Physics/ImpactMarkBuffer.h"
#include "Scene/SceneBuilder.h"
#include <DirectXMath.h>
#include <cmath>

namespace Tank::Rendering::ImpactMarkGeometry
{
    inline Engine::SceneMeshId AppendQuad(Engine::SceneBuilder& builder)
    {
        auto& mesh = builder.GetMesh();
        Engine::SceneMesh::Range range;
        range.firstVertex = static_cast<uint32_t>(mesh.vertices.size());
        range.firstIndex = static_cast<uint32_t>(mesh.indices.size());
        // Local XY plane, front normal +Z. Same winding as RtPbrSurvey's +Z cube face:
        // this becomes CW in D3D viewport coordinates when seen from the +Z side.
        mesh.vertices.push_back({{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});
        mesh.vertices.push_back({{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}});
        mesh.vertices.push_back({{ 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
        mesh.vertices.push_back({{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}});
        const uint32_t base = range.firstVertex;
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        range.vertexCount = 4;
        range.indexCount = 6;
        mesh.ranges.push_back(range);
        return static_cast<Engine::SceneMeshId>(mesh.ranges.size() - 1);
    }

    inline DirectX::XMMATRIX World(const Physics::ImpactMark& mark)
    {
        using namespace DirectX;
        XMVECTOR normal = XMVectorSet(mark.normal.x, mark.normal.y, mark.normal.z, 0.0f);
        if (XMVectorGetX(XMVector3LengthSq(normal)) < 1.0e-8f) normal = XMVectorSet(0, 1, 0, 0);
        normal = XMVector3Normalize(normal);
        const XMVECTOR seed = std::abs(XMVectorGetY(normal)) < 0.9f
            ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
        const XMVECTOR tangent = XMVector3Normalize(XMVector3Cross(seed, normal));
        const XMVECTOR bitangent = XMVector3Cross(normal, tangent);
        // An orthonormal, positive-determinant basis keeps the CW front intact.
        const XMVECTOR position = XMVectorSet(mark.position.x, mark.position.y, mark.position.z, 1.0f)
            + normal * 0.005f;
        const XMMATRIX basis(tangent, bitangent, normal, position);
        return XMMatrixScaling(0.35f, 0.35f, 1.0f) * basis;
    }
}
