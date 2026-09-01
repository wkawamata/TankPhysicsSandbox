#pragma once

#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "MortarRangeCue.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Engine { class SceneBuilder; }

class PhysicsDebugOverlay
{
public:
    explicit PhysicsDebugOverlay(Engine::SceneBuilder& sceneBuilder);

    void BuildScene(uint32_t trackProxyMaterial);

    void UpdateScene(
        const Tank::Physics::TrackedVehicleTestState& state,
        const Tank::Physics::TankSettings& tankSettings,
        bool showTrackProxies,
        bool visible);

    void Clear();
    void SetMortarRangeCue(const Tank::Rendering::MortarRangeCue& cue)
    {
        m_mortarRangeCue = cue;
    }
    const Tank::Rendering::MortarRangeCue& MortarRangeCue() const
    {
        return m_mortarRangeCue;
    }

private:
    Engine::SceneBuilder& m_sceneBuilder;

    size_t m_leftTrack = 0;
    size_t m_rightTrack = 0;
    std::array<size_t, Tank::Physics::kTankWheelCount> m_suspensionLines = {};
    std::array<size_t, Tank::Physics::kTankWheelCount> m_contactMarkers = {};
    std::array<size_t, Tank::Physics::kTankWheelCount> m_contactNormalLines = {};

    uint32_t m_trackProxyMaterial = 0;
    uint32_t m_debugContactMaterial = 0;
    uint32_t m_debugAirborneMaterial = 0;
    Tank::Rendering::MortarRangeCue m_mortarRangeCue = {};
};
