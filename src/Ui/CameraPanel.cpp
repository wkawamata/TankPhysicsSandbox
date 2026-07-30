#include "stdafx.h"
#include "Ui/CameraPanel.h"

#include "App/CameraController.h"
#include "App/CameraSettingsStore.h"
#include "Physics/TrackedVehicleTest.h"
#include "imgui.h"
#include <ImGuiWidgets.h>
#include <Scene/Scene.h>

#include <DirectXMath.h>

#include <string>

namespace Ui
{
    void DrawCameraPanel(CameraPanelContext& ctx)
    {
        if (ctx.camera == nullptr)
        {
            return;
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 190.0f), ImVec2(1000.0f, 1000.0f));
        ImGui::Begin("Camera");
        ImGui::SeparatorText("Save Slot");
        for (int slot = 0; slot < 3; ++slot)
        {
            if (slot > 0)
            {
                ImGui::SameLine();
            }
            const std::string label = std::to_string(slot + 1);
            if (ImGui::RadioButton(
                label.c_str(),
                ctx.cameraController->SelectedSlot() == slot))
            {
                if (ctx.cameraController->SelectSlot(
                        slot,
                        ctx.cameraController->AutoLoad()) &&
                    ctx.loadCamera)
                {
                    ctx.loadCamera();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Debug", ctx.cameraController->SelectedSlot() == 3))
        {
            if (ctx.cameraController->SelectSlot(
                    3,
                    ctx.cameraController->AutoLoad()) &&
                ctx.loadCamera)
            {
                ctx.loadCamera();
            }
        }
        ImGui::SameLine();
        {
            bool autoLoad = ctx.cameraController->AutoLoad();
            if (ImGui::Checkbox("AutoLoad", &autoLoad))
            {
                ctx.cameraController->SetAutoLoad(autoLoad);
            }
        }
        if (ImGui::Button("Save Camera"))
        {
            if (ctx.saveCamera) ctx.saveCamera();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Camera"))
        {
            if (ctx.loadCamera) ctx.loadCamera();
        }
        if (!ctx.cameraController->Status().empty())
        {
            ImGui::TextWrapped("%s", ctx.cameraController->Status().c_str());
        }
        if (ctx.trackedVehicleActive)
        {
            bool follow = ctx.cameraController->FollowEnabled();
            if (ImGui::Checkbox("Follow Tank", &follow))
            {
                ctx.cameraController->SetFollowEnabled(follow);
                ctx.cameraController->ResetFollowState();
                if (!follow)
                {
                    const Tank::Physics::TrackedVehicleTestState& state = *ctx.vehicleState;
                    if (ctx.activateOrbitCamera && ctx.vehicleScene)
                    {
                        ctx.activateOrbitCamera(
                            *ctx.vehicleScene,
                            DirectX::XMFLOAT3(
                                state.bodyPosition.x,
                                state.bodyPosition.y + 0.5f,
                                state.bodyPosition.z));
                    }
                }
            }
            ImGui::BeginDisabled(!ctx.cameraController->FollowEnabled());
            {
                float dist = ctx.cameraController->FollowDistance();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Follow Distance", &dist, 4.0f, 250.0f, 0.5f, 16.0f))
                {
                    ctx.cameraController->SetFollowDistance(dist);
                }
            }
            {
                float val = ctx.cameraController->LookDownDegrees();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Look Down Angle", &val, 0.0f, 89.0f, 1.0f, 25.0f, "%.1f deg"))
                {
                    ctx.cameraController->SetLookDownDegrees(val);
                }
            }
            {
                float val = ctx.cameraController->PositionSpeed();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Position Speed", &val, 0.5f, 20.0f, 0.5f, 5.0f))
                {
                    ctx.cameraController->SetPositionSpeed(val);
                }
            }
            {
                float val = ctx.cameraController->RotationSpeed();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Rotation Speed", &val, 0.5f, 20.0f, 0.5f, 8.0f))
                {
                    ctx.cameraController->SetRotationSpeed(val);
                }
            }
            {
                float val = ctx.cameraController->YawSpeedLimitDegrees();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Yaw Speed Limit", &val, 15.0f, 720.0f, 15.0f, 180.0f, "%.0f deg/s"))
                {
                    ctx.cameraController->SetYawSpeedLimitDegrees(val);
                }
            }
            {
                float val = ctx.cameraController->YawDamping();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Yaw Damping", &val, 0.5f, 30.0f, 0.5f, 8.0f))
                {
                    ctx.cameraController->SetYawDamping(val);
                }
            }
            {
                float val = ctx.cameraController->Damping();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Damping", &val, 0.1f, 2.0f, 0.05f, 1.0f))
                {
                    ctx.cameraController->SetDamping(val);
                }
            }
            ImGui::EndDisabled();
            ImGui::BeginDisabled(ctx.cameraController->FollowEnabled());
            ImGui::SeparatorText("Angle");
            if (ImGui::Button("Rear High"))
            {
                const Tank::Physics::TrackedVehicleTestState& state = *ctx.vehicleState;
                ctx.cameraController->ApplyCameraPreset(
                    { 0.0f, 9.2f, -16.0f }, state,
                    ctx.vehicleScene->camera);
                if (ctx.activateOrbitCamera && ctx.vehicleScene)
                {
                    ctx.activateOrbitCamera(
                        *ctx.vehicleScene,
                        { state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Rear Quarter"))
            {
                const Tank::Physics::TrackedVehicleTestState& state = *ctx.vehicleState;
                ctx.cameraController->ApplyCameraPreset(
                    { 10.0f, 7.0f, -14.0f }, state,
                    ctx.vehicleScene->camera);
                if (ctx.activateOrbitCamera && ctx.vehicleScene)
                {
                    ctx.activateOrbitCamera(
                        *ctx.vehicleScene,
                        { state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
                }
            }
            if (ImGui::Button("Side High"))
            {
                const Tank::Physics::TrackedVehicleTestState& state = *ctx.vehicleState;
                ctx.cameraController->ApplyCameraPreset(
                    { 16.0f, 6.0f, 0.0f }, state,
                    ctx.vehicleScene->camera);
                if (ctx.activateOrbitCamera && ctx.vehicleScene)
                {
                    ctx.activateOrbitCamera(
                        *ctx.vehicleScene,
                        { state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Top Rear"))
            {
                const Tank::Physics::TrackedVehicleTestState& state = *ctx.vehicleState;
                ctx.cameraController->ApplyCameraPreset(
                    { 0.0f, 18.0f, -4.0f }, state,
                    ctx.vehicleScene->camera);
                if (ctx.activateOrbitCamera && ctx.vehicleScene)
                {
                    ctx.activateOrbitCamera(
                        *ctx.vehicleScene,
                        { state.bodyPosition.x, state.bodyPosition.y + 0.5f, state.bodyPosition.z });
                }
            }
            ImGui::EndDisabled();
            ImGui::SeparatorText("Projection");
        }
        int projection = static_cast<int>(ctx.camera->projection);
        bool changed = false;
        changed |= ImGui::RadioButton(
            "Perspective", &projection, static_cast<int>(Engine::CameraProjection::Perspective));
        ImGui::SameLine();
        changed |= ImGui::RadioButton(
            "Orthographic", &projection, static_cast<int>(Engine::CameraProjection::Orthographic));
        ctx.camera->projection = static_cast<Engine::CameraProjection>(projection);

        if (ctx.camera->projection == Engine::CameraProjection::Perspective)
        {
            const bool fovChanged =
                ImGui::SliderFloat("FOV Y", &ctx.camera->fov, 20.0f, 120.0f, "%.1f deg");
            changed |= fovChanged;
            if (fovChanged)
            {
                ctx.cameraController->ResetFollowState();
            }
        }
        else
        {
            changed |= ImGui::SliderFloat(
                "Ortho Height", &ctx.camera->orthographicHeight, 1.0f, 50.0f, "%.1f");
        }

        if (changed && ctx.setCamera)
        {
            ctx.setCamera(*ctx.camera);
        }
        ImGui::End();
    }
}
