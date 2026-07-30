#pragma once

#include "Physics/BoxDropTest.h"
#include "Scene/SceneBuilder.h"

#include <cstddef>

class BoxDropScenePresenter
{
public:
    void BuildScene();
    void UpdateScene(const Tank::Physics::BoxDropState& state);
    void Clear();
    Engine::SceneBuilder& SceneBuilder();
    Engine::Scene& GetScene();
    const Engine::Scene& GetScene() const;

private:
    Engine::SceneBuilder m_sceneBuilder;
    size_t m_boxInstanceIndex = 0;
};
