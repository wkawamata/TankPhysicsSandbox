#include "Rendering/TrackedVehicleScenePresenter.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        const auto check = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };
        TrackedVehicleScenePresenter presenter;
        Tank::Physics::TankSettings tank;
        Tank::Rendering::TankVisualSettings visual;
        presenter.BuildScene({}, {}, visual, tank, nullptr, false);
        Tank::Physics::TrackedVehicleTestState state;
        state.bodyRotation.w = 1;
        state.bodyPosition = {3, 2, 4};
        Tank::Physics::FixedTurretState enemy;
        enemy.target.active = true;
        enemy.simpleMoving = true;
        enemy.rotation.w = 1;
        enemy.position = {10, 2, 20};
        state.fixedTurrets.push_back(enemy);
        enemy.position = {-10, 2, 20};
        state.fixedTurrets.push_back(enemy);
        const auto start = presenter.GetScene().instances.size();
        presenter.AppendDestructibleBoxes(state);
        bool showShoes = true, showWheels = true;
        const auto update = [&] { presenter.UpdateScene(state, tank, visual, showShoes, false, false, true, showWheels, false, false, false); };
        update();
        const auto& scene = presenter.GetScene();
        const auto source = presenter.Model().hullUpper;
        // Each enemy has a proxy and direction marker, then the cloned tank parts.
        const auto first = start + 2;
        const auto second = first + 7 + Tank::Physics::kTankWheelCount +
            Tank::Physics::kTankTrackCount * TrackedVehicleScenePresenter::TrackedVehicleModel::kTrackShoeCountPerTrack + 2;
        check(scene.instances[first].meshId == scene.instances[source].meshId &&
            scene.instances[first].materialId == scene.instances[source].materialId, "Tank shares mesh and material");
        check(std::abs(scene.instances[first].world._14 - 10) < 0.001f &&
            std::abs(scene.instances[second].world._14 + 10) < 0.001f, "Enemy transforms are independent");
        check(scene.instances[start].world._11 == 0, "Moving tank hides proxy cube");
        state.bodyPosition.x = 100;
        state.fixedTurrets[0].position.z = 30;
        update();
        check(std::abs(scene.instances[first].world._14 - 10) < 0.001f &&
            std::abs(scene.instances[first].world._34 - 30) < 0.001f, "Player translation does not affect enemy");
        const auto firstWheel = first+7;
        const auto firstShoe = firstWheel+Tank::Physics::kTankWheelCount;
        const auto secondShoe = second+7+Tank::Physics::kTankWheelCount;
        auto& moving = state.fixedTurrets[0];
        moving.physicalMoving = true;
        moving.runningGear.emplace();
        moving.runningGear->wheelCount = 1;
        moving.runningGear->wheels[0].transform.position = {11, 2.5f, 30};
        moving.runningGear->wheels[0].hasContact = true;
        visual.colorWheelsByContact = true;
        moving.runningGear->trackAngularVelocityRadians = {2, -2};
        state.wheelCount = 1;
        state.wheels[0].transform.position = {1000, 1000, 1000};
        update();
        check(std::abs(scene.instances[firstWheel].world._14-11) < 0.001f &&
            std::abs(scene.instances[firstWheel].world._24-2.5f) < 0.001f,
            "Enemy wheel uses its own physical suspension snapshot");
        check(scene.instances[firstWheel].materialId == presenter.Model().contactedWheelMaterial &&
            scene.instances[second+7].materialId == presenter.Model().wheelMaterial,
            "Contact color comes from the individual enemy wheel");
        const auto before = scene.instances[firstShoe].world;
        const auto stationary = scene.instances[secondShoe].world;
        float standardTrackTop = 0;
        for (size_t i = 0; i < TrackedVehicleScenePresenter::TrackedVehicleModel::kTrackShoeCountPerTrack; ++i)
            standardTrackTop = std::max(standardTrackTop, scene.instances[secondShoe+i].world._24);
        check(standardTrackTop > 2.8f, "Simple track path wraps the standard upper wheels");
        state.timeSeconds += 0.05f;
        state.linearVelocity = {0, 0, 30};
        update();
        check(std::abs(scene.instances[firstShoe].world._24-before._24)+
            std::abs(scene.instances[firstShoe].world._34-before._34) > 0.001f,
            "Enemy track phase advances from its own track velocity");
        check(std::abs(scene.instances[secondShoe].world._24-stationary._24)+
            std::abs(scene.instances[secondShoe].world._34-stationary._34) < 0.001f,
            "Stationary enemy track does not inherit player or other enemy motion");
        const auto phase = scene.instances[firstShoe].world;
        update();
        check(std::abs(scene.instances[firstShoe].world._34-phase._34) < 0.001f,
            "Rendering a paused physics snapshot does not advance tracks");
        showShoes = showWheels = false;
        update();
        check(scene.instances[firstWheel].world._11 == 0 && scene.instances[firstShoe].world._11 == 0,
            "Running gear respects display toggles");
        showShoes = showWheels = true;
        moving.runningGear.reset();
        moving.physicalMoving = false;
        update();
        check(std::abs(scene.instances[firstWheel].world._14-11.2f) < 0.001f,
            "Simple movement restores enemy-local standard running gear");
        const auto simpleLeft = scene.instances[secondShoe].world;
        const auto secondRightShoe = secondShoe+TrackedVehicleScenePresenter::TrackedVehicleModel::kTrackShoeCountPerTrack;
        const auto simpleRight = scene.instances[secondRightShoe].world;
        state.fixedTurrets[1].navigationYawRadiansPerSecond = 1;
        state.timeSeconds += 0.05f;
        update();
        const auto leftDelta = scene.instances[secondShoe].world._34-simpleLeft._34;
        const auto rightDelta = scene.instances[secondRightShoe].world._34-simpleRight._34;
        check(leftDelta*rightDelta < 0, "Simple pivot drives the two displayed tracks in opposite directions");
        state.fixedTurrets[0].target.active = false;
        update();
        check(scene.instances[first].world._11 == 0 && scene.instances[second].world._11 != 0,
            "Destruction hides only the destroyed tank");
        check(scene.instances[firstWheel].world._11 == 0 && scene.instances[firstShoe].world._11 == 0,
            "Destruction hides wheels and track shoes");
        const auto shellInstance = scene.instances.size();
        check(presenter.EnsureMortarCapacity(64, 8), "Mortar reservation changes scene resources");
        check(!presenter.EnsureMortarCapacity(8, 8), "Reserved mortar capacity supports live count changes");
        Tank::Physics::MortarProjectileState shell;
        shell.position = {7, 12, 20};
        shell.velocity = {0, 4, 9};
        state.mortarProjectiles.push_back(shell);
        update();
        check(std::abs(scene.instances[shellInstance].world._14-7) < 0.001f &&
            std::abs(scene.instances[shellInstance].world._24-12) < 0.001f,
            "MyTank mortar shell is placed at its ballistic position");
        state.mortarProjectiles.clear();
        update();
        check(scene.instances[shellInstance].world._11 == 0,
            "Expired mortar shell is hidden");
        Engine::InstanceData visibility {};
        DirectX::XMStoreFloat4x4(&visibility.world, DirectX::XMMatrixTranspose(
            DirectX::XMMatrixScaling(0, 0, 0) * DirectX::XMMatrixTranslation(10, 20, 30)));
        check(!Engine::HasRenderableTransform(visibility), "Collapsed translated instances skip drawing");
        DirectX::XMStoreFloat4x4(&visibility.world, DirectX::XMMatrixTranspose(
            DirectX::XMMatrixRotationY(DirectX::XM_PIDIV2)));
        check(Engine::HasRenderableTransform(visibility), "Rotated visible instances retain drawing");
        DirectX::XMStoreFloat4x4(&visibility.world, DirectX::XMMatrixScaling(1, 0, 1));
        check(Engine::HasRenderableTransform(visibility), "Flat visible geometry retains drawing");
        presenter.Clear();
        check(presenter.GetScene().instances.empty(), "Clear removes enemy instances");
        std::cout << "PASS EnemyTankVisual\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}