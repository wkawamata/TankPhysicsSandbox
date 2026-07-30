#include "BoxDropScenePresenter.h"

#include <DirectXMath.h>
#include <DirectXMathConvert.inl>
#include <DirectXMathMatrix.inl>
#include <DirectXMathVector.inl>
#include <GltfLoader.h>

using namespace DirectX;

void BoxDropScenePresenter::BuildScene()
{
    m_sceneBuilder.Clear();

    const uint32_t matFloor = m_sceneBuilder.AddSolidColorMaterial(128, 128, 128, 255);
    const uint32_t matBox = m_sceneBuilder.AddSolidColorMaterial(200, 50, 50, 255);

    m_sceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);
    m_sceneBuilder.AppendCube(1.0f, kGltfVertexMaterialFromInstance);

    m_sceneBuilder.AddInstance(
        XMMatrixScaling(20.0f, 0.2f, 20.0f) * XMMatrixTranslation(0.0f, -0.1f, 0.0f),
        matFloor);

    m_boxInstanceIndex = 1;
    m_sceneBuilder.AddInstance(
        XMMatrixTranslation(0.0f, 5.0f, 0.0f),
        matBox);

    Engine::CameraState camera;
    camera.pos = { 0.0f, 4.0f, -12.0f };
    camera.gazePoint = { 0.0f, 1.0f, 0.0f };
    camera.fov = 60.0f;
    camera.nearZ = 0.1f;
    camera.farZ = 10000.0f;
    m_sceneBuilder.SetCamera(camera);
}

void BoxDropScenePresenter::UpdateScene(const Tank::Physics::BoxDropState& state)
{
    Engine::Scene& scene = m_sceneBuilder.GetScene();
    scene.instances[m_boxInstanceIndex].prevWorld = scene.instances[m_boxInstanceIndex].world;
    const XMMATRIX boxWorld = XMMatrixTranslation(
        state.boxPosition.x,
        state.boxPosition.y,
        state.boxPosition.z);
    XMStoreFloat4x4(&scene.instances[m_boxInstanceIndex].world, XMMatrixTranspose(boxWorld));
}

void BoxDropScenePresenter::Clear()
{
    m_sceneBuilder.Clear();
}

Engine::SceneBuilder& BoxDropScenePresenter::SceneBuilder()
{
    return m_sceneBuilder;
}

Engine::Scene& BoxDropScenePresenter::GetScene()
{
    return m_sceneBuilder.GetScene();
}

const Engine::Scene& BoxDropScenePresenter::GetScene() const
{
    return m_sceneBuilder.GetScene();
}
