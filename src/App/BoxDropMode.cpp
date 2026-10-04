#include "App/BoxDropMode.h"
#include "Runtime/SceneRenderer.h"

#include "imgui.h"

using namespace DirectX;

void BoxDropMode::Enter(RtPbrSurvey::SceneRenderer& renderer)
{
    m_presenter.BuildScene();
    Engine::Scene& scene = m_presenter.GetScene();

    m_playback.Reset();
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
    if (ImGui::Button("Reset (F7)"))
    {
        Reset(renderer);
    }
    ImGui::SameLine();
    if (ImGui::Button(m_playback.IsPaused() ? "Resume (P)" : "Pause (P)"))
    {
        m_playback.TogglePaused();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!m_playback.IsPaused());
    if (ImGui::Button("Step (N)"))
    {
        m_playback.RequestSingleStep();
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::Text("F7: Reset  |  P: Pause/Resume  |  N: Step  |  ESC: Top menu");
    ImGui::End();
}

void BoxDropMode::Reset(RtPbrSurvey::SceneRenderer& renderer)
{
    m_test.Initialize();
    m_presenter.UpdateScene(m_test.State());
    renderer.SetScene(m_presenter.GetScene());
}

void BoxDropMode::TogglePaused()
{
    m_playback.TogglePaused();
}

bool BoxDropMode::Paused() const
{
    return m_playback.IsPaused();
}

void BoxDropMode::RequestSingleStep()
{
    m_playback.RequestSingleStep();
}

bool BoxDropMode::ConsumeSimulationStep()
{
    return m_playback.ConsumeSimulationStep();
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
