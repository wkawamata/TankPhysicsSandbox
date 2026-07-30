#pragma once

#include "Physics/BoxDropTest.h"
#include "Rendering/BoxDropScenePresenter.h"

#include <cstdint>

namespace Engine { struct CameraState; class Scene; }
namespace RtPbrSurvey { class SceneRenderer; }

class BoxDropMode
{
public:
    void Enter(RtPbrSurvey::SceneRenderer& renderer);
    void Update(RtPbrSurvey::SceneRenderer& renderer);
    void DrawUi(RtPbrSurvey::SceneRenderer& renderer, float cpuFrameTimeMs);
    void Reset(RtPbrSurvey::SceneRenderer& renderer);
    void Exit();
    Engine::CameraState* ActiveCamera();
    Engine::Scene& GetScene();

    static constexpr float kPhysicsFixedDt = 1.0f / 60.0f;

private:
    BoxDropScenePresenter m_presenter;
    Tank::Physics::BoxDropTest m_test;
};
