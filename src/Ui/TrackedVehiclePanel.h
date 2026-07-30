#pragma once

#include "Input/GamepadState.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/TankTypes.h"
#include "Rendering/TankVisualSettings.h"

#include <functional>
#include <string>

namespace Tank::Physics
{
    struct TrackedDriverInput;
    struct TrackedVehicleTestState;
}

namespace Ui
{
    struct TrackedVehiclePanelContext
    {
        const Tank::Physics::TrackedVehicleTestState* state = nullptr;
        const Tank::Physics::TrackedDriverInput* driverInput = nullptr;
        Tank::Input::GamepadState gamepadState = {};
        bool gamepadAvailable = false;
        float cpuFrameTimeMs = 0.0f;
        float analogLeftTrack = 0.0f;
        float analogRightTrack = 0.0f;
        float analogRoll = 0.0f;
        bool analogTracksConnected = false;

        bool* physicsDebugOverlay = nullptr;
        bool* trackShoeDisplay = nullptr;
        bool* showTrackProxies = nullptr;
        bool* trackedVehiclePaused = nullptr;
        bool* trackedVehicleSingleStep = nullptr;
        int* tankSettingsSlot = nullptr;
        bool* tankSettingsAutoLoad = nullptr;
        bool* tankVisualSettingsAutoLoad = nullptr;
        bool* tankVisualMaterialApplyPending = nullptr;
        std::string* tankSettingsStatus = nullptr;
        std::string* tankVisualSettingsStatus = nullptr;
        std::string* envSettingsStatus = nullptr;
        std::string* tankModelExportStatus = nullptr;

        Tank::Physics::TankSettings* tankSettings = nullptr;
        const Tank::Physics::TankSettings* appliedTankSettings = nullptr;
        Tank::Physics::PhysicsEnvironmentSettings* envSettings = nullptr;
        const Tank::Physics::PhysicsEnvironmentSettings* appliedEnvSettings = nullptr;
        Tank::Rendering::TankVisualSettings* visualSettings = nullptr;

        std::function<void()> updateScene;
        std::function<void()> enterTrackedVehicleMode;
        std::function<void()> resetTrackedVehicle;
        std::function<void()> applyMaterials;
        std::function<void()> saveTankSettings;
        std::function<void()> loadTankSettings;
        std::function<void()> saveTankVisualSettings;
        std::function<void()> loadTankVisualSettings;
        std::function<void()> saveEnvSettings;
        std::function<void()> loadEnvSettings;
        std::function<void()> exportTankModel;
    };

    void DrawTrackedVehiclePanel(TrackedVehiclePanelContext& ctx);
}
