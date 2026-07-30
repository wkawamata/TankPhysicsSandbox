#include "App/BoxDropMode.h"
#include "Runtime/SceneRenderer.h"

#include "imgui.h"

using namespace DirectX;

void BoxDropMode::Enter(RtPbrSurvey::SceneRenderer& renderer)
{
    m_presenter.BuildScene();
    Engine::Scene& scene = m_presenter.GetScene();

    m_test.Initialize();

    renderer.SetScene(scene);
    renderer.ReloadSceneResources(scene);
    renderer.SetDisplayInstanceCount(static_cast<int>(scene.instances.size()));
}

void BoxDropMode::Update(RtPbrSurvey::SceneRenderer& renderer)
{
    const Tank::Physics::BoxDropState state = m_test.Step(kPhysicsFixedDt);
    m_presenter.UpdateScene(state);
    renderer.SetScene(m_presenter.GetScene());
}

void BoxDropMode::DrawUi(RtPbrSurvey::SceneRenderer& renderer, float cpuFrameTimeMs)
{
    const Tank::Physics::BoxDropState& state = m_test.State();
    ImGui::Begin("Box Drop");
    ImGui::Text("Step: %d", state.stepIndex);
    ImGui::Text("Time: %.2f s", state.timeSeconds);
    ImGui::Text("Box Y: %.3f", state.boxPosition.y);
    ImGui::Text("Sleeping: %s", state.boxSleeping ? "yes" : "no");
    ImGui::Separator();
    ImGui::Text("Frame: %.1f ms", cpuFrameTimeMs);
    if (ImGui::Button("Reset"))
    {
        Reset(renderer);
    }
    ImGui::Separator();
    ImGui::Text("Press ESC to return to the top menu.");
    ImGui::End();
}

void BoxDropMode::Reset(RtPbrSurvey::SceneRenderer& renderer)
{
    m_test.Initialize();
    m_presenter.UpdateScene(m_test.State());
    renderer.SetScene(m_presenter.GetScene());
}

void BoxDropMode::Exit()
{
    m_presenter.Clear();
}

Engine::CameraState* BoxDropMode::ActiveCamera()
{
    return &m_presenter.GetScene().camera;
}

Engine::Scene& BoxDropMode::GetScene()
{
    return m_presenter.GetScene();
}
