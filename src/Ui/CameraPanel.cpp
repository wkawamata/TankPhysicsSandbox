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
    namespace
    {
        void DrawMortarCameraCheatWindow(CameraPanelContext& ctx)
        {
            if (!ctx.mortarCheatWindowVisible) return;
            ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin("CheatWindow: Mortar Camera", &ctx.mortarCheatWindowVisible))
            {
                ImGui::End();
                return;
            }
            ImGui::SetWindowFontScale(ctx.mortarCheatFontScale);
            ImGui::SliderFloat("Text Scale##MortarCamera", &ctx.mortarCheatFontScale,
                0.5f, 2.0f, "%.2f x", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::Button(reinterpret_cast<const char*>(u8"日本語##MortarCameraLanguage")))
                ctx.mortarCheatWindowJapanese = true;
            ImGui::SameLine();
            if (ImGui::Button("English##MortarCameraLanguage"))
                ctx.mortarCheatWindowJapanese = false;
            const bool jp = ctx.mortarCheatWindowJapanese;
            auto explain = [jp](const char* english, const char8_t* japanese) {
                ImGui::TextWrapped("%s", jp ? reinterpret_cast<const char*>(japanese) : english);
            };
            explain("These settings control the follow camera during mortar elevation. Enable Follow Tank to see their effect. Values apply immediately; Reset Tank is not required.",
                u8"迫撃時の車体仰角に応じた追従カメラの変化を調整します。Follow TankをONにすると有効になります。変更は即時反映され、Reset Tankは不要です。");
            ImGui::Separator();
            ImGui::Text("Mortar Pitch Offset: %.1f deg", ctx.cameraController->MortarPitchOffsetDegrees());
            explain("Extra look-down angle at maximum mortar elevation. Positive values look down more; negative values reduce the angle. Default: 18 degrees. Range: -60 to +60 degrees. The total angle is limited by the normal camera look-down limit.",
                u8"最大仰角時に通常の俯角へ追加する角度です。正値でより見下ろし、負値で俯角を減らします。初期値18度、設定範囲-60〜+60度。合計角度は通常カメラの俯角上限に制限されます。");
            ImGui::Spacing();
            ImGui::Text("Mortar Distance Offset: %.1f m", ctx.cameraController->MortarDistanceOffsetMeters());
            explain("Extra distance from the tank at maximum mortar elevation. Positive values move away; negative values move closer. Default: 0 m. Range: -100 to +100 m. Follow Distance plus this offset is limited to 4-250 m.",
                u8"最大仰角時にFollow Distanceへ追加する距離です。正値で遠ざかり、負値で近づきます。初期値0 m、設定範囲-100〜+100 m。合計距離は4〜250 mに制限されます。");
            ImGui::Spacing();
            ImGui::Text("Mortar Response Speed: %.1f /s", ctx.cameraController->MortarResponseSpeed());
            explain("How quickly the extra angle and distance approach their targets, including return after mortar release. Larger values respond faster. Default: 6 /s. Range: 0.1-30 /s. Normal Position Speed and Damping also affect the final camera motion.",
                u8"俯角と距離の追加量が目標値へ近づく速さです。迫撃解除後の復帰にも適用されます。大きいほど速く反応します。初期値6 /s、設定範囲0.1〜30 /s。最終的なカメラ移動には通常のPosition SpeedとDampingも影響します。");
            ImGui::Separator();
            explain("Offsets scale with mortar elevation progress: at 50% progress, the target offsets are half their configured values. After release, they return smoothly to zero as the stance lowers.",
                u8"追加量は迫撃の仰角進行度に比例します。進行度50%では設定値の半分が目標になります。解除後は車体の復帰に合わせて追加量が滑らかに0へ戻ります。");
            explain("Save Camera stores these settings in the selected Camera Slot. Load Camera restores them. They are camera settings, not Mortar Profile settings. Unsaved changes are lost when the app closes.",
                u8"Save Cameraで選択中のCamera Slotへ保存し、Load Cameraで復元します。保存先はCamera設定で、Mortar Profileではありません。未保存の変更はアプリ終了時に失われます。");
            ImGui::End();
        }
    }

    void DrawCameraPanel(CameraPanelContext& ctx)
    {
        if (ctx.camera == nullptr)
        {
            return;
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 190.0f), ImVec2(1000.0f, 1000.0f));
        ImGui::Begin("Camera");
        int mouseMode = static_cast<int>(ctx.cameraController->GetMouseControlMode());
        const char* mouseModes[] = { "Gameplay", "Alt Gesture", "Always Debug" };
        if (ImGui::Combo("Mouse Control", &mouseMode, mouseModes, 3))
        {
            ctx.cameraController->SetMouseControlMode(
                static_cast<Tank::App::CameraController::MouseControlMode>(mouseMode));
        }
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

        const float focusDx = ctx.camera->pos.x - ctx.camera->gazePoint.x;
        const float focusDy = ctx.camera->pos.y - ctx.camera->gazePoint.y;
        const float focusDz = ctx.camera->pos.z - ctx.camera->gazePoint.z;
        const float focusDistance = std::sqrt(
            focusDx * focusDx + focusDy * focusDy + focusDz * focusDz);
        const bool transitioning = ctx.cameraController->IsTransitioning();
        if (transitioning && !ctx.telemetryWasTransitioning)
        {
            ctx.telemetryMinimumPositionY = ctx.camera->pos.y;
            ctx.telemetryMinimumFocusDistance = focusDistance;
            ctx.telemetryHasSample = true;
        }
        else if (transitioning && ctx.telemetryHasSample)
        {
            ctx.telemetryMinimumPositionY =
                std::min(ctx.telemetryMinimumPositionY, ctx.camera->pos.y);
            ctx.telemetryMinimumFocusDistance =
                std::min(ctx.telemetryMinimumFocusDistance, focusDistance);
        }
        ctx.telemetryWasTransitioning = transitioning;

        if (ImGui::CollapsingHeader(
                "Transition Telemetry", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Transition: %s", transitioning ? "Active" : "Idle");
            ImGui::Text(
                "Position: %.3f, %.3f, %.3f",
                ctx.camera->pos.x, ctx.camera->pos.y, ctx.camera->pos.z);
            ImGui::Text(
                "Gaze: %.3f, %.3f, %.3f",
                ctx.camera->gazePoint.x,
                ctx.camera->gazePoint.y,
                ctx.camera->gazePoint.z);
            ImGui::Text("Focus Distance: %.3f m", focusDistance);
            if (ctx.telemetryHasSample)
            {
                ImGui::Text(
                    "Transition Min Position Y: %.3f m",
                    ctx.telemetryMinimumPositionY);
                ImGui::Text(
                    "Transition Min Focus Distance: %.3f m",
                    ctx.telemetryMinimumFocusDistance);
            }
            else
            {
                ImGui::TextDisabled("Transition minimums: no sample");
            }
            ImGui::Text("Near / Far: %.4f / %.1f m", ctx.camera->nearZ, ctx.camera->farZ);
            if (ctx.camera->projection == Engine::CameraProjection::Orthographic)
            {
                ImGui::Text("Projection: Orthographic");
                ImGui::Text("Ortho Height: %.3f m", ctx.camera->orthographicHeight);
            }
            else
            {
                ImGui::Text("Projection: Perspective");
                ImGui::Text("FOV Y: %.3f deg", ctx.camera->fov);
            }
            if (ImGui::Button("Reset Telemetry"))
            {
                ctx.telemetryHasSample = false;
                ctx.telemetryMinimumPositionY = 0.0f;
                ctx.telemetryMinimumFocusDistance = 0.0f;
            }
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
                    "Look Down Angle", &val, 0.0f,
                    Tank::App::CameraController::kMaximumLookDownDegrees,
                    1.0f, 25.0f, "%.1f deg"))
                {
                    ctx.cameraController->SetLookDownDegrees(val);
                }
            }
            {
                float val = ctx.cameraController->FollowYawOffsetDegrees();
                if (ImGuiWidgets::SliderFloatWithControls(
                    "Follow Yaw Offset", &val, -180.0f, 180.0f, 1.0f, 0.0f, "%.1f deg"))
                {
                    ctx.cameraController->SetFollowYawOffsetDegrees(val);
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
            if (ImGui::CollapsingHeader("Mortar Camera"))
            {
                if (ImGui::Button("Open Mortar Camera CheatWindow"))
                    ctx.mortarCheatWindowVisible = true;
                ImGui::TextWrapped("Offsets at maximum mortar elevation. Changes apply immediately to Follow Tank; Save Camera stores them in the selected camera slot.");
                ImGui::BeginDisabled(!ctx.cameraController->FollowEnabled());
                float pitch = ctx.cameraController->MortarPitchOffsetDegrees();
                if (ImGuiWidgets::SliderFloatWithControls("Mortar Pitch Offset", &pitch,
                    -60.0f, 60.0f, 1.0f, 18.0f, "%.1f deg"))
                    ctx.cameraController->SetMortarPitchOffsetDegrees(pitch);
                float distance = ctx.cameraController->MortarDistanceOffsetMeters();
                if (ImGuiWidgets::SliderFloatWithControls("Mortar Distance Offset", &distance,
                    -100.0f, 100.0f, 0.5f, 0.0f, "%.1f m"))
                    ctx.cameraController->SetMortarDistanceOffsetMeters(distance);
                float response = ctx.cameraController->MortarResponseSpeed();
                if (ImGuiWidgets::SliderFloatWithControls("Mortar Response Speed", &response,
                    0.1f, 30.0f, 0.1f, 6.0f, "%.1f /s"))
                    ctx.cameraController->SetMortarResponseSpeed(response);
                ImGui::TextDisabled("Higher response speed = faster change and return.");
                ImGui::EndDisabled();
            }
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
            const bool fovChanged = ImGuiWidgets::SliderFloatWithControls(
                "FOV Y", &ctx.camera->fov, 0.1f, 120.0f, 0.1f, 45.0f, "%.1f deg");
            changed |= fovChanged;
            if (fovChanged)
            {
                ctx.cameraController->ResetFollowState();
            }
            changed |= ImGuiWidgets::SliderFloatWithControls(
                "Horizontal Lens Shift", &ctx.camera->lensShiftX,
                -1.0f, 1.0f, 0.01f, 0.0f, "%.2f");
            changed |= ImGuiWidgets::SliderFloatWithControls(
                "Vertical Lens Shift", &ctx.camera->lensShiftY,
                -1.0f, 1.0f, 0.01f, 0.0f, "%.2f");
            ImGui::TextDisabled("Offsets the projection centre; 0.00 is symmetric.");

            const float halfFovRadians = DirectX::XMConvertToRadians(
                std::clamp(ctx.camera->fov, 0.1f, 179.0f)) * 0.5f;
            const float halfHeight = std::tan(halfFovRadians);
            float upFovDegrees = DirectX::XMConvertToDegrees(std::atan(
                halfHeight * (1.0f + ctx.camera->lensShiftY)));
            float downFovDegrees = DirectX::XMConvertToDegrees(std::atan(
                halfHeight * (1.0f - ctx.camera->lensShiftY)));
            ImGui::SeparatorText("Advanced Vertical Frustum");
            const bool upChanged = ImGuiWidgets::SliderFloatWithControls(
                "Up FOV", &upFovDegrees, 0.1f, 89.9f, 0.1f, 30.0f, "%.1f deg");
            const bool downChanged = ImGuiWidgets::SliderFloatWithControls(
                "Down FOV", &downFovDegrees, 0.1f, 89.9f, 0.1f, 30.0f, "%.1f deg");
            if (upChanged || downChanged)
            {
                const float top = std::tan(DirectX::XMConvertToRadians(upFovDegrees));
                const float bottom = std::tan(DirectX::XMConvertToRadians(downFovDegrees));
                const float total = std::atan(top) + std::atan(bottom);
                ctx.camera->fov = std::clamp(
                    DirectX::XMConvertToDegrees(total), 0.1f, 179.0f);
                ctx.camera->lensShiftY = std::clamp(
                    (top - bottom) / std::max(top + bottom, 0.0001f), -1.0f, 1.0f);
                changed = true;
                ctx.cameraController->ResetFollowState();
            }
        }
        else
        {
            changed |= ImGuiWidgets::SliderFloatWithControls(
                "Ortho Height", &ctx.camera->orthographicHeight,
                1.0f, 1000.0f, 1.0f, 10.0f, "%.1f m");
        }

        if (changed && ctx.setCamera)
        {
            ctx.setCamera(*ctx.camera);
        }
        ImGui::End();
        DrawMortarCameraCheatWindow(ctx);
    }
}
