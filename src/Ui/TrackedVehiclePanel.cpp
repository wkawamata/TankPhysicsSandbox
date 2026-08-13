#include "Ui/TrackedVehiclePanel.h"

#include "Input/GamepadState.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/TankVisualSettings.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <ImGuiWidgets.h>

#include <filesystem>
#include <string>

namespace Ui
{
	namespace
	{
		bool IsPending(float value, float appliedValue)
		{
			return std::abs(value - appliedValue) > 0.0001f;
		}

		bool SliderFloatWithPendingColor(
			const char* label,
			float* value,
			float min,
			float max,
			float delta,
			float defaultValue,
			const char* format,
			bool pending)
		{
			if (pending)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
			}
			const bool changed = ImGuiWidgets::SliderFloatWithControls(
				label, value, min, max, delta, defaultValue, format);
			if (pending)
			{
				ImGui::PopStyleColor();
			}
			return changed;
		}

		bool SliderIntWithPendingColor(
			const char* label,
			int* value,
			int min,
			int max,
			int delta,
			int defaultValue,
			const char* format,
			bool pending)
		{
			if (pending)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
			}
			const bool changed = ImGuiWidgets::SliderIntWithControls(
				label, value, min, max, delta, defaultValue, format);
			if (pending)
			{
				ImGui::PopStyleColor();
			}
			return changed;
		}
	}

	void DrawTrackedVehiclePanel(TrackedVehiclePanelContext& ctx)
	{
		const Tank::Physics::TrackedVehicleTestState& state = *ctx.state;
		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Tracked Vehicle");
		if (ImGui::Button("Reset GUI"))
		{
			ImGui::SetWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
			ImGui::SetWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_Always);
		}
		ImGui::SeparatorText("Tank Settings");
		ImGui::TextUnformatted("Slot");
		ImGui::SameLine();
		for (int slot = 0; slot < 3; ++slot)
		{
			if (slot > 0)
			{
				ImGui::SameLine();
			}
			const std::string label = std::to_string(slot + 1);
			if (ImGui::RadioButton(label.c_str(), *ctx.tankSettingsSlot == slot))
			{
				*ctx.tankSettingsSlot = slot;
				if (*ctx.tankSettingsAutoLoad && ctx.loadTankSettings)
				{
					ctx.loadTankSettings();
				}
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("AutoLoad##TankSettings", ctx.tankSettingsAutoLoad);
		if (ImGui::Button("Apply & Reset"))
		{
			if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
		}
		ImGui::SameLine();
		if (ImGui::Button("Save"))
		{
			if (ctx.saveTankSettings) ctx.saveTankSettings();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			if (ctx.loadTankSettings) ctx.loadTankSettings();
		}
		if (ctx.tankSettingsStatus && !ctx.tankSettingsStatus->empty())
		{
			ImGui::TextWrapped("%s", ctx.tankSettingsStatus->c_str());
		}
		ImGui::SeparatorText("Simulation");
		if (ImGui::Button(*ctx.trackedVehiclePaused ? "Resume" : "Pause"))
		{
			*ctx.trackedVehiclePaused = !*ctx.trackedVehiclePaused;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!*ctx.trackedVehiclePaused);
		if (ImGui::Button("Step Fwd"))
		{
			*ctx.trackedVehicleSingleStep = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Fire / Recoil"))
		{
			if (ctx.fireRecoil) ctx.fireRecoil();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Tank"))
		{
			if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
		}
		ImGui::Text("Frame: %.1f ms", ctx.cpuFrameTimeMs);
		ImGui::Text("Step: %d", state.stepIndex);
		ImGui::Text("Time: %.2f s", state.timeSeconds);
		ImGui::Text("Position: %.2f, %.2f, %.2f",
			state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
		{
			int wheelContactCount = 0;
			for (int i = 0; i < state.wheelCount; ++i)
			{
				wheelContactCount += state.wheels[static_cast<size_t>(i)].hasContact ? 1 : 0;
			}
			ImGui::Text("Wheel contacts: %d / %d", wheelContactCount, state.wheelCount);
		}
		ImGui::Text("Sleeping: %s", state.sleeping ? "yes" : "no");
		ImGui::TextUnformatted("Press ESC to return to the top menu.");
		if (ImGui::CollapsingHeader("Drive Telemetry"))
		{
			ImGui::Text("Speed: %.2f m/s  (%.1f km/h)",
				state.speedMetersPerSecond,
				state.speedMetersPerSecond * 3.6f);
			ImGui::Text("Maximum: %.2f m/s  (%.1f km/h)",
				state.maximumSpeedMetersPerSecond,
				state.maximumSpeedMetersPerSecond * 3.6f);
			if (state.zeroToTenTimeSeconds >= 0.0f)
			{
				ImGui::Text("0-10 m/s: %.2f s", state.zeroToTenTimeSeconds);
			}
			else
			{
				ImGui::TextUnformatted("0-10 m/s: measuring");
			}
			ImGui::Text("Engine: %.0f rpm", state.engineRpm);
			ImGui::Text("Gear: %d", state.transmissionGear);
			ImGui::Text("Clutch: %.0f%%", state.clutchFriction * 100.0f);
			int contacts[2] = {};
			float longitudinalImpulse[2] = {};
			float lateralImpulse[2] = {};
			float slipSpeed[2] = {};
			for (int i = 0; i < state.wheelCount; ++i)
			{
				const Tank::Physics::TrackedWheelState& wheel =
					state.wheels[static_cast<size_t>(i)];
				if (!wheel.hasContact || wheel.trackIndex < 0 || wheel.trackIndex > 1)
				{
					continue;
				}
				const int track = wheel.trackIndex;
				++contacts[track];
				longitudinalImpulse[track] +=
					std::abs(wheel.longitudinalImpulseNewtonSeconds);
				lateralImpulse[track] +=
					std::abs(wheel.lateralImpulseNewtonSeconds);
				slipSpeed[track] +=
					std::abs(wheel.longitudinalSlipMetersPerSecond);
			}
			for (int track = 0; track < 2; ++track)
			{
				if (contacts[track] > 0)
				{
					slipSpeed[track] /= static_cast<float>(contacts[track]);
				}
			}
			ImGui::SeparatorText("Track Traction");
			ImGui::Text("Left : contact %d  drive %.1f Ns  lateral %.1f Ns  slip %.2f m/s",
				contacts[0], longitudinalImpulse[0], lateralImpulse[0], slipSpeed[0]);
			ImGui::Text("Right: contact %d  drive %.1f Ns  lateral %.1f Ns  slip %.2f m/s",
				contacts[1], longitudinalImpulse[1], lateralImpulse[1], slipSpeed[1]);
		}
		if (ImGui::CollapsingHeader("Map"))
		{
			ImGui::Text("Active: %s",
				ctx.activeMapName != nullptr ? ctx.activeMapName->c_str() : "Unknown");
			ImGui::TextUnformatted("Friction colors");
			ImGui::ColorButton(
				"##LowFriction",
				ImVec4(50.0f / 255.0f, 120.0f / 255.0f, 210.0f / 255.0f, 1.0f),
				ImGuiColorEditFlags_NoTooltip,
				ImVec2(18.0f, 18.0f));
			ImGui::SameLine();
			ImGui::TextUnformatted("Low: < 0.45");
			ImGui::ColorButton(
				"##StandardFriction",
				ImVec4(70.0f / 255.0f, 145.0f / 255.0f, 85.0f / 255.0f, 1.0f),
				ImGuiColorEditFlags_NoTooltip,
				ImVec2(18.0f, 18.0f));
			ImGui::SameLine();
			ImGui::TextUnformatted("Standard: 0.45 - 0.79");
			ImGui::ColorButton(
				"##HighFriction",
				ImVec4(205.0f / 255.0f, 85.0f / 255.0f, 55.0f / 255.0f, 1.0f),
				ImGuiColorEditFlags_NoTooltip,
				ImVec2(18.0f, 18.0f));
			ImGui::SameLine();
			ImGui::TextUnformatted("High: >= 0.80");
		}
		if (ImGui::Checkbox("Physics Debug Overlay", ctx.physicsDebugOverlay))
		{
			if (ctx.updateScene) ctx.updateScene();
		}
		if (*ctx.physicsDebugOverlay)
		{
			ImGui::TextUnformatted("Cyan: suspension  Green/Orange: contact  Yellow: normal");
		}
		ImGui::Text("Controls: W/S drive, A/D skid turn, Shift+A/D pivot");
		ImGui::Text("Q/E roll, Space brake");
		const Tank::Input::GamepadState& gamepadState = ctx.gamepadState;
		if (ImGui::CollapsingHeader("Gamepad"))
		{
			if (!ctx.gamepadAvailable)
			{
				ImGui::TextUnformatted("Gamepad: GameInput unavailable");
			}
			else if (!gamepadState.connected)
			{
				ImGui::TextUnformatted("Gamepad: Not connected");
			}
			else
			{
				ImGui::TextUnformatted("Gamepad: Connected");
				ImGui::Text("Device: %s",
					gamepadState.deviceName.empty()
					? "Controller (name unavailable; identify by VID/PID)"
					: gamepadState.deviceName.c_str());
				ImGui::Text("VID: %04X  PID: %04X", gamepadState.vendorId, gamepadState.productId);
				ImGui::Text("Buttons: %u  Axes: %u  Switches: %u",
					gamepadState.buttonCount, gamepadState.axisCount, gamepadState.switchCount);
				ImGui::Text("Gamepad mapping: %s", gamepadState.hasGamepadMapping ? "yes" : "no");
				if (!gamepadState.hasGamepadMapping)
				{
					ImGui::TextUnformatted("Raw fallback: axes 0/1");
					for (std::uint32_t axis = 0;
						axis < gamepadState.axisCount && axis < gamepadState.rawAxes.size();
						++axis)
					{
						ImGui::Text("Axis %u: %.3f", axis, gamepadState.rawAxes[axis]);
					}
					ImGui::TextUnformatted("Pressed raw buttons:");
					ImGui::SameLine();
					bool anyButtonPressed = false;
					for (std::uint32_t button = 0;
						button < gamepadState.buttonCount && button < gamepadState.rawButtons.size();
						++button)
					{
						if (!gamepadState.rawButtons[button])
						{
							continue;
						}
						ImGui::SameLine();
						ImGui::Text("%u", button);
						anyButtonPressed = true;
					}
					if (!anyButtonPressed)
					{
						ImGui::SameLine();
						ImGui::TextUnformatted("none");
					}
					static constexpr const char* switchNames[] = {
						"Center", "Up", "Up-Right", "Right", "Down-Right",
						"Down", "Down-Left", "Left", "Up-Left"
					};
					for (std::uint32_t switchIndex = 0;
						switchIndex < gamepadState.switchCount &&
						switchIndex < gamepadState.rawSwitches.size();
						++switchIndex)
					{
						const std::uint32_t position = gamepadState.rawSwitches[switchIndex];
						const char* positionName =
							position < std::size(switchNames) ? switchNames[position] : "Unknown";
						ImGui::Text("Switch %u: %s", switchIndex, positionName);
					}
				}
				ImGui::Text("Left Stick: X %.2f  Y %.2f",
					gamepadState.leftStickX, gamepadState.leftStickY);
				ImGui::Text("Brake: %s", gamepadState.brakePressed ? "On" : "Off");
				ImGui::Text("Brake binding: raw button %u",
					Tank::Input::GamepadState::BrakeButtonIndex);
			}
		}
		if (ImGui::CollapsingHeader("Ground"))
		{
			SliderFloatWithPendingColor(
				"Floor Size",
				&ctx.envSettings->floorSizeM,
				20.0f,
				1000.0f,
				10.0f,
				200.0f,
				"%.0f m",
				IsPending(ctx.envSettings->floorSizeM, ctx.appliedEnvSettings->floorSizeM));
			SliderFloatWithPendingColor(
				"Floor Friction",
				&ctx.envSettings->floorFriction,
				0.0f,
				2.0f,
				0.05f,
				0.6f,
				"%.2f",
				IsPending(
					ctx.envSettings->floorFriction,
					ctx.appliedEnvSettings->floorFriction));
			ImGui::Checkbox("Grid Enabled", &ctx.envSettings->gridEnabled);
			SliderFloatWithPendingColor(
				"Grid Spacing",
				&ctx.envSettings->gridSpacingM,
				0.5f,
				20.0f,
				0.5f,
				5.0f,
				"%.1f m",
				IsPending(ctx.envSettings->gridSpacingM, ctx.appliedEnvSettings->gridSpacingM));
			SliderIntWithPendingColor(
				"Obstacle Count",
				&ctx.envSettings->obstacleCount,
				0,
				100,
				1,
				20,
				"%d",
				ctx.envSettings->obstacleCount != ctx.appliedEnvSettings->obstacleCount);
			SliderIntWithPendingColor(
				"Obstacle Seed",
				&ctx.envSettings->obstacleSeed,
				0,
				9999,
				1,
				1,
				"%d",
				ctx.envSettings->obstacleSeed != ctx.appliedEnvSettings->obstacleSeed);
			SliderFloatWithPendingColor(
				"Obstacle Area",
				&ctx.envSettings->obstacleAreaSizeM,
				20.0f,
				500.0f,
				10.0f,
				100.0f,
				"%.0f m",
				IsPending(
					ctx.envSettings->obstacleAreaSizeM,
					ctx.appliedEnvSettings->obstacleAreaSizeM));
			if (ImGui::Button("Apply Ground & Reset"))
			{
				if (ctx.enterTrackedVehicleMode) ctx.enterTrackedVehicleMode();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Ground"))
			{
				if (ctx.saveEnvSettings) ctx.saveEnvSettings();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Ground"))
			{
				if (ctx.loadEnvSettings) ctx.loadEnvSettings();
			}
			if (ctx.envSettingsStatus && !ctx.envSettingsStatus->empty())
			{
				ImGui::TextWrapped("%s", ctx.envSettingsStatus->c_str());
			}
		}
		if (ImGui::CollapsingHeader("Physics Settings"))
		{
		SliderFloatWithPendingColor(
			"Chassis Mass",
			&ctx.tankSettings->chassisMassKg,
			1000.0f,
			8000.0f,
			100.0f,
			4000.0f,
			"%.0f kg",
			IsPending(
				ctx.tankSettings->chassisMassKg,
				ctx.appliedTankSettings->chassisMassKg));
		SliderFloatWithPendingColor(
			"Recoil Impulse",
			&ctx.tankSettings->recoilImpulseNewtonSeconds,
			1000.0f,
			100000.0f,
			1000.0f,
			20000.0f,
			"%.0f N s",
			IsPending(
				ctx.tankSettings->recoilImpulseNewtonSeconds,
				ctx.appliedTankSettings->recoilImpulseNewtonSeconds));
		SliderFloatWithPendingColor(
			"Recoil Point Forward",
			&ctx.tankSettings->recoilPointForwardM,
			0.0f,
			3.0f,
			0.1f,
			1.2f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->recoilPointForwardM,
				ctx.appliedTankSettings->recoilPointForwardM));
		SliderFloatWithPendingColor(
			"Recoil Point Height",
			&ctx.tankSettings->recoilPointHeightM,
			0.0f,
			2.0f,
			0.1f,
			0.8f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->recoilPointHeightM,
				ctx.appliedTankSettings->recoilPointHeightM));
		ImGui::Checkbox("Neutral Brake", &ctx.tankSettings->neutralBrakeEnabled);
		SliderFloatWithPendingColor(
			"Neutral Brake Strength",
			&ctx.tankSettings->neutralBrakeAmount,
			0.0f,
			1.0f,
			0.05f,
			0.15f,
			"%.2f",
			false);

		}
		if (ImGui::CollapsingHeader("Rolling Parameters"))
		{

		ImGui::Checkbox("Rolling Input", &ctx.tankSettings->rollingInputEnabled);
		SliderFloatWithPendingColor(
			"Roll Torque",
			&ctx.tankSettings->rollTorqueNm,
			20000.0f,
			300000.0f,
			5000.0f,
			120000.0f,
			"%.0f N m",
			IsPending(ctx.tankSettings->rollTorqueNm, ctx.appliedTankSettings->rollTorqueNm));
		SliderFloatWithPendingColor(
			"Roll Distance",
			&ctx.tankSettings->rollDistanceM,
			0.5f,
			5.0f,
			0.1f,
			2.4f,
			"%.2f m",
			IsPending(ctx.tankSettings->rollDistanceM, ctx.appliedTankSettings->rollDistanceM));
		SliderFloatWithPendingColor(
			"Torque Cutoff Angle",
			&ctx.tankSettings->rollTorqueCutoffDegrees,
			45.0f,
			120.0f,
			5.0f,
			90.0f,
			"%.0f deg",
			IsPending(
				ctx.tankSettings->rollTorqueCutoffDegrees,
				ctx.appliedTankSettings->rollTorqueCutoffDegrees));
		SliderFloatWithPendingColor(
			"Stabilization Torque",
			&ctx.tankSettings->rollStabilizationTorqueNm,
			0.0f,
			100000.0f,
			5000.0f,
			30000.0f,
			"%.0f N m",
			IsPending(
				ctx.tankSettings->rollStabilizationTorqueNm,
				ctx.appliedTankSettings->rollStabilizationTorqueNm));
		SliderFloatWithPendingColor(
			"Stabilization Damping",
			&ctx.tankSettings->rollStabilizationDampingNms,
			0.0f,
			50000.0f,
			1000.0f,
			10000.0f,
			"%.0f N m s",
			IsPending(
				ctx.tankSettings->rollStabilizationDampingNms,
				ctx.appliedTankSettings->rollStabilizationDampingNms));

		}
		if (ImGui::CollapsingHeader("Tank Design"))
		{

		ImGui::SeparatorText("Body Material");
			auto drawMaterial = [](const char* label, Tank::Rendering::BodyMaterialSettings& material)
				{
					bool changed = false;
					if (ImGui::TreeNode(label))
					{
						changed |= ImGui::ColorEdit3("Albedo", &material.albedo.r);
						changed |= ImGuiWidgets::SliderFloatWithControls(
							"Roughness", &material.roughness, 0.04f, 1.0f, 0.02f, 0.8f);
						changed |= ImGuiWidgets::SliderFloatWithControls(
							"Metallic", &material.metallic, 0.0f, 1.0f, 0.05f, 0.0f);
						changed |= ImGuiWidgets::SliderFloatWithControls(
							"Ambient Occlusion",
							&material.ambientOcclusion,
							0.0f,
							1.0f,
							0.05f,
							1.0f);
						changed |= ImGuiWidgets::SliderFloatWithControls(
							"Emissive", &material.emissive, 0.0f, 4.0f, 0.1f, 0.0f);
						ImGui::TreePop();
					}
					return changed;
				};
			bool materialChanged = false;
			materialChanged |= drawMaterial("Hull Upper", ctx.visualSettings->hullUpper);
			materialChanged |= drawMaterial("Hull Lower", ctx.visualSettings->hullLower);
			materialChanged |= drawMaterial(
				"Structure Upper", ctx.visualSettings->structureUpper);
			materialChanged |= drawMaterial(
				"Structure Lower", ctx.visualSettings->structureLower);
			materialChanged |= drawMaterial("Wheels", ctx.visualSettings->wheels);
			ImGui::BeginDisabled(!ctx.visualSettings->colorWheelsByContact);
			materialChanged |= drawMaterial(
				"Contacted Wheels",
				ctx.visualSettings->contactedWheels);
			ImGui::EndDisabled();
			materialChanged |= drawMaterial("Track Shoes", ctx.visualSettings->trackShoes);
			materialChanged |= drawMaterial(
				"Track Proxies", ctx.visualSettings->trackProxies);
			materialChanged |= drawMaterial(
				"Forward Marker", ctx.visualSettings->forwardMarker);
			*ctx.tankVisualMaterialApplyPending |= materialChanged;
			if (*ctx.tankVisualMaterialApplyPending && !ImGui::IsAnyItemActive())
			{
				if (ctx.applyMaterials) ctx.applyMaterials();
				*ctx.tankVisualMaterialApplyPending = false;
			}
			if (ImGui::Button("Save Visual"))
			{
				if (ctx.saveTankVisualSettings) ctx.saveTankVisualSettings();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Visual"))
			{
				if (ctx.loadTankVisualSettings) ctx.loadTankVisualSettings();
			}
			ImGui::SameLine();
			ImGui::Checkbox("AutoLoad##TankVisual", ctx.tankVisualSettingsAutoLoad);
			if (ctx.tankVisualSettingsStatus && !ctx.tankVisualSettingsStatus->empty())
			{
				ImGui::TextWrapped("%s", ctx.tankVisualSettingsStatus->c_str());
			}
		if (ImGui::Checkbox("Track Shoe Display", ctx.trackShoeDisplay))
		{
			if (ctx.updateScene) ctx.updateScene();
		}
		if (ImGui::Checkbox(
			"Color Wheels by Contact",
			&ctx.visualSettings->colorWheelsByContact))
		{
			if (ctx.applyMaterials) ctx.applyMaterials();
		}
		if (ImGui::Checkbox("Show Track Proxies", ctx.showTrackProxies))
		{
			if (ctx.updateScene) ctx.updateScene();
		}
		}
		if (ImGui::CollapsingHeader("Tank Model Display"))
		{
			if (ctx.tankModelLoadStatus && !ctx.tankModelLoadStatus->empty())
			{
				ImGui::TextWrapped("%s", ctx.tankModelLoadStatus->c_str());
			}
			bool displayChanged = false;
			displayChanged |= ImGui::Checkbox("Dummy Model", ctx.showDummyModel);
			ImGui::SeparatorText("glTF Overlay");
			displayChanged |= ImGui::Checkbox("Body", ctx.showGltfBody);
			displayChanged |= ImGui::Checkbox("Cannon", ctx.showGltfCannon);
			displayChanged |= ImGui::Checkbox("Side", ctx.showGltfSide);
			if (displayChanged && ctx.updateScene)
			{
				ctx.updateScene();
			}
		}
		if (ImGui::CollapsingHeader("Turn Traction"))
		{
		SliderFloatWithPendingColor(
			"Longitudinal Friction",
			&ctx.tankSettings->trackLongitudinalFriction,
			0.0f,
			10.0f,
			0.1f,
			4.0f,
			"%.2f",
			IsPending(
				ctx.tankSettings->trackLongitudinalFriction,
				ctx.appliedTankSettings->trackLongitudinalFriction));
		SliderFloatWithPendingColor(
			"Lateral Friction",
			&ctx.tankSettings->trackLateralFriction,
			0.0f,
			10.0f,
			0.1f,
			2.0f,
			"%.2f",
			IsPending(
				ctx.tankSettings->trackLateralFriction,
				ctx.appliedTankSettings->trackLateralFriction));
		SliderFloatWithPendingColor(
			"Stationary Inner Track Ratio",
			&ctx.tankSettings->stationaryTurnInnerTrackRatio,
			0.0f,
			1.0f,
			0.05f,
			0.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->stationaryTurnInnerTrackRatio,
				ctx.appliedTankSettings->stationaryTurnInnerTrackRatio));
		SliderFloatWithPendingColor(
			"Stationary Left Track",
			&ctx.tankSettings->stationaryTurnLeftTraction,
			0.0f,
			1.0f,
			0.05f,
			1.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->stationaryTurnLeftTraction,
				ctx.appliedTankSettings->stationaryTurnLeftTraction));
		SliderFloatWithPendingColor(
			"Stationary Right Track",
			&ctx.tankSettings->stationaryTurnRightTraction,
			0.0f,
			1.0f,
			0.05f,
			1.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->stationaryTurnRightTraction,
				ctx.appliedTankSettings->stationaryTurnRightTraction));
		SliderFloatWithPendingColor(
			"Pivot Left Track",
			&ctx.tankSettings->pivotTurnLeftTraction,
			0.0f,
			1.0f,
			0.05f,
			1.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->pivotTurnLeftTraction,
				ctx.appliedTankSettings->pivotTurnLeftTraction));
		SliderFloatWithPendingColor(
			"Pivot Right Track",
			&ctx.tankSettings->pivotTurnRightTraction,
			0.0f,
			1.0f,
			0.05f,
			1.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->pivotTurnRightTraction,
				ctx.appliedTankSettings->pivotTurnRightTraction));
		}
		if (ImGui::CollapsingHeader("Drive Response"))
		{
		SliderFloatWithPendingColor(
			"Engine Torque",
			&ctx.tankSettings->engineMaxTorqueNm,
			100.0f,
			5000.0f,
			50.0f,
			900.0f,
			"%.0f Nm",
			IsPending(
				ctx.tankSettings->engineMaxTorqueNm,
				ctx.appliedTankSettings->engineMaxTorqueNm));
		SliderFloatWithPendingColor(
			"Engine Max RPM",
			&ctx.tankSettings->engineMaxRpm,
			2000.0f,
			10000.0f,
			100.0f,
			5000.0f,
			"%.0f rpm",
			IsPending(
				ctx.tankSettings->engineMaxRpm,
				ctx.appliedTankSettings->engineMaxRpm));
		SliderFloatWithPendingColor(
			"Shift Down RPM",
			&ctx.tankSettings->transmissionShiftDownRpm,
			500.0f,
			9000.0f,
			100.0f,
			1000.0f,
			"%.0f rpm",
			IsPending(
				ctx.tankSettings->transmissionShiftDownRpm,
				ctx.appliedTankSettings->transmissionShiftDownRpm));
		SliderFloatWithPendingColor(
			"Shift Up RPM",
			&ctx.tankSettings->transmissionShiftUpRpm,
			1000.0f,
			9900.0f,
			100.0f,
			4375.0f,
			"%.0f rpm",
			IsPending(
				ctx.tankSettings->transmissionShiftUpRpm,
				ctx.appliedTankSettings->transmissionShiftUpRpm));
		SliderFloatWithPendingColor(
			"Clutch Strength",
			&ctx.tankSettings->transmissionClutchStrength,
			1.0f,
			100.0f,
			1.0f,
			10.0f,
			"%.1f",
			IsPending(
				ctx.tankSettings->transmissionClutchStrength,
				ctx.appliedTankSettings->transmissionClutchStrength));
		SliderFloatWithPendingColor(
			"Final Drive Ratio",
			&ctx.tankSettings->finalDriveRatio,
			0.25f,
			4.0f,
			0.05f,
			1.0f,
			"%.2f x",
			IsPending(
				ctx.tankSettings->finalDriveRatio,
				ctx.appliedTankSettings->finalDriveRatio));
		SliderFloatWithPendingColor(
			"Clutch Release",
			&ctx.tankSettings->clutchReleaseTimeSeconds,
			0.01f,
			0.5f,
			0.01f,
			0.03f,
			"%.2f s",
			IsPending(
				ctx.tankSettings->clutchReleaseTimeSeconds,
				ctx.appliedTankSettings->clutchReleaseTimeSeconds));
		}
		if (ImGui::CollapsingHeader("Body Yaw"))
		{
		SliderFloatWithPendingColor(
			"Yaw Speed Limit",
			&ctx.tankSettings->yawSpeedLimitDegrees,
			15.0f,
			720.0f,
			5.0f,
			720.0f,
			"%.0f deg/s",
			IsPending(
				ctx.tankSettings->yawSpeedLimitDegrees,
				ctx.appliedTankSettings->yawSpeedLimitDegrees));
		SliderFloatWithPendingColor(
			"Yaw Damping",
			&ctx.tankSettings->yawDamping,
			0.0f,
			30.0f,
			0.5f,
			0.0f,
			"%.1f /s",
			IsPending(
				ctx.tankSettings->yawDamping,
				ctx.appliedTankSettings->yawDamping));
		}
		if (ImGui::CollapsingHeader("Track Layout Adjustment"))
		{
		SliderFloatWithPendingColor(
			"Track Width", &ctx.tankSettings->trackWidthM, 0.15f, 0.6f, 0.01f, 0.3f, "%.2f m",
			IsPending(ctx.tankSettings->trackWidthM, ctx.appliedTankSettings->trackWidthM));
		SliderFloatWithPendingColor(
			"Track Spacing", &ctx.tankSettings->trackSpacingM, 1.8f, 3.2f, 0.1f, 2.4f, "%.2f m",
			IsPending(ctx.tankSettings->trackSpacingM, ctx.appliedTankSettings->trackSpacingM));
		SliderFloatWithPendingColor(
			"Suspension Stroke", &ctx.tankSettings->rideHeightScale, 0.5f, 1.1f, 0.05f, 0.8f, "%.2f x",
			IsPending(ctx.tankSettings->rideHeightScale, ctx.appliedTankSettings->rideHeightScale));
		SliderFloatWithPendingColor(
			"Chassis Width", &ctx.tankSettings->chassisWidthM, 1.6f, 3.2f, 0.1f, 2.4f, "%.2f m",
			IsPending(ctx.tankSettings->chassisWidthM, ctx.appliedTankSettings->chassisWidthM));
		SliderFloatWithPendingColor(
			"Chassis Length", &ctx.tankSettings->chassisLengthM, 3.0f, 5.5f, 0.1f, 4.0f, "%.2f m",
			IsPending(ctx.tankSettings->chassisLengthM, ctx.appliedTankSettings->chassisLengthM));
		SliderFloatWithPendingColor(
			"End Wheel Radius",
			&ctx.tankSettings->endWheelRadiusM,
			0.01f,
			0.6f,
			0.01f,
			0.4f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->endWheelRadiusM,
				ctx.appliedTankSettings->endWheelRadiusM));
		SliderFloatWithPendingColor(
			"Road Wheel Radius",
			&ctx.tankSettings->roadWheelRadiusM,
			0.2f,
			0.5f,
			0.01f,
			0.3f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->roadWheelRadiusM,
				ctx.appliedTankSettings->roadWheelRadiusM));
		{
			const char* wheelLayouts[] = { "1 + 2 + 1", "1 + 3 + 1", "1 + 4 + 1" };
			int wheelLayoutIndex = std::clamp(ctx.tankSettings->roadWheelCount, 2, 4) - 2;
			if (ImGui::Combo("Wheel Layout", &wheelLayoutIndex, wheelLayouts, std::size(wheelLayouts)))
			{
				ctx.tankSettings->roadWheelCount = wheelLayoutIndex + 2;
			}
		}
		SliderFloatWithPendingColor(
			"End Wheel Offset",
			&ctx.tankSettings->endWheelOffsetM,
			0.0f,
			1.0f,
			0.05f,
			0.0f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->endWheelOffsetM,
				ctx.appliedTankSettings->endWheelOffsetM));
		SliderFloatWithPendingColor(
			"End Wheel Vertical Offset",
			&ctx.tankSettings->endWheelVerticalOffsetM,
			-0.5f,
			0.5f,
			0.05f,
			0.0f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->endWheelVerticalOffsetM,
				ctx.appliedTankSettings->endWheelVerticalOffsetM));
		SliderFloatWithPendingColor(
			"Road Wheel Vertical Offset",
			&ctx.tankSettings->roadWheelVerticalOffsetM,
			-0.5f,
			0.5f,
			0.05f,
			0.0f,
			"%.2f m",
			IsPending(
				ctx.tankSettings->roadWheelVerticalOffsetM,
				ctx.appliedTankSettings->roadWheelVerticalOffsetM));
		if (ctx.tankSettings->roadWheelCount == 2)
		{
			SliderFloatWithPendingColor(
				"Middle Wheel Offset",
				&ctx.tankSettings->twoRoadWheelOffsetM,
				0.1f,
				2.0f,
				0.05f,
				0.67f,
				"%.2f m",
				IsPending(
					ctx.tankSettings->twoRoadWheelOffsetM,
					ctx.appliedTankSettings->twoRoadWheelOffsetM));
		}
		else if (ctx.tankSettings->roadWheelCount == 3)
		{
			SliderFloatWithPendingColor(
				"Middle Wheel Offset",
				&ctx.tankSettings->threeRoadWheelOffsetM,
				0.1f,
				2.0f,
				0.05f,
				1.0f,
				"%.2f m",
				IsPending(
					ctx.tankSettings->threeRoadWheelOffsetM,
					ctx.appliedTankSettings->threeRoadWheelOffsetM));
		}
		ImGui::Checkbox("Start Upside Down", &ctx.tankSettings->startUpsideDown);
		}
		if (ImGui::CollapsingHeader("Export glTF"))
		{
        if (ctx.tankModelExportBinary)
        {
            bool binary = *ctx.tankModelExportBinary;
            if (ImGui::RadioButton("glTF (.gltf)", !binary))
            {
                binary = false;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("glB (.glb)", binary))
            {
                binary = true;
            }
            *ctx.tankModelExportBinary = binary;
        }
        if (ctx.tankModelExportPath)
        {
            ImGui::InputText("Export Path", ctx.tankModelExportPath);
            std::filesystem::path resolvedPath(*ctx.tankModelExportPath);
            resolvedPath.replace_extension(
                ctx.tankModelExportBinary && *ctx.tankModelExportBinary
                    ? ".glb"
                    : ".gltf");
            resolvedPath = std::filesystem::absolute(resolvedPath).lexically_normal();
            ImGui::TextWrapped("Resolved: %s", resolvedPath.string().c_str());
        }
		if (ImGui::Button("Export Tank glTF"))
		{
			if (ctx.exportTankModel) ctx.exportTankModel();
		}
		if (ctx.tankModelExportStatus && !ctx.tankModelExportStatus->empty())
		{
			ImGui::TextWrapped("%s", ctx.tankModelExportStatus->c_str());
		}
		}
		if (ImGui::CollapsingHeader("Track Input"))
		{
		ImGui::Text(
			"Analog track axes 1 / 3: %s",
			!ctx.analogTracksConnected ? "not connected" :
				(ctx.analogTracksArmed ? "ready" : "waiting for neutral"));
		ImGui::Text(
			"Left %.2f  Right %.2f  Roll %.2f",
			ctx.analogLeftTrack,
			ctx.analogRightTrack,
			ctx.analogRoll);
		if (state.yawSpeedLimited)
		{
			ImGui::PushStyleColor(
				ImGuiCol_Text,
				ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
		}
		ImGui::Text(
			"Tank Yaw Speed: %+.1f deg/s%s",
			state.yawSpeedDegrees,
			state.yawSpeedLimited ? "  LIMITED" : "");
		if (state.yawSpeedLimited)
		{
			ImGui::PopStyleColor();
		}
		const Tank::Physics::TrackedDriverInput& driverInput = *ctx.driverInput;
		ImGui::Text(
			"SetDriverInput: Fwd %.2f  L %.2f  R %.2f  Brake %.2f",
			driverInput.forward,
			driverInput.leftRatio,
			driverInput.rightRatio,
			driverInput.brake);
		}
		ImGui::End();
	}
}
