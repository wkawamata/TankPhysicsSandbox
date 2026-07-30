#pragma once

#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/PhysicsDebugOverlay.h"
#include "Rendering/TankVisualSettings.h"
#include "Scene/SceneBuilder.h"

#include <array>
#include <cstddef>
#include <cstdint>

class TrackedVehicleScenePresenter
{
public:
    struct TrackedVehicleModel
    {
        static constexpr int kTrackShoeCountPerTrack = 32;

        size_t hullUpper = 0;
        size_t hullLower = 0;
        size_t upperStructureUpper = 0;
        size_t upperStructureLower = 0;
        size_t lowerStructureUpper = 0;
        size_t lowerStructureLower = 0;
        size_t forwardMarker = 0;
        std::array<size_t, Tank::Physics::kTankWheelCount> wheels = {};
        std::array<std::array<size_t, kTrackShoeCountPerTrack>, Tank::Physics::kTankTrackCount>
            trackShoes = {};
        uint32_t wheelMaterial = 0;
        uint32_t contactedWheelMaterial = 0;
        uint32_t trackShoeMaterial = 0;
        uint32_t trackProxyMaterial = 0;
        uint32_t forwardMarkerMaterial = 0;
        uint32_t hullUpperMaterial = 0;
        uint32_t hullLowerMaterial = 0;
        uint32_t structureUpperMaterial = 0;
        uint32_t structureLowerMaterial = 0;
    };

    void BuildScene(
        const Tank::Physics::PhysicsEnvironmentSettings& envSettings,
        const Tank::Rendering::TankVisualSettings& visualSettings,
        const Tank::Physics::TankSettings& tankSettings);

    void UpdateScene(
        const Tank::Physics::TrackedVehicleTestState& state,
        const Tank::Physics::TankSettings& tankSettings,
        const Tank::Rendering::TankVisualSettings& visualSettings,
        bool showTrackShoes,
        bool showTrackProxies,
        bool physicsDebugOverlay);

    void ApplyMaterials(
        const Tank::Rendering::TankVisualSettings& visualSettings);

    void Clear();
    Engine::SceneBuilder& SceneBuilder();
    Engine::Scene& GetScene();
    const Engine::Scene& GetScene() const;
    TrackedVehicleModel& Model();

private:
    Engine::SceneBuilder m_sceneBuilder;
    PhysicsDebugOverlay m_physicsDebugOverlay{m_sceneBuilder};
    TrackedVehicleModel m_model;
    std::array<float, Tank::Physics::kTankTrackCount> m_trackShoeDistances = {};
    float m_trackShoeLastTimeSeconds = 0.0f;
};
