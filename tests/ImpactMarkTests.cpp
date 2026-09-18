#include "Physics/TrackedVehicleTest.h"
#include "Rendering/ImpactMarkGeometry.h"
#include <cmath>
#include <iostream>

using namespace DirectX;

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition) std::cerr << "FAIL Impact marks: " << message << '\n';
        return condition;
    }

    float ScreenArea(const Engine::SceneMesh& mesh, uint32_t firstIndex, FXMMATRIX transform)
    {
        XMFLOAT3 points[3];
        for (uint32_t i = 0; i < 3; ++i)
        {
            const auto& p = mesh.vertices[mesh.indices[firstIndex + i]].position;
            XMStoreFloat3(&points[i], XMVector3TransformCoord(XMLoadFloat3(&p), transform));
            points[i].y = -points[i].y; // D3D viewport Y points down.
        }
        return (points[1].x - points[0].x) * (points[2].y - points[0].y) -
            (points[1].y - points[0].y) * (points[2].x - points[0].x);
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;
    ImpactMarkBuffer ring;
    ring.Add({}, {0, 1, 0});
    passed &= Check(ring.Count() == 0, "zero capacity ignores hits");
    ring.SetCapacity(3);
    for (int i = 1; i <= 5; ++i) ring.Add({static_cast<float>(i), 0, 0}, {0, 1, 0});
    passed &= Check(ring.Count() == 3 && ring.Slots()[0].sequence == 4 &&
        ring.Slots()[1].sequence == 5 && ring.Slots()[2].sequence == 3, "ring wraps and replaces oldest slot");
    ring.SetCapacity(2);
    passed &= Check(ring.Count() == 2 && ring.Slots()[0].sequence == 4 && ring.Slots()[1].sequence == 5,
        "shrinking keeps newest marks");
    ring.Add({}, {0, 1, 0});
    passed &= Check(ring.Slots()[0].sequence == 6, "shrunk ring overwrites oldest");
    ring.SetCapacity(4);
    for (int i = 0; i < 3; ++i) ring.Add({}, {0, 1, 0});
    passed &= Check(ring.Count() == 4 && ring.Slots()[0].sequence == 9 && ring.Slots()[1].sequence == 6,
        "growing preserves chronology and ring cursor");
    ring.SetCapacity(0);
    passed &= Check(ring.Count() == 0 && ring.Slots().empty(), "disabling clears marks");

    Engine::SceneBuilder builder;
    builder.AddCube(1.0f); // Exercise absolute vertex/index offsets.
    const auto meshId = Tank::Rendering::ImpactMarkGeometry::AppendQuad(builder);
    const auto& mesh = builder.GetMesh();
    const auto& range = mesh.ranges[meshId];
    passed &= Check(range.vertexCount == 4 && range.indexCount == 6, "mark is a one-sided XY quad");
    const Vec3 normals[] = {{0, 1, 0}, {0.3f, 0.8f, -0.4f}, {0, 0, -1}, {0, -1, 0}, {1, 0, 0}};
    for (const auto& normal : normals)
    {
        ImpactMark mark{{2, 3, 4}, normal, 1};
        const auto world = Tank::Rendering::ImpactMarkGeometry::World(mark);
        const auto n = XMVector3Normalize(XMVectorSet(normal.x, normal.y, normal.z, 0));
        const auto center = XMVector3TransformCoord(XMVectorZero(), world);
        const auto up = std::abs(XMVectorGetY(n)) > 0.9f ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
        const auto projection = XMMatrixPerspectiveFovLH(XM_PI / 3.0f, 1, 0.1f, 10);
        const auto front = world * XMMatrixLookAtLH(center + n * 3, center, up) * projection;
        const auto back = world * XMMatrixLookAtLH(center - n * 3, center, up) * projection;
        for (uint32_t triangle = 0; triangle < 2; ++triangle)
        {
            const uint32_t first = range.firstIndex + triangle * 3;
            passed &= Check(ScreenArea(mesh, first, front) > 0, "front is CW in D3D screen coordinates");
            passed &= Check(ScreenArea(mesh, first, back) < 0, "back is CCW and must be culled");
        }
        const auto transformedNormal = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), world));
        passed &= Check(XMVectorGetX(XMVector3Dot(transformedNormal, n)) > 0.999f &&
            XMVectorGetX(XMMatrixDeterminant(world)) > 0, "local +Z maps to surface normal without mirroring");
        for (uint32_t i = 0; i < 4; ++i)
        {
            const auto& vertex = mesh.vertices[range.firstVertex + i];
            const auto p = XMVector3TransformCoord(XMLoadFloat3(&vertex.position), world);
            const auto offset = p - XMVectorSet(2, 3, 4, 1);
            passed &= Check(std::abs(XMVectorGetX(XMVector3Dot(offset, n)) - 0.005f) < 0.0001f,
                "polygon is parallel to hit plane and offset toward normal");
        }
    }

    TrackedVehicleTest test;
    MapPrimitive slope;
    slope.type = MapPrimitiveType::TriangularPrism;
    slope.position = {0, 0, 15};
    slope.size = {6, 6, 10};
    test.Initialize({}, {}, {slope});
    for (int i = 0; i < 180; ++i) test.Step(1.0f / 60);
    auto settings = test.ProjectileSettings();
    settings.maximumImpactMarks = 2;
    test.SetAssaultProjectileSettings(settings);
    for (int round = 0; round < 5; ++round)
    {
        test.FireAssault();
        for (int i = 0; i < 20; ++i) test.Step(1.0f / 60);
    }
    const auto& marks = test.State().assaultImpactMarks;
    passed &= Check(marks.Count() == 2 && marks.Slots()[0].sequence == 5 && marks.Slots()[1].sequence == 4,
        "actual Jolt slope hits feed bounded ring");
    passed &= Check(marks.Slots()[0].normal.y > 0.8f && marks.Slots()[0].normal.z < -0.4f &&
        marks.Slots()[0].position.z > 16, "slope hit stores actual point and world surface normal");
    settings.maximumImpactMarks = 0;
    test.SetAssaultProjectileSettings(settings);
    test.FireAssault();
    for (int i = 0; i < 20; ++i) test.Step(1.0f / 60);
    passed &= Check(test.State().assaultImpactMarks.Count() == 0, "zero limit disables world marks");
    test.Initialize();
    passed &= Check(test.State().assaultImpactMarks.Count() == 0, "reset clears world marks");
    if (!passed) return 1;
    std::cout << "PASS ImpactMark / ring, normal and CW front\n";
    return 0;
}
