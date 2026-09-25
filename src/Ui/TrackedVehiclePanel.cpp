#include "Ui/TrackedVehiclePanel.h"
#include "Ui/DriveTelemetry.h"
#include "App/RollingSpeedOptimizationSession.h"
#include "App/RollingProfileSlotSelection.h"

#include "Input/GamepadState.h"
#include "Physics/PhysicsEnvironmentSettings.h"
#include "Physics/TankTypes.h"
#include "Physics/TrackedVehicleTest.h"
#include "Rendering/TankVisualSettings.h"
#include "imgui.h"
#include "imgui_stdlib.h"
#include <ImGuiWidgets.h>

#include <filesystem>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <Physics/MobilityTypes.h>
#include <Physics/RollingSpeedOptimizer.h>
#include <Physics/RollingSpeedTuning.h>
#include <Physics/SpecialMoveTypes.h>

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

		bool IsPendingColor(
			const Tank::Physics::ColorRgb& value,
			const Tank::Physics::ColorRgb& appliedValue)
		{
			return
				IsPending(value.r, appliedValue.r) ||
				IsPending(value.g, appliedValue.g) ||
				IsPending(value.b, appliedValue.b);
		}

		bool ColorEdit3WithPendingColor(
			const char* label,
			float color[3],
			bool pending)
		{
			if (pending)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
			}
			const bool changed = ImGui::ColorEdit3(
				label,
				color,
				ImGuiColorEditFlags_NoInputs);
			if (pending)
			{
				ImGui::PopStyleColor();
			}
			return changed;
		}

		const char* MobilityReasonName(
			Tank::Physics::MobilityTransitionReason reason)
		{
			switch (reason)
			{
			case Tank::Physics::MobilityTransitionReason::StopConditionsEntered:
				return "StopConditionsEntered";
			case Tank::Physics::MobilityTransitionReason::StopConfirmed:
				return "StopConfirmed";
			case Tank::Physics::MobilityTransitionReason::DriveRequested:
				return "DriveRequested";
			case Tank::Physics::MobilityTransitionReason::LinearSpeedExceeded:
				return "LinearSpeedExceeded";
			case Tank::Physics::MobilityTransitionReason::AngularSpeedExceeded:
				return "AngularSpeedExceeded";
			case Tank::Physics::MobilityTransitionReason::TrackSlipExceeded:
				return "TrackSlipExceeded";
			case Tank::Physics::MobilityTransitionReason::SuspensionUnstable:
				return "SuspensionUnstable";
			case Tank::Physics::MobilityTransitionReason::RequiredContactLost:
				return "RequiredContactLost";
			case Tank::Physics::MobilityTransitionReason::PoseUnstable:
				return "PoseUnstable";
			case Tank::Physics::MobilityTransitionReason::InvalidObservation:
				return "InvalidObservation";
			default:
				return "None";
			}
		}

		const char* RollingPhaseName(Tank::Physics::RollingPhase phase)
		{
			switch (phase)
			{
			case Tank::Physics::RollingPhase::Windup: return "Windup";
			case Tank::Physics::RollingPhase::PoweredRoll: return "PoweredRoll";
			case Tank::Physics::RollingPhase::Evaluating: return "Evaluating";
			case Tank::Physics::RollingPhase::CommitRoll: return "CommitRoll";
			case Tank::Physics::RollingPhase::BallisticRoll: return "BallisticRoll";
			case Tank::Physics::RollingPhase::Settling: return "Settling";
			default: return "None";
			}
		}

		const char* RollingDecisionName(Tank::Physics::RollingDecision decision)
		{
			switch (decision)
			{
			case Tank::Physics::RollingDecision::ContinueForward:
				return "ContinueForward";
			case Tank::Physics::RollingDecision::ReturnToStart:
				return "ReturnToStart";
			default:
				return "None";
			}
		}

		const char* SpecialMoveStateName(Tank::Physics::SpecialMoveState state)
		{
			switch (state)
			{
			case Tank::Physics::SpecialMoveState::RollStarting: return "RollStarting";
			case Tank::Physics::SpecialMoveState::Rolling: return "Rolling";
			case Tank::Physics::SpecialMoveState::MortarStarting: return "MortarStarting";
			case Tank::Physics::SpecialMoveState::MortarAiming: return "MortarAiming";
			case Tank::Physics::SpecialMoveState::Blocked: return "Blocked";
			case Tank::Physics::SpecialMoveState::RecoveringToStart: return "Recovering";
			default: return "Idle";
			}
		}

		void DrawStateSummary(
			const TrackedVehiclePanelContext& ctx,
			const Tank::Physics::TrackedVehicleTestState& state)
		{
			const Tank::Physics::TankMotionObservation& motion =
				state.motionObservation;
			if (!ImGui::CollapsingHeader(
				"State Summary", ImGuiTreeNodeFlags_DefaultOpen))
			{
				return;
			}
			const char* mobilityName = "Moving";
			switch (state.mobility.state)
			{
			case Tank::Physics::MobilityState::StopCandidate:
				mobilityName = "StopCandidate";
				break;
			case Tank::Physics::MobilityState::Stopped:
				mobilityName = "Stopped";
				break;
			default:
				break;
			}
			ImGui::Text("Mobility: %s  Time: %.2f s  Stop: %.0f%%",
				mobilityName,
				state.mobility.stateTimeSeconds,
				state.mobility.stopCandidateProgress * 100.0f);
			ImGui::Text("Roll: %s  Special: %s (#%llu)",
				RollingPhaseName(state.rollingPhase),
				SpecialMoveStateName(state.specialMove.state),
				static_cast<unsigned long long>(state.specialMove.transitionCount));
			ImGui::Text("Roll Chain: %s",
				state.rollChainAvailable ? "Ready while sliding" : "Stopped only");
			const ImVec4 rollingDecisionColor =
				state.lastRollingDecision == Tank::Physics::RollingDecision::ReturnToStart
				? ImVec4(1.0f, 0.35f, 0.20f, 1.0f)
				: state.lastRollingDecision == Tank::Physics::RollingDecision::ContinueForward
				? ImVec4(0.35f, 1.0f, 0.45f, 1.0f)
				: ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
			ImGui::TextColored(rollingDecisionColor,
				"Roll Decision: %s (#%llu)  Command: %+.0f  Input: %+.0f",
				RollingDecisionName(state.lastRollingDecision),
				static_cast<unsigned long long>(state.rollingDecisionCount),
				state.rollingDecisionCommandSign,
				state.rollingDecisionInputSign);
			ImGui::TextDisabled(
				"Roll Trace: #%llu  Request: %+.0f  Command: %+.0f  Input: %+.0f",
				static_cast<unsigned long long>(state.rollingTraceSequence),
				state.rollingTraceRequestSign,
				state.rollingTraceCommandSign,
				state.rollingTraceInputSign);
			ImGui::Text("Mortar: %s%s  %.1f deg / %.1f m",
				state.mortarAim.canFire ? "Ready" : "Charging",
				state.mortarAim.atMaximum ? " (Max)" : "",
				state.mortarAim.angleDegrees,
				state.mortarAim.rangeMeters);
			ImGui::Text("Assault rounds fired: %llu",
				static_cast<unsigned long long>(state.assaultWeapon.roundsFired));
			for (const auto& box : state.destructibleBoxes)
			{
				ImGui::Text("Box %llu: %s / HP %.0f",
					static_cast<unsigned long long>(box.target.id),
					box.target.active ? "Active" : "Destroyed", box.target.hitPoints);
			}
			ImGui::Text("Track: %s  Obstruction: %s%s",
				state.trackInputSwapped
				? "Swapped (Inverted)"
				: "Normal (Upright)",
				state.rollingObstructionSuspected ? "Detected" : "None",
				state.rollingRecoveryActive ? " / Recovery" : "");
			ImGui::Text("Lever X: L %+.2f  R %+.2f  Roll: %+.2f",
				ctx.leftLeverX,
				ctx.rightLeverX,
				ctx.analogRoll);
			if (ctx.appliedTankSettings != nullptr)
			{
				ImGui::Text("Rolling Input Applied: %s",
					ctx.appliedTankSettings->rollingInputEnabled ? "ON" : "OFF");
			}
			if (ImGui::CollapsingHeader("Motion / Stop Diagnostics"))
			{
				ImGui::Text("Mobility Reason: %s",
					MobilityReasonName(state.mobility.lastTransitionReason));
				ImGui::Text("Motion Data: %s",
					motion.allFinite ? "Valid" : "INVALID");
				ImGui::Text(
					"Speed: %.2f m/s  Angular: %.2f rad/s",
					motion.linearSpeedMetersPerSecond,
					motion.angularSpeedRadiansPerSecond);
				ImGui::Text(
					"Contacts: L %d (%d lower)  R %d (%d lower)",
					motion.tracks[0].contactCount,
					motion.tracks[0].lowerSurfaceContactCount,
					motion.tracks[1].contactCount,
					motion.tracks[1].lowerSurfaceContactCount);
				ImGui::Text(
					"Max Slip: L %.2f  R %.2f m/s",
					motion.tracks[0]
					.maximumAbsoluteLongitudinalSlipMetersPerSecond,
					motion.tracks[1]
					.maximumAbsoluteLongitudinalSlipMetersPerSecond);
				if (ctx.tankSettings != nullptr)
				{
					const Tank::Physics::TankSettings& settings = *ctx.tankSettings;
					ImGui::Text("Stop In: Speed %.2f  Angular %.2f  Slip %.2f",
						settings.stoppedEnterLinearSpeedMetersPerSecond,
						settings.stoppedEnterAngularSpeedRadiansPerSecond,
						settings.stoppedEnterTrackSlipMetersPerSecond);
					ImGui::Text("Stop Out: Speed %.2f  Angular %.2f  Slip %.2f",
						settings.stoppedExitLinearSpeedMetersPerSecond,
						settings.stoppedExitAngularSpeedRadiansPerSecond,
						settings.stoppedExitTrackSlipMetersPerSecond);
				}
			}
		}

		void DrawLeverInputMapping(
			TrackedVehiclePanelContext& ctx)
		{
			if (!ImGui::CollapsingHeader("Input Mapping"))
			{
				return;
			}

			ImGui::TextUnformatted("Device-specific raw axis and button mapping.");
			ImGui::TextUnformatted("File: Config/input_devices.json");
			if (ctx.inputDeviceProfile != nullptr)
			{
				auto& profile = *ctx.inputDeviceProfile;
				ImGui::Text("Profile: %s", profile.id.c_str());
				ImGui::Text("VID: %04X  PID: %04X", profile.vendorId, profile.productId);
				int leftTrackAxis = static_cast<int>(profile.leftTrackAxis);
				int rightTrackAxis = static_cast<int>(profile.rightTrackAxis);
				int leftRollAxis = static_cast<int>(profile.leftRollAxis);
				int rightRollAxis = static_cast<int>(profile.rightRollAxis);
				if (ImGui::InputInt("Left Track Axis", &leftTrackAxis))
				{
					profile.leftTrackAxis = static_cast<std::size_t>(
						std::clamp(leftTrackAxis, 0, 15));
				}
				if (ImGui::InputInt("Right Track Axis", &rightTrackAxis))
				{
					profile.rightTrackAxis = static_cast<std::size_t>(
						std::clamp(rightTrackAxis, 0, 15));
				}
				if (ImGui::InputInt("Left Roll Axis", &leftRollAxis))
					profile.leftRollAxis = static_cast<std::size_t>(std::clamp(leftRollAxis, 0, 15));
				if (ImGui::InputInt("Right Roll Axis", &rightRollAxis))
					profile.rightRollAxis = static_cast<std::size_t>(std::clamp(rightRollAxis, 0, 15));
				ImGui::Checkbox("Invert Left Track", &profile.invertLeftTrack);
				ImGui::Checkbox("Invert Right Track", &profile.invertRightTrack);
				ImGui::Checkbox("Invert Left Roll", &profile.invertLeftRoll);
				ImGui::Checkbox("Invert Right Roll", &profile.invertRightRoll);
				ImGui::SliderFloat("Neutral", &profile.neutral, 0.0f, 1.0f, "%.3f");
				ImGui::SliderFloat("Neutral Tolerance", &profile.neutralTolerance, 0.0f, 0.25f, "%.3f");
				ImGui::SliderFloat("Deadzone", &profile.deadzone, 0.0f, 0.5f, "%.3f");
				int brakeButton = static_cast<int>(profile.brakeButton);
				int fireButton = static_cast<int>(profile.fireButton);
				int cameraNextButton = static_cast<int>(profile.cameraNextButton);
				int cameraPreviousButton = static_cast<int>(profile.cameraPreviousButton);
				if (ImGui::InputInt("Brake Button", &brakeButton))
					profile.brakeButton = static_cast<std::uint32_t>(std::clamp(brakeButton, 0, 63));
				if (ImGui::InputInt("Fire Button", &fireButton))
					profile.fireButton = static_cast<std::uint32_t>(std::clamp(fireButton, 0, 63));
				if (ImGui::InputInt("Camera Next Button", &cameraNextButton))
					profile.cameraNextButton = static_cast<std::uint32_t>(std::clamp(cameraNextButton, 0, 63));
				if (ImGui::InputInt("Camera Previous Button", &cameraPreviousButton))
					profile.cameraPreviousButton = static_cast<std::uint32_t>(std::clamp(cameraPreviousButton, 0, 63));
			}
			else
			{
				ImGui::TextUnformatted("Current device has no matching profile.");
			}
			if (ImGui::Button("Save Mapping") && ctx.saveInputMappingSettings)
			{
				ctx.saveInputMappingSettings();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load Mapping") && ctx.loadInputMappingSettings)
			{
				ctx.loadInputMappingSettings();
			}
			if (ctx.inputMappingStatus != nullptr &&
				!ctx.inputMappingStatus->empty())
			{
				ImGui::TextWrapped("%s", ctx.inputMappingStatus->c_str());
			}
		}
	}

	void DrawRollingCheatWindow(TrackedVehiclePanelContext& ctx)
	{
		if (ctx.rollingCheatWindowVisible == nullptr ||
			!*ctx.rollingCheatWindowVisible)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(520.0f, 620.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("CheatWindow: Rolling", ctx.rollingCheatWindowVisible))
		{
			ImGui::End();
			return;
		}
		if (ctx.rollingCheatFontScale != nullptr)
		{
			*ctx.rollingCheatFontScale = std::clamp(
				*ctx.rollingCheatFontScale, 0.5f, 2.0f);
			ImGui::SetWindowFontScale(*ctx.rollingCheatFontScale);
			ImGui::SetNextItemWidth(180.0f);
			ImGui::SliderFloat("Text Scale##RollingCheat", ctx.rollingCheatFontScale,
				0.5f, 2.0f, "%.2f x", ImGuiSliderFlags_AlwaysClamp);
			ImGui::Separator();
		}
		bool japanese = ctx.rollingCheatWindowJapanese != nullptr &&
			*ctx.rollingCheatWindowJapanese;
		if (ImGui::Button("日本語##RollingCheatLanguage"))
		{
			japanese = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("English##RollingCheatLanguage"))
		{
			japanese = false;
		}
		if (ctx.rollingCheatWindowJapanese != nullptr)
		{
			*ctx.rollingCheatWindowJapanese = japanese;
		}

		ImGui::TextWrapped("%s", japanese
			? "ローリング挙動の調整値です。値を変更した後は Reset Tank / Apply を押してください。"
			: "Rolling behavior tuning. Press Reset Tank / Apply after changing a value.");
		ImGui::TextUnformatted(japanese ? "設定値の色" : "Setting value colors");
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImGui::TextUnformatted(japanese ? "白: 現在値はSave済みの値と同じ。" : "White: current value matches the saved value.");
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.25f, 0.80f, 1.0f, 1.0f));
		ImGui::TextUnformatted(japanese ? "水色: 現在適用済みだが未保存。Saveしない場合は揮発する。" : "Cyan: applied now but not saved; it is volatile until saved.");
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
		ImGui::TextUnformatted(japanese ? "オレンジ: 編集中で、Reset Tank / Applyが必要。" : "Orange: edited and awaiting Reset Tank / Apply.");
		ImGui::PopStyleColor();
		if (ImGui::Button("Reset Tank / Apply##RollingCheat"))
		{
			if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
		}
		ImGui::Separator();

		auto description = [japanese](
			const char* label,
			bool pending,
			const char* english,
			const char* japaneseText)
			{
				if (pending)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.1f, 1.0f));
				}
				ImGui::TextUnformatted(label);
				if (pending)
				{
					ImGui::PopStyleColor();
				}
				ImGui::Indent();
				ImGui::TextWrapped("%s", japanese ? japaneseText : english);
				ImGui::Unindent();
			};

		if (ImGui::CollapsingHeader(japanese ? "開始と折り返し判断" : "Start and return decision", ImGuiTreeNodeFlags_DefaultOpen))
		{
				description(
				"Roll Speed Multiplier", IsPending(ctx.tankSettings->rollSpeedMultiplier, ctx.appliedTankSettings->rollSpeedMultiplier),
				"Scales the rolling motion using the internal coefficients below. 1.0 preserves the reference tuning; 0.5 is slower and 2.0 is faster. It does not change the configured travel target or return decision angle.",
				"下記の内部係数を使ってRolling中の動きを倍率調整します。1.0は基準設定、0.5は遅く、2.0は速くなります。設定した移動目標距離や復帰判断角自体は変わりません。");
			description(
				"Rolling Input", ctx.tankSettings->rollingInputEnabled != ctx.appliedTankSettings->rollingInputEnabled,
				"Enables rolling requests. Use same-direction horizontal input on both levers, or press Q (left roll) / E (right roll). Release the input to neutral before requesting the next roll. When a request is made while moving, the tank brakes and starts the roll after meeting the stop conditions.",
				"Rolling要求を有効にします。左右レバーを同方向へ倒すか、Q（左Rolling）／E（右Rolling）を押します。次のRollingには、一度入力を中立へ戻してください。走行中に要求した場合は制動し、停止条件を満たしてから開始します。");
			description(
				"Roll Torque", IsPending(ctx.tankSettings->rollTorqueNm, ctx.appliedTankSettings->rollTorqueNm),
				"Primary roll torque applied from the start until Torque Cutoff Angle. Higher values make the initial rise faster. Approach Damping is applied separately from Approach Start Angle.",
				"開始から Torque Cutoff Angle まで加える主ロールトルクです。大きくすると立ち上がりが速くなります。Approach Start Angle以降はApproach Dampingも別に作用します。");
			description(
				"Torque Cutoff Angle", IsPending(ctx.tankSettings->rollTorqueCutoffDegrees, ctx.appliedTankSettings->rollTorqueCutoffDegrees),
				"Angle from the starting pose at which the primary Roll Torque above ends (45 to 180 degrees). The roll then continues under the later phase controls; at 180 degrees the primary torque can remain active almost to the inverted attitude.",
				"開始姿勢から測った角度で、直上の主Roll Torqueを終了します（45〜180度）。以降は後段の制御へ移ります。180度では反転姿勢の直前まで主トルクが作用し得ます。");
			description(
				"Return Decision Angle", IsPending(ctx.tankSettings->rollReturnDecisionDegrees, ctx.appliedTankSettings->rollReturnDecisionDegrees),
				"Return Decision Angle: reverse paired levers are accepted at this angle or later and select ReturnToStart. Default: 75 deg.",
				"Return Decision Angle：この角度以降で逆向きの両レバーを受け付け、ReturnToStart を選択します。既定値は75度です。");
		}
		if (ImGui::CollapsingHeader(japanese ? "90度へ近づく区間" : "Approach to 90 degrees", ImGuiTreeNodeFlags_DefaultOpen))
		{
			description(
				"Approach Start Angle", IsPending(ctx.tankSettings->rollApproachStartDegrees, ctx.appliedTankSettings->rollApproachStartDegrees),
				"Approach Start Angle: begins approach damping. Use it to choose where the rise starts to soften before the decision point.",
				"Approach Start Angle：この角度から減衰を開始します。判断点の前で立ち上がりをどこから穏やかにするかを決めます。");
			description(
				"Approach Damping", IsPending(ctx.tankSettings->rollApproachDampingNms, ctx.appliedTankSettings->rollApproachDampingNms),
				"Approach Damping: opposes roll angular velocity from the approach angle to 90 degrees. Higher values reduce overshoot.",
				"Approach Damping：開始角から90度まで、ロール角速度へ逆らう減衰です。高くすると行き過ぎを抑えます。");
			description(
				"Commit Torque", IsPending(ctx.tankSettings->rollCommitTorqueNm, ctx.appliedTankSettings->rollCommitTorqueNm),
				"Commit Torque: a short positive torque in the selected rolling direction after the 90-degree decision. It drives either ContinueForward or ReturnToStart, whichever was selected.",
				"Commit Torque：90度付近の判断後、選択されたRolling方向に対して正方向へ短時間加えるトルクです。ContinueForwardなら前方への回転、ReturnToStartなら開始姿勢への復帰方向へ作用します。");
		}
		if (ImGui::CollapsingHeader(japanese ? "横移動と着地" : "Travel and landing", ImGuiTreeNodeFlags_DefaultOpen))
		{
				description(
				"Roll Travel", IsPending(ctx.tankSettings->rollTravelVehicleWidths, ctx.appliedTankSettings->rollTravelVehicleWidths),
				"When Match Physical Vehicle Width is enabled, target horizontal travel equals the displayed physical vehicle width multiplied by this value (effective range 1.0 to 10.0 vehicle widths).",
				"Match Physical Vehicle WidthがONのとき、表示された物理車幅にこの倍率を掛けた距離を横移動の目標にします。有効範囲は車幅の1.0〜10.0倍です。");
			description(
				"Manual Roll Distance", IsPending(ctx.tankSettings->rollDistanceM, ctx.appliedTankSettings->rollDistanceM),
				"Manual Roll Distance: used only when matching the physical vehicle width is disabled.",
				"Manual Roll Distance：物理車幅への追従をOFFにした場合だけ使う、横移動の直接指定距離です。");
			description(
				"Match Physical Vehicle Width", ctx.tankSettings->rollDistanceMatchesVehicleWidth != ctx.appliedTankSettings->rollDistanceMatchesVehicleWidth,
				"Selects how the roll travel target is calculated. On uses physical vehicle width x Roll Travel; off uses Manual Roll Distance. The inactive distance control is ignored.",
				"Rollingの移動目標距離の決め方を選びます。ONでは物理車幅×Roll Travel、OFFではManual Roll Distanceを使います。選ばれていない側の距離設定は使いません。");
			description(
				"Post-90 Air Brake Torque", IsPending(ctx.tankSettings->rollAirBrakeTorqueNm, ctx.appliedTankSettings->rollAirBrakeTorqueNm),
				"Braking torque (Nm) during the normal forward roll's BallisticRoll phase, after the commit torque. Opposes roll rotation while commanded-direction angular speed exceeds 0.5 rad/s (about 29 deg/s), until the release angle is reached. Higher values slow the fall more; 0 disables this brake. Not used for ReturnToStart. Roll Speed Multiplier also scales the effective torque.",
				"通常の倒れ込みで、Commit Torque 後の BallisticRoll 中に加える回転制動トルク（Nm）です。指示方向の角速度が0.5 rad/s（約29度/秒）を超え、解除角に達する前に回転と逆向きに作用します。大きいほど倒れ込みの回転を強く抑え、0でこの制動を無効にします。ReturnToStart には使いません。実効トルクは Roll Speed Multiplier でも変化します。");
			description(
				"Air Brake Release Angle", IsPending(ctx.tankSettings->rollAirBrakeReleaseDegrees, ctx.appliedTankSettings->rollAirBrakeReleaseDegrees),
				"Remaining angle to the 180-degree inverted target at which the air brake releases, not the angle from the starting pose. For 30 deg, braking ends at 150 deg from the start. Larger values release earlier and leave more rotation unbraked; smaller values brake closer to the final pose. Effective range: 0 to 89 deg. This is an attitude threshold, not a ground-contact check.",
				"180度反転した目標姿勢までの残り角度で、Air Brake を解除する位置を指定します。開始姿勢からの回転角ではありません。30度なら開始から150度で解除します。大きいほど早く解除して残りの回転を制動せず、小さいほど最終姿勢近くまで制動します。実効範囲は0〜89度です。接地検出ではなく姿勢角による判定です。");
		}
		if (ImGui::CollapsingHeader(japanese ? "速度倍率の内部係数" : "Speed multiplier internal coefficients"))
		{
			ImGui::TextWrapped("%s", japanese
				? "各係数はRoll Speed Multiplierが2.0のときの倍率です。1.0では全係数が実効1.0となり基準動作を保ちます。0.5では各係数の平方根の逆数が適用されます。つまり、基準速度から離れたときに各物理要素をどれだけ強く変化させるかを調整します。"
				: "Each coefficient is the scale at Roll Speed Multiplier 2.0. At 1.0 all resolve to an effective scale of 1.0 and preserve reference motion. At 0.5 the inverse square root of each coefficient is applied. They control how strongly each physics element changes away from the reference speed.");
			struct CoefficientHelp
			{
				const char* label;
				const char* english;
				const char* japanese;
			};
			static constexpr CoefficientHelp help[] = {
				{ "Drive torque", "Scales the primary roll torque.", "主Roll Torqueを倍率調整します。" },
				{ "Approach damping", "Scales damping before the 90-degree decision.", "90度判断前のApproach Dampingを倍率調整します。" },
				{ "Commit torque", "Scales the short torque pulse after the roll decision.", "判断後に加える短時間のCommit Torqueを倍率調整します。" },
				{ "Air brake torque", "Scales Post-90 Air Brake Torque during the forward fall.", "前方への倒れ込み中のPost-90 Air Brake Torqueを倍率調整します。" },
				{ "Stabilization torque", "Scales hull attitude correction during the rolling phases, including settling.", "着地後のSettlingを含むRolling各段階で、車体姿勢を補正するトルクを倍率調整します。" },
				{ "Stabilization damping", "Scales damping that opposes roll angular velocity during the rolling phases, including settling.", "着地後のSettlingを含むRolling各段階で、ロール角速度を抑える減衰を倍率調整します。" },
				{ "Travel force (P/I)", "Scales the force driving the tank toward its lateral travel target.", "横移動目標へ車体を動かす力を倍率調整します。" },
				{ "Travel damping", "Scales resistance to lateral velocity while approaching the travel target.", "横移動目標へ近づく間の横速度への減衰を倍率調整します。" },
				{ "Travel force limit", "Scales the maximum allowed lateral travel force.", "横移動に使える力の上限を倍率調整します。" },
				{ "Gravity", "Scales gravity while the rolling motion is active.", "Rolling動作中の重力を倍率調整します。" },
				{ "Suspension frequency", "Scales suspension spring frequency while rolling.", "Rolling中のサスペンションばねの固有振動数を倍率調整します。" },
				{ "Commit duration", "Scales how long the post-decision torque pulse lasts.", "判断後のCommit Torqueを加える時間を倍率調整します。" },
			};
			for (const CoefficientHelp& item : help)
			{
				ImGui::TextUnformatted(item.label);
				ImGui::Indent();
				ImGui::TextWrapped("%s", japanese ? item.japanese : item.english);
				ImGui::Unindent();
			}
		}
		if (ImGui::CollapsingHeader(japanese ? "最終姿勢の安定" : "Final attitude stabilization", ImGuiTreeNodeFlags_DefaultOpen))
		{
			description(
				"Stabilization Torque / Damping",
				IsPending(ctx.tankSettings->rollStabilizationTorqueNm, ctx.appliedTankSettings->rollStabilizationTorqueNm) ||
				IsPending(ctx.tankSettings->rollStabilizationDampingNms, ctx.appliedTankSettings->rollStabilizationDampingNms),
				"Stabilization Torque and Damping: keep the completed result upright or inverted and remove residual roll speed. These do not add travel distance after landing.",
				"Stabilization Torque と Damping：完了姿勢（表または裏）を保ち、残った回転を止めます。着地後に横移動距離を後追い補正するものではありません。");
		}
		ImGui::SetWindowFontScale(1.0f);
		ImGui::End();
	}

	enum class RollingPlotMetric
	{
		RollDegrees,
		MoveX,
		MoveY,
		MoveZ,
	};

	float RollingPlotValue(
		const Tank::Physics::RollingTrajectorySample& sample,
		RollingPlotMetric metric)
	{
		switch (metric)
		{
		case RollingPlotMetric::RollDegrees:
			return 2.0f * std::atan2(sample.rotation.z, sample.rotation.w) *
				180.0f / 3.14159265358979323846f;
		case RollingPlotMetric::MoveX: return sample.movement.x;
		case RollingPlotMetric::MoveY: return sample.movement.y;
		case RollingPlotMetric::MoveZ: return sample.movement.z;
		default: return 0.0f;
		}
	}

	void DrawRollingComparisonPlot(
		const char* label,
		const Tank::Physics::RollingSpeedOptimizationResult& result,
		RollingPlotMetric metric,
		const char* unit,
		bool beforeOptimization)
	{
		const std::array<const Tank::Physics::RollingTrajectory*, 4> trajectories =
			beforeOptimization
			? std::array<const Tank::Physics::RollingTrajectory*, 4> {
				&result.slowBeforeTrajectory, &result.reference,
				&result.intermediateBeforeTrajectory, &result.fastBeforeTrajectory }
			: std::array<const Tank::Physics::RollingTrajectory*, 4> {
				&result.slow, &result.reference, &result.intermediate, &result.optimized };
		constexpr std::array<float, 4> speeds = { 0.5f, 1.0f, 1.5f, 2.0f };
		constexpr std::array<ImU32, 4> colors = {
			IM_COL32(125, 235, 155, 255),
			IM_COL32(90, 190, 255, 255),
			IM_COL32(255, 205, 70, 255),
			IM_COL32(255, 105, 125, 255) };
		if (trajectories[0]->samples.empty() || trajectories[1]->samples.empty() ||
			trajectories[2]->samples.empty() || trajectories[3]->samples.empty())
		{
			return;
		}

		constexpr float dt = 1.0f / 60.0f;
		const float maximumReferenceTime =
			(result.reference.samples.size() - 1) * dt;
		float minimumValue = 0.0f;
		float maximumValue = 0.0f;
		for (size_t series = 0; series < trajectories.size(); ++series)
		{
			const auto& samples = trajectories[series]->samples;
			for (size_t frame = 0; frame < samples.size(); ++frame)
			{
				if (frame * dt * speeds[series] > maximumReferenceTime) break;
				const float value = RollingPlotValue(samples[frame], metric);
				minimumValue = (std::min)(minimumValue, value);
				maximumValue = (std::max)(maximumValue, value);
			}
		}
		if (maximumValue - minimumValue < 0.001f)
		{
			minimumValue -= 0.5f;
			maximumValue += 0.5f;
		}
		const float padding = (maximumValue - minimumValue) * 0.08f;
		minimumValue -= padding;
		maximumValue += padding;

		ImGui::Text("Y axis: %s (%s)", label, unit);
		const ImVec2 size((std::max)(ImGui::GetContentRegionAvail().x, 200.0f), 145.0f);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(label, size);
		const bool plotHovered = ImGui::IsItemHovered();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin,
			{ origin.x + size.x, origin.y + size.y },
			IM_COL32(20, 23, 28, 255));
		for (int grid = 0; grid <= 4; ++grid)
		{
			const float x = origin.x + size.x * grid / 4.0f;
			const float y = origin.y + size.y * grid / 4.0f;
			drawList->AddLine({ x, origin.y }, { x, origin.y + size.y }, IM_COL32(70, 75, 82, 120));
			drawList->AddLine({ origin.x, y }, { origin.x + size.x, y }, IM_COL32(70, 75, 82, 120));
		}
		if (minimumValue <= 0.0f && maximumValue >= 0.0f)
		{
			const float zeroY = origin.y + size.y * maximumValue /
				(maximumValue - minimumValue);
			drawList->AddLine({ origin.x, zeroY }, { origin.x + size.x, zeroY },
				IM_COL32(155, 160, 170, 180));
		}
		for (size_t series = 0; series < trajectories.size(); ++series)
		{
			const auto& samples = trajectories[series]->samples;
			bool hasPrevious = false;
			ImVec2 previous;
			for (size_t frame = 0; frame < samples.size(); ++frame)
			{
				const float referenceTime = frame * dt * speeds[series];
				if (referenceTime > maximumReferenceTime) break;
				const float value = RollingPlotValue(samples[frame], metric);
				const ImVec2 point = {
					origin.x + size.x * referenceTime / maximumReferenceTime,
					origin.y + size.y * (maximumValue - value) /
						(maximumValue - minimumValue) };
				if (hasPrevious) drawList->AddLine(previous, point, colors[series], 2.0f);
				previous = point;
				hasPrevious = true;
			}
		}
		char valueLabel[32] = {};
		std::snprintf(valueLabel, sizeof(valueLabel), "%.2f", maximumValue);
		drawList->AddText({ origin.x + 4.0f, origin.y + 3.0f },
			IM_COL32(195, 200, 210, 255), valueLabel);
		std::snprintf(valueLabel, sizeof(valueLabel), "%.2f", minimumValue);
		drawList->AddText({ origin.x + 4.0f, origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
			IM_COL32(195, 200, 210, 255), valueLabel);
		std::snprintf(valueLabel, sizeof(valueLabel), "%.2f s", maximumReferenceTime);
		drawList->AddText({ origin.x + size.x - ImGui::CalcTextSize(valueLabel).x - 4.0f,
			origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
			IM_COL32(195, 200, 210, 255), valueLabel);

		if (plotHovered)
		{
			const float mouseX = ImGui::GetMousePos().x;
			const float normalizedX = std::clamp(
				(mouseX - origin.x) / size.x, 0.0f, 1.0f);
			const float referenceTime = normalizedX * maximumReferenceTime;
			const float cursorX = origin.x + normalizedX * size.x;
			drawList->AddLine({ cursorX, origin.y },
				{ cursorX, origin.y + size.y }, IM_COL32(235, 235, 235, 190));
			ImGui::BeginTooltip();
			ImGui::Text("X: %.3f s (x1-equivalent)", referenceTime);
			for (size_t series = 0; series < trajectories.size(); ++series)
			{
				const auto& samples = trajectories[series]->samples;
				const size_t frame = (std::min)(
					static_cast<size_t>(std::round(
						referenceTime / (dt * speeds[series]))),
					samples.size() - 1);
				const float actualTime = frame * dt;
				const float value = RollingPlotValue(samples[frame], metric);
				ImGui::TextColored(ImColor(colors[series]),
					"x%.1f: actual %.3f s, Y %.4f %s",
					speeds[series], actualTime, value, unit);
			}
			ImGui::EndTooltip();
		}
		ImGui::TextColored(ImColor(colors[0]), "x0.5"); ImGui::SameLine();
		ImGui::TextColored(ImColor(colors[1]), "x1.0"); ImGui::SameLine();
		ImGui::TextColored(ImColor(colors[2]), "x1.5"); ImGui::SameLine();
		ImGui::TextColored(ImColor(colors[3]), "x2.0"); ImGui::SameLine();
		ImGui::TextDisabled("X axis: x1-equivalent seconds = actual seconds x multiplier");
	}

	DriveSpeedHistory& GetDriveSpeedTelemetry()
	{
		static DriveSpeedHistory telemetry;
		return telemetry;
	}

	DriveSpeedGraphSettings& GetDriveSpeedGraphSettings()
	{
		static DriveSpeedGraphSettings settings;
		return settings;
	}

	ImU32 DriveSpeedStateColor(DriveSpeedState state)
	{
		switch (state)
		{
		case DriveSpeedState::Acceleration: return IM_COL32(80, 220, 120, 255);
		case DriveSpeedState::NaturalBrake: return IM_COL32(255, 205, 70, 255);
		case DriveSpeedState::Brake: return IM_COL32(255, 95, 85, 255);
		default: return IM_COL32(165, 175, 190, 255);
		}
	}

	void UpdateDriveSpeedTelemetry(
		const TrackedVehiclePanelContext& ctx,
		const Tank::Physics::TrackedVehicleTestState& state,
		const Tank::Physics::TrackedDriverInput& driverInput)
	{
		int wheelContactCount = 0;
		for (int i = 0; i < state.wheelCount; ++i)
		{
			wheelContactCount += state.wheels[static_cast<size_t>(i)].hasContact ? 1 : 0;
		}
		DriveTelemetryInput telemetry = {};
		telemetry.sample = {
			state.stepIndex,
			state.timeSeconds,
			state.speedMetersPerSecond,
			ClassifyDriveSpeedState(driverInput.forward, driverInput.brake),
			ctx.analogLeftTrack,
			ctx.analogRightTrack,
			driverInput.forward,
			driverInput.leftRatio,
			driverInput.rightRatio,
			driverInput.brake,
			state.engineRpm,
			state.clutchFriction,
			state.leftTrackAngularVelocityRadians,
			state.rightTrackAngularVelocityRadians,
			state.leftTrackDriveTorqueNm,
			state.rightTrackDriveTorqueNm,
			state.yawSpeedDegrees,
			state.transmissionGear,
			wheelContactCount,
			state.transmissionSwitchingGear,
			state.mobility.state == Tank::Physics::MobilityState::Stopped };
		telemetry.stopped = state.mobility.state == Tank::Physics::MobilityState::Stopped;
		const auto& roll = state.rollingTelemetry;
		auto& sample = telemetry.sample;
		sample.rollLeftLever = roll.leftLeverX;
		sample.rollRightLever = roll.rightLeverX;
		sample.rollInputArmed = roll.inputArmed;
		sample.rollPendingSign = roll.pendingSign;
		sample.rollPhase = static_cast<int>(state.rollingPhase);
		sample.rollStopGate = static_cast<int>(roll.stopGateFailure);
		sample.rollRejectReason = static_cast<int>(state.specialMove.lastRejectReason);
		sample.stopProgress = state.mobility.stopCandidateProgress;
		sample.angularSpeed = state.motionObservation.angularSpeedRadiansPerSecond;
		sample.rollAngleDegrees = roll.angleFromStartDegrees;
		sample.rollAccumulatedAngleDegrees = roll.signedAccumulatedAngleDegrees;
		sample.rollSpeedDegrees = roll.angularSpeedDegrees;
		sample.rollTravelMeters = roll.actualTravelMeters;
		sample.rollTargetMeters = roll.targetTravelMeters;
		for (const auto& track : state.motionObservation.tracks)
		{
			sample.maximumSlip = (std::max)(sample.maximumSlip,
				track.maximumAbsoluteLongitudinalSlipMetersPerSecond);
		sample.maximumSuspensionSpeed = (std::max)(sample.maximumSuspensionSpeed,
				track.maximumAbsoluteSuspensionVelocityMetersPerSecond);
		}
		sample.rollPrimaryTorqueNm = roll.primaryTorqueNm;
		sample.rollApproachDampingTorqueNm = roll.approachDampingTorqueNm;
		sample.rollCommitTorqueNm = roll.commitTorqueNm;
		sample.rollAirBrakeTorqueNm = roll.airBrakeTorqueNm;
		sample.rollStabilizationTorqueNm = roll.stabilizationTorqueNm;
		sample.rollControllerTorqueSumNm = roll.controllerTorqueSumNm;
		sample.rollRunId = roll.runId;
		GetDriveSpeedTelemetry().Update(telemetry);
	}

	void DrawTelemetryGraphValues(
		const char* label,
		float minimum,
		float maximum,
		const std::vector<float>& values,
		const std::vector<DriveSpeedSample>& samples,
		size_t firstSample)
	{
		if (values.empty())
		{
			ImGui::TextDisabled("Waiting for telemetry samples.");
			return;
		}
		const DriveSpeedGraphSettings& settings = GetDriveSpeedGraphSettings();
		const float endTime = samples.back().timeSeconds;
		const float startTime = endTime - settings.historyDurationSeconds;
		const ImVec2 size((std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 72.0f);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::PushID(label);
		ImGui::InvisibleButton("##TelemetryGraph", size);
		ImGui::PopID();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin, { origin.x + size.x, origin.y + size.y },
			IM_COL32(20, 23, 28, 255));
		for (int grid = 0; grid <= 4; ++grid)
		{
			const float x = origin.x + size.x * grid / 4.0f;
			const float y = origin.y + size.y * grid / 4.0f;
			drawList->AddLine({ x, origin.y }, { x, origin.y + size.y }, IM_COL32(70, 75, 82, 120));
			drawList->AddLine({ origin.x, y }, { origin.x + size.x, y }, IM_COL32(70, 75, 82, 120));
		}
		const auto pointFor = [&](size_t index)
		{
			return ImVec2(
				origin.x + size.x * std::clamp(
					(samples[firstSample + index].timeSeconds - startTime) /
						settings.historyDurationSeconds, 0.0f, 1.0f),
				origin.y + size.y * (1.0f - std::clamp(
					(values[index] - minimum) / (std::max)(maximum - minimum, 0.0001f), 0.0f, 1.0f)));
		};
		for (size_t index = 1; index < values.size(); ++index)
		{
			drawList->AddLine(pointFor(index - 1), pointFor(index), IM_COL32(90, 190, 255, 255), 1.5f);
		}
		char scaleLabel[48] = {};
		std::snprintf(scaleLabel, sizeof(scaleLabel), "%s  %.0f..%.0f", label, minimum, maximum);
		drawList->AddText({ origin.x + 4.0f, origin.y + 3.0f }, IM_COL32(215, 220, 230, 255), scaleLabel);
		char durationLabel[16] = {};
		std::snprintf(durationLabel, sizeof(durationLabel), "%.0f s", settings.historyDurationSeconds);
		drawList->AddText({ origin.x + size.x - ImGui::CalcTextSize(durationLabel).x - 4.0f,
			origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
			IM_COL32(195, 200, 210, 255), durationLabel);
		if (ImGui::IsItemHovered())
		{
			const float time = startTime + settings.historyDurationSeconds *
				std::clamp((ImGui::GetIO().MousePos.x - origin.x) / size.x, 0.0f, 1.0f);
			size_t nearest = 0;
			for (size_t i = 1; i < values.size(); ++i)
				if (std::abs(samples[firstSample + i].timeSeconds - time) <
					std::abs(samples[firstSample + nearest].timeSeconds - time)) nearest = i;
			ImGui::SetTooltip("%s: %.3f\nTime: %.3f s", label, values[nearest],
				samples[firstSample + nearest].timeSeconds);
		}
	}

	template <typename Value>
	void DrawTelemetryGraph(
		const char* label,
		float minimum,
		float maximum,
		Value DriveSpeedSample::* value)
	{
		const std::vector<DriveSpeedSample>& samples = GetDriveSpeedTelemetry().Samples();
		if (samples.empty()) return;
		const float startTime = samples.back().timeSeconds -
			GetDriveSpeedGraphSettings().historyDurationSeconds;
		size_t firstSample = 0;
		while (firstSample + 1 < samples.size() && samples[firstSample].timeSeconds < startTime)
			++firstSample;
		std::vector<float> values;
		values.reserve(samples.size() - firstSample);
		for (size_t index = firstSample; index < samples.size(); ++index)
			values.push_back(static_cast<float>(samples[index].*value));
		DrawTelemetryGraphValues(label, minimum, maximum, values, samples, firstSample);
	}

	struct RollingTorqueGraphSettings
	{
		float angleStartDegrees = 0.0f;
		float angleEndDegrees = 180.0f;
		float torqueScaleNm = 250000.0f;
		std::array<bool, 6> visible = { true, true, true, true, true, true };
	};

	RollingTorqueGraphSettings& GetRollingTorqueGraphSettings()
	{
		static RollingTorqueGraphSettings settings;
		return settings;
	}

	void DrawRollingTorqueOverlay(bool angleAxis)
	{
		RollingTorqueGraphSettings& graphSettings =
			GetRollingTorqueGraphSettings();
		static constexpr std::array<float DriveSpeedSample::*, 6> channels = {
			&DriveSpeedSample::rollPrimaryTorqueNm,
			&DriveSpeedSample::rollApproachDampingTorqueNm,
			&DriveSpeedSample::rollCommitTorqueNm,
			&DriveSpeedSample::rollAirBrakeTorqueNm,
			&DriveSpeedSample::rollStabilizationTorqueNm,
			&DriveSpeedSample::rollControllerTorqueSumNm };
		static constexpr std::array<const char*, 6> labels = {
			"Roll", "Approach", "Commit", "Air brake", "Stabilization", "Controller sum" };
		static constexpr std::array<ImU32, 6> colors = {
			IM_COL32(80, 180, 255, 255),
			IM_COL32(255, 170, 60, 255),
			IM_COL32(80, 220, 125, 255),
			IM_COL32(205, 110, 255, 255),
			IM_COL32(50, 220, 220, 255),
			IM_COL32(255, 245, 130, 255) };
		for (size_t i = 0; i < labels.size(); ++i)
		{
			if (i % 3 != 0) ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text,
				ImGui::ColorConvertU32ToFloat4(colors[i]));
			ImGui::Checkbox(labels[i], &graphSettings.visible[i]);
			ImGui::PopStyleColor();
		}
		const std::vector<DriveSpeedSample>& samples = GetDriveSpeedTelemetry().Samples();
		if (samples.empty())
		{
			ImGui::TextDisabled("Waiting for torque samples.");
			return;
		}

		const DriveSpeedGraphSettings& settings = GetDriveSpeedGraphSettings();
		size_t firstSample = 0;
		size_t endSample = samples.size();
		float axisMinimum = 0.0f;
		float axisMaximum = 0.0f;
		if (angleAxis)
		{
			while (endSample > 0 && samples[endSample - 1].rollPhase == 0)
			{
				--endSample;
			}
			if (endSample == 0)
			{
				ImGui::TextDisabled("No rolling run in the current history.");
				return;
			}
			const std::uint64_t latestRunId = samples[endSample - 1].rollRunId;
			while (firstSample < endSample &&
				(samples[firstSample].rollRunId != latestRunId ||
					samples[firstSample].rollPhase == 0))
			{
				++firstSample;
			}
			axisMinimum = graphSettings.angleStartDegrees;
			axisMaximum = graphSettings.angleEndDegrees;
		}
		else
		{
			const float startTime = samples.back().timeSeconds -
				settings.historyDurationSeconds;
			while (firstSample + 1 < endSample &&
				samples[firstSample].timeSeconds < startTime)
			{
				++firstSample;
			}
			axisMinimum = startTime;
			axisMaximum = samples.back().timeSeconds;
		}
		const float torqueLimit = graphSettings.torqueScaleNm;

		const ImVec2 size((std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 144.0f);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##RollingTorqueOverlay", size);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin, { origin.x + size.x, origin.y + size.y },
			IM_COL32(20, 23, 28, 255));
		for (int grid = 0; grid <= 4; ++grid)
		{
			const float x = origin.x + size.x * grid / 4.0f;
			const float y = origin.y + size.y * grid / 4.0f;
			drawList->AddLine({ x, origin.y }, { x, origin.y + size.y },
				IM_COL32(70, 75, 82, 120));
			drawList->AddLine({ origin.x, y }, { origin.x + size.x, y },
				grid == 2 ? IM_COL32(150, 155, 165, 200) : IM_COL32(70, 75, 82, 120));
		}
		const auto pointFor = [&](size_t sampleIndex, float value)
		{
			return ImVec2(
				origin.x + size.x * std::clamp(
				((angleAxis ? samples[sampleIndex].rollAccumulatedAngleDegrees :
					samples[sampleIndex].timeSeconds) - axisMinimum) /
						(std::max)(axisMaximum - axisMinimum, 0.0001f),
					0.0f, 1.0f),
				origin.y + size.y * std::clamp(
					0.5f - 0.5f * value / torqueLimit, 0.0f, 1.0f));
		};
		for (size_t channelIndex = 0; channelIndex < channels.size(); ++channelIndex)
		{
			if (!graphSettings.visible[channelIndex]) continue;
			const auto channel = channels[channelIndex];
			for (size_t i = firstSample + 1; i < endSample; ++i)
			{
				if (angleAxis)
				{
					const float previousAngle = samples[i - 1].rollAccumulatedAngleDegrees;
					const float currentAngle = samples[i].rollAccumulatedAngleDegrees;
					if ((previousAngle < axisMinimum && currentAngle < axisMinimum) ||
						(previousAngle > axisMaximum && currentAngle > axisMaximum))
					{
						continue;
					}
				}
				drawList->AddLine(
					pointFor(i - 1, samples[i - 1].*channel),
					pointFor(i, samples[i].*channel),
					colors[channelIndex],
					channelIndex == 5 ? 2.5f : 1.7f);
			}
		}
		char scaleLabel[64] = {};
		std::snprintf(scaleLabel, sizeof(scaleLabel),
			"Roll torque (Nm)  +/-%.0f", torqueLimit);
		drawList->AddText({ origin.x + 4.0f, origin.y + 3.0f },
			IM_COL32(215, 220, 230, 255), scaleLabel);
		char axisLabel[64] = {};
		std::snprintf(axisLabel, sizeof(axisLabel), "%s  %.0f..%.0f",
			angleAxis ? "Angle from start (deg)" : "Time (s)",
			axisMinimum, axisMaximum);
		drawList->AddText({ origin.x + 4.0f,
				origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
			IM_COL32(195, 200, 210, 255), axisLabel);
		char durationLabel[16] = {};
		if (!angleAxis)
		{
			std::snprintf(durationLabel, sizeof(durationLabel), "%.0f s",
				settings.historyDurationSeconds);
			drawList->AddText(
				{ origin.x + size.x - ImGui::CalcTextSize(durationLabel).x - 4.0f,
					origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
				IM_COL32(195, 200, 210, 255), durationLabel);
		}
		if (ImGui::IsItemHovered())
		{
			const float axisValue = axisMinimum + (axisMaximum - axisMinimum) *
				std::clamp((ImGui::GetIO().MousePos.x - origin.x) / size.x, 0.0f, 1.0f);
			size_t nearest = firstSample;
			for (size_t i = firstSample + 1; i < endSample; ++i)
			{
				const float sampleAxis = angleAxis
					? samples[i].rollAccumulatedAngleDegrees : samples[i].timeSeconds;
				const float nearestAxis = angleAxis
					? samples[nearest].rollAccumulatedAngleDegrees : samples[nearest].timeSeconds;
				if (std::abs(sampleAxis - axisValue) <
					std::abs(nearestAxis - axisValue))
				{
					nearest = i;
				}
			}
			const DriveSpeedSample& sample = samples[nearest];
			ImGui::BeginTooltip();
			ImGui::Text("%s %.3f  Time %.3f s  Phase %d",
				angleAxis ? "Angle" : "Time",
				angleAxis ? sample.rollAccumulatedAngleDegrees : sample.timeSeconds,
				sample.timeSeconds, sample.rollPhase);
			for (size_t i = 0; i < channels.size(); ++i)
			{
				if (graphSettings.visible[i])
				{
					ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colors[i]),
						"%s %+.0f Nm", labels[i], sample.*channels[i]);
				}
			}
			ImGui::EndTooltip();
		}
		ImGui::Spacing();
		ImGui::TextWrapped("Signed components about the tank's local forward roll axis. Controller sum includes these explicit controller torques; ground-contact and gravity moments are not included.");
	}

	void DrawDriveSpeedGraph()
	{
		DriveSpeedGraphSettings& settings = GetDriveSpeedGraphSettings();
		ImGui::SliderFloat(
			"Time Window##DriveSpeedGraph",
			&settings.historyDurationSeconds,
			1.0f,
			60.0f,
			"%.0f s",
			ImGuiSliderFlags_AlwaysClamp);
		ImGui::SliderFloat(
			"Speed Scale##DriveSpeedGraph",
			&settings.maximumSpeedMetersPerSecond,
			1.0f,
			200.0f,
			"%.0f m/s",
			ImGuiSliderFlags_AlwaysClamp);
		const std::vector<DriveSpeedSample>& samples =
			GetDriveSpeedTelemetry().Samples();
		const float historyDurationSeconds = settings.historyDurationSeconds;
		const float maximumGraphSpeedMetersPerSecond = settings.maximumSpeedMetersPerSecond;
		ImGui::Text("Speed history (last %.0f s, 0-%.0f m/s)",
			historyDurationSeconds, maximumGraphSpeedMetersPerSecond);
		if (samples.size() < 2)
		{
			ImGui::TextDisabled("Waiting for speed samples.");
			return;
		}

		const float endTime = samples.back().timeSeconds;
		const float startTime = endTime - historyDurationSeconds;
		const ImVec2 size((std::max)(ImGui::GetContentRegionAvail().x, 200.0f), 120.0f);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##DriveSpeedGraph", size);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin,
			{ origin.x + size.x, origin.y + size.y },
			IM_COL32(20, 23, 28, 255));
		for (int grid = 0; grid <= 4; ++grid)
		{
			const float x = origin.x + size.x * grid / 4.0f;
			const float y = origin.y + size.y * grid / 4.0f;
			drawList->AddLine({ x, origin.y }, { x, origin.y + size.y },
				IM_COL32(70, 75, 82, 120));
			drawList->AddLine({ origin.x, y }, { origin.x + size.x, y },
				IM_COL32(70, 75, 82, 120));
		}
		const auto pointFor = [&](const DriveSpeedSample& sample)
		{
			return ImVec2(
				origin.x + size.x * std::clamp(
					(sample.timeSeconds - startTime) / historyDurationSeconds,
					0.0f, 1.0f),
				origin.y + size.y * (1.0f - std::clamp(
					sample.speedMetersPerSecond / maximumGraphSpeedMetersPerSecond,
					0.0f, 1.0f)));
		};
		for (size_t index = 1; index < samples.size(); ++index)
		{
			drawList->AddLine(pointFor(samples[index - 1]), pointFor(samples[index]),
				DriveSpeedStateColor(samples[index].state), 2.0f);
		}
		char maximumLabel[32] = {};
		std::snprintf(maximumLabel, sizeof(maximumLabel), "%.0f m/s", maximumGraphSpeedMetersPerSecond);
		drawList->AddText({ origin.x + 4.0f, origin.y + 3.0f },
			IM_COL32(195, 200, 210, 255), maximumLabel);
		char durationLabel[32] = {};
		std::snprintf(durationLabel, sizeof(durationLabel), "%.0f s", historyDurationSeconds);
		drawList->AddText(
			{ origin.x + size.x - ImGui::CalcTextSize(durationLabel).x - 4.0f,
				origin.y + size.y - ImGui::GetTextLineHeight() - 3.0f },
			IM_COL32(195, 200, 210, 255), durationLabel);
		ImGui::TextColored(ImColor(DriveSpeedStateColor(DriveSpeedState::Acceleration)), "Acceleration");
		ImGui::SameLine();
		ImGui::TextColored(ImColor(DriveSpeedStateColor(DriveSpeedState::NaturalBrake)), "Natural Brake");
		ImGui::SameLine();
		ImGui::TextColored(ImColor(DriveSpeedStateColor(DriveSpeedState::Brake)), "Brake");
		ImGui::SameLine();
		ImGui::TextColored(ImColor(DriveSpeedStateColor(DriveSpeedState::Coast)), "Coast");
	}

	void UpdateAndDrawRollingTravelTelemetry(
		const TrackedVehiclePanelContext& ctx,
		const Tank::Physics::TrackedVehicleTestState& state)
	{
        if (ImGui::CollapsingHeader("Frame Timing"))
        {
            ImGui::Text("Total CPU: %.2f ms  peak %.2f ms",
                ctx.cpuFrameTimeMs,
                ctx.peakCpuFrameTimeMs);
            ImGui::Text("Rolling 300: avg %.2f  p95 %.2f  p99 %.2f ms",
                ctx.averageCpuFrameTimeMs,
                ctx.p95CpuFrameTimeMs,
                ctx.p99CpuFrameTimeMs);
            ImGui::Text("Physics: %.2f ms  peak %.2f ms",
                ctx.physicsStepTimeMs,
                ctx.physicsStepPeakTimeMs);
            ImGui::Text("Scene Update: %.2f ms  peak %.2f ms",
                ctx.sceneUpdateTimeMs,
                ctx.sceneUpdatePeakTimeMs);
            if (ImGui::Button("Reset Timing Peaks") && ctx.resetFrameTimingPeaks)
            {
                ctx.resetFrameTimingPeaks();
            }
        }

		const Tank::Physics::TrackedDriverInput& driverInput = *ctx.driverInput;
		UpdateDriveSpeedTelemetry(ctx, state, driverInput);
		if (ctx.telemetryWindowVisible != nullptr && *ctx.telemetryWindowVisible)
        {
			ImGui::SetNextWindowSize(ImVec2(560.0f, 620.0f), ImGuiCond_FirstUseEver);
			if (!ImGui::Begin("Vehicle Telemetry", ctx.telemetryWindowVisible))
			{
				ImGui::End();
			}
			else
			{
            if (ImGui::CollapsingHeader("Rolling"))
            {
                const auto& roll = state.rollingTelemetry;
                ImGui::Text("Phase: %s  Input: %s", RollingPhaseName(state.rollingPhase),
                    roll.inputArmed ? "armed" : "release both levers to rearm");
                ImGui::Text("Pending: %+.0f  Braking: %s  Wait: %.2f s",
                    roll.pendingSign, roll.brakingToStart ? "yes" : "no", roll.waitingSeconds);
                ImGui::Text("Stop gate: %s  Confirmation: %.0f%%",
                    MobilityReasonName(roll.stopGateFailure), state.mobility.stopCandidateProgress * 100.0f);
                ImGui::SliderFloat("Time Window##Rolling", &GetDriveSpeedGraphSettings().historyDurationSeconds,
                    5.0f, 60.0f, "%.0f s");
                if (ImGui::BeginTabBar("RollingDiagnostics"))
                {
                    if (ImGui::BeginTabItem("Input / Start"))
                    {
                        ImGui::TextWrapped("Pair threshold: +/-0.70. Release both to +/-0.20 to rearm.");
                        DrawTelemetryGraph("Left lever X", -1.0f, 1.0f, &DriveSpeedSample::rollLeftLever);
                        DrawTelemetryGraph("Right lever X", -1.0f, 1.0f, &DriveSpeedSample::rollRightLever);
                        DrawTelemetryGraph("Input armed", 0.0f, 1.0f, &DriveSpeedSample::rollInputArmed);
                        DrawTelemetryGraph("Pending roll sign", -1.0f, 1.0f, &DriveSpeedSample::rollPendingSign);
                        DrawTelemetryGraph("Brake", 0.0f, 1.0f, &DriveSpeedSample::brakeInput);
                        DrawTelemetryGraph("Phase", 0.0f, 6.0f, &DriveSpeedSample::rollPhase);
                        DrawTelemetryGraph("Request rejection", 0.0f, 4.0f, &DriveSpeedSample::rollRejectReason);
                        ImGui::TextWrapped("Reject: 0=None, 1=Not stopped, 2=Already active, 3=Invalid, 4=Blocked");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Stop gate"))
                    {
                        const auto& settings = ctx.appliedTankSettings != nullptr
                            ? *ctx.appliedTankSettings : *ctx.tankSettings;
                        ImGui::TextWrapped("Start limits: speed < %.2f m/s, angular < %.2f rad/s, slip < %.2f m/s, suspension < %.2f m/s; confirm %.2f s.",
                            settings.stoppedEnterLinearSpeedMetersPerSecond,
                            settings.stoppedEnterAngularSpeedRadiansPerSecond,
                            settings.stoppedEnterTrackSlipMetersPerSecond,
                            settings.stoppedEnterSuspensionSpeedMetersPerSecond, settings.stoppedConfirmSeconds);
                        DrawTelemetryGraph("Speed m/s", 0.0f, 10.0f, &DriveSpeedSample::speedMetersPerSecond);
                        DrawTelemetryGraph("Angular speed rad/s", 0.0f, 2.0f, &DriveSpeedSample::angularSpeed);
                        DrawTelemetryGraph("Max slip m/s", 0.0f, 2.0f, &DriveSpeedSample::maximumSlip);
                        DrawTelemetryGraph("Max suspension m/s", 0.0f, 1.0f, &DriveSpeedSample::maximumSuspensionSpeed);
                        DrawTelemetryGraph("Stop confirmation", 0.0f, 1.0f, &DriveSpeedSample::stopProgress);
                        DrawTelemetryGraph("Stop gate reason", 0.0f, 10.0f, &DriveSpeedSample::rollStopGate);
                        ImGui::TextWrapped("0=None, 3=Drive, 4=Speed, 5=Angular, 6=Slip, 7=Suspension, 8=Contact, 9=Pose, 10=Invalid");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Motion / Travel"))
                    {
                        ImGui::Text("Rolling Travel: %.2f / %.2f m", roll.actualTravelMeters, roll.targetTravelMeters);
                        ImGui::TextWrapped("Travel is horizontal displacement from roll start, not path length. Angle is unsigned from start up-vector.");
                        float maximumTravel = 1.0f;
                        for (const auto& sample : GetDriveSpeedTelemetry().Samples())
                            maximumTravel = (std::max)(maximumTravel,
                                (std::max)(sample.rollTravelMeters, sample.rollTargetMeters) * 1.1f);
                        DrawTelemetryGraph("Actual travel m", 0.0f, maximumTravel, &DriveSpeedSample::rollTravelMeters);
                        DrawTelemetryGraph("Target travel m", 0.0f, maximumTravel, &DriveSpeedSample::rollTargetMeters);
                        DrawTelemetryGraph("Roll angle deg", 0.0f, 180.0f, &DriveSpeedSample::rollAngleDegrees);
                        DrawTelemetryGraph("Roll speed deg/s", -720.0f, 720.0f, &DriveSpeedSample::rollSpeedDegrees);
                        DrawTelemetryGraph("Phase", 0.0f, 6.0f, &DriveSpeedSample::rollPhase);
                        ImGui::TextWrapped("0=None, 1=Windup, 2=Powered, 3=Evaluating, 4=Commit, 5=Ballistic, 6=Settling");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Torque"))
                    {
                        static bool torqueAngleAxis = false;
                        auto& graphSettings = GetRollingTorqueGraphSettings();
                        if (ImGui::RadioButton("Time", !torqueAngleAxis))
                            torqueAngleAxis = false;
                        ImGui::SameLine();
                        if (ImGui::RadioButton("Roll angle", torqueAngleAxis))
                            torqueAngleAxis = true;
                        ImGui::SetNextItemWidth(190.0f);
                        ImGui::SliderFloat("Torque Range (+/- Nm)",
                            &graphSettings.torqueScaleNm, 1000.0f, 1000000.0f,
                            "%.0f Nm", ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
                        if (torqueAngleAxis)
                        {
                            graphSettings.angleStartDegrees = std::clamp(
                                graphSettings.angleStartDegrees, 0.0f, 359.0f);
                            graphSettings.angleEndDegrees = std::clamp(
                                graphSettings.angleEndDegrees,
                                graphSettings.angleStartDegrees + 1.0f, 360.0f);
                            ImGui::SetNextItemWidth(190.0f);
                            ImGui::SliderFloat("Start (deg)##TorqueAngleStart",
                                &graphSettings.angleStartDegrees, 0.0f, 359.0f, "%.0f deg");
                            ImGui::SetNextItemWidth(190.0f);
                            ImGui::SliderFloat("End (deg)##TorqueAngleEnd",
                                &graphSettings.angleEndDegrees,
                                (std::min)(graphSettings.angleStartDegrees + 1.0f, 360.0f),
                                360.0f, "%.0f deg");
                            if (graphSettings.angleEndDegrees <=
                                graphSettings.angleStartDegrees)
                            {
                                graphSettings.angleEndDegrees =
                                    (std::min)(graphSettings.angleStartDegrees + 1.0f, 360.0f);
                            }
                        }
                        ImGui::TextWrapped("All colored components and their signed sum share the fixed +/- Nm range above. Values outside it stop at the graph edge; hover for actual values. Angle view uses signed accumulated rotation and continues past 180 degrees; a return roll traces back toward 0. The sum includes explicit tank-controller roll-axis torques, not ground-contact or gravity moments.");
                        DrawRollingTorqueOverlay(torqueAngleAxis);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            const Tank::Input::GamepadState& gamepadState = ctx.gamepadState;
            int wheelContactCount = 0;
            for (int i = 0; i < state.wheelCount; ++i)
            {
                wheelContactCount += state.wheels[static_cast<size_t>(i)].hasContact ? 1 : 0;
            }
			if (ImGui::CollapsingHeader("Speed", ImGuiTreeNodeFlags_DefaultOpen))
			{
            ImGui::Text("Step: %d  Time: %.2f s", state.stepIndex, state.timeSeconds);
            ImGui::Text("Position: %.2f, %.2f, %.2f",
                state.bodyPosition.x, state.bodyPosition.y, state.bodyPosition.z);
            
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
				DrawDriveSpeedGraph();
			}
			if (ImGui::CollapsingHeader("Powertrain", ImGuiTreeNodeFlags_DefaultOpen))
			{
            ImGui::Text("Engine: %.0f rpm", state.engineRpm); ImGui::SameLine();
            ImGui::Text("Gear: %d", state.transmissionGear); ImGui::SameLine();
				ImGui::Text("Clutch: %.0f%%  Switching: %s", state.clutchFriction * 100.0f,
					state.transmissionSwitchingGear ? "yes" : "no");
				ImGui::Text("Track torque estimate: L %+.0f  R %+.0f Nm",
					state.leftTrackDriveTorqueNm, state.rightTrackDriveTorqueNm);
				DrawTelemetryGraph("Engine RPM", 0.0f, 6000.0f,
					&DriveSpeedSample::engineRpm);
				DrawTelemetryGraph("Gear", -2.0f, 5.0f,
					&DriveSpeedSample::gear);
				DrawTelemetryGraph("Clutch", 0.0f, 1.0f,
					&DriveSpeedSample::clutchFriction);
				DrawTelemetryGraph("L drive torque est.", -40000.0f, 40000.0f,
					&DriveSpeedSample::leftTrackDriveTorqueNm);
				DrawTelemetryGraph("R drive torque est.", -40000.0f, 40000.0f,
					&DriveSpeedSample::rightTrackDriveTorqueNm);
				DrawTelemetryGraph("Shift active", 0.0f, 1.0f,
					&DriveSpeedSample::switchingGear);
			}
			if (ImGui::CollapsingHeader("Input"))
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
			ImGui::Text(
                "SetDriverInput: Fwd %.2f  L %.2f  R %.2f  Brake %.2f",
                driverInput.forward,
                driverInput.leftRatio,
                driverInput.rightRatio,
                driverInput.brake);
            if (ctx.inputDeviceProfile != nullptr)
            {
                const std::uint32_t brakeButton = ctx.inputDeviceProfile->brakeButton;
                ImGui::Text("Gamepad Brake: %s  (profile raw button %u)",
                    gamepadState.IsRawButtonPressed(brakeButton) ? "On" : "Off",
                    brakeButton);
            }
            else if (gamepadState.hasGamepadMapping)
            {
                ImGui::Text("Gamepad Brake: %s  (standard mapping)",
                    gamepadState.brakePressed ? "On" : "Off");
            }
			else
			{
				ImGui::TextUnformatted("Gamepad Brake: no input-device profile");
			}
				DrawTelemetryGraph("Analog left track", -1.0f, 1.0f,
					&DriveSpeedSample::analogLeftTrack);
				DrawTelemetryGraph("Analog right track", -1.0f, 1.0f,
					&DriveSpeedSample::analogRightTrack);
				DrawTelemetryGraph("SetDriverInput forward", -1.0f, 1.0f,
					&DriveSpeedSample::forwardInput);
				DrawTelemetryGraph("SetDriverInput left ratio", -1.0f, 1.0f,
					&DriveSpeedSample::leftRatio);
				DrawTelemetryGraph("SetDriverInput right ratio", -1.0f, 1.0f,
					&DriveSpeedSample::rightRatio);
				DrawTelemetryGraph("SetDriverInput brake", 0.0f, 1.0f,
					&DriveSpeedSample::brakeInput);
			}
			if (ImGui::CollapsingHeader("Tracks"))
			{
                ImGui::Text("Wheel contacts: %d / %d  Is Jolt sleeping: %s",
                    wheelContactCount, state.wheelCount, state.sleeping ? "yes" : "no");
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
			    ImGui::Text("Left : contact %d  drive %.1f Ns  lateral %.1f Ns  slip %.2f m/s",
                    contacts[0], longitudinalImpulse[0], lateralImpulse[0], slipSpeed[0]);
                ImGui::Text("Right: contact %d  drive %.1f Ns  lateral %.1f Ns  slip %.2f m/s",
                    contacts[1], longitudinalImpulse[1], lateralImpulse[1], slipSpeed[1]);
                if (ImGui::TreeNode("Suspension per Wheel"))
                {
                    if (ImGui::BeginTable(
                        "SuspensionTelemetry",
                        8,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_SizingFixedFit))
                    {
                        ImGui::TableSetupColumn("Wheel");
                        ImGui::TableSetupColumn("Contact");
                        ImGui::TableSetupColumn("Length");
                        ImGui::TableSetupColumn("Range");
                        ImGui::TableSetupColumn("Used");
                        ImGui::TableSetupColumn("Velocity");
                        ImGui::TableSetupColumn("Hard");
                        ImGui::TableSetupColumn("Impulse");
                        ImGui::TableHeadersRow();
                        const int wheelsPerSurface = ctx.tankSettings->roadWheelCount + 2;
                        for (int i = 0; i < state.wheelCount; ++i)
                        {
                            const Tank::Physics::TrackedWheelState& wheel =
                                state.wheels[static_cast<size_t>(i)];
                            const int wheelOnSurface = wheel.wheelIndex % wheelsPerSurface;
                            const bool endWheel = wheelOnSurface == 0 ||
                                wheelOnSurface == wheelsPerSurface - 1;
                            const char* positionName = wheelOnSurface == 0
                                ? "Front"
                                : (wheelOnSurface == wheelsPerSurface - 1 ? "Rear" : nullptr);
                            const std::string wheelName = std::string(wheel.trackIndex == 0 ? "L" : "R") +
                                (wheel.upperSurface ? "-U-" : "-L-") +
                                (positionName != nullptr
                                    ? positionName
                                    : "Road" + std::to_string(wheelOnSurface));
                            const float stroke =
                                wheel.suspensionMaxLength - wheel.suspensionMinLength;
                            const float used = stroke > 0.0001f
                                ? std::clamp(
                                    (wheel.suspensionLength - wheel.suspensionMinLength) / stroke,
                                    0.0f,
                                    1.0f)
                                : 0.0f;

                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(wheelName.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextUnformatted(wheel.hasContact ? "yes" : "no");
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%.3f m", wheel.suspensionLength);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%.3f-%.3f", wheel.suspensionMinLength, wheel.suspensionMaxLength);
                            ImGui::TableSetColumnIndex(4);
                            if (endWheel && stroke <= 0.0001f)
                            {
                                ImGui::TextUnformatted("fixed");
                            }
                            else
                            {
                                ImGui::Text("%.0f%%", used * 100.0f);
                            }
                            ImGui::TableSetColumnIndex(5);
                            ImGui::Text("%+.2f m/s", wheel.suspensionVelocityMetersPerSecond);
                            ImGui::TableSetColumnIndex(6);
                            ImGui::TextUnformatted(wheel.suspensionAtHardPoint ? "yes" : "no");
                            ImGui::TableSetColumnIndex(7);
                            ImGui::Text("%.1f Ns", wheel.suspensionImpulseNewtonSeconds);
                        }
                        ImGui::EndTable();
                    }
				    ImGui::TreePop();
			    }
				DrawTelemetryGraph("Left track angular velocity", -150.0f, 150.0f,
					&DriveSpeedSample::leftTrackAngularVelocityRadians);
				DrawTelemetryGraph("Right track angular velocity", -150.0f, 150.0f,
					&DriveSpeedSample::rightTrackAngularVelocityRadians);
				DrawTelemetryGraph("Yaw speed", -180.0f, 180.0f,
					&DriveSpeedSample::yawSpeedDegrees);
				DrawTelemetryGraph("Wheel contacts", 0.0f,
					static_cast<float>(state.wheelCount), &DriveSpeedSample::wheelContactCount);
				DrawTelemetryGraph("Stopped", 0.0f, 1.0f, &DriveSpeedSample::stopped);
			}
				ImGui::End();
			}
		}


	}

	void DrawTrackedVehiclePanel(TrackedVehiclePanelContext& ctx)
	{
		if (ctx.rollingOptimizer)
		{
			ctx.rollingOptimizer->Poll();
			if (ctx.tankSettings && ctx.envSettings)
			{
				// This updates editable internal coefficients only. Speed selection
				// and Reset Tank remain explicit user actions.
				ctx.rollingOptimizer->Apply(
					*ctx.tankSettings, *ctx.envSettings);
			}
		}
		const Tank::Physics::TrackedVehicleTestState& state = *ctx.state;
		const Tank::Input::GamepadState& gamepadState = ctx.gamepadState;
		ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Tracked Vehicle");

		ImGui::Text("Frame: %.1f ms", ctx.cpuFrameTimeMs);

		if (ImGui::CollapsingHeader(
			"Sub Windows", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ctx.cameraWindowVisible != nullptr)
			{
				ImGui::Checkbox("Camera##SubWindowOnOff", ctx.cameraWindowVisible);
			}
			ImGui::SameLine();
			if (ctx.telemetryWindowVisible != nullptr)
			{
				ImGui::Checkbox("Vehicle Telemetry##SubWindowOnOff", ctx.telemetryWindowVisible);
			}
			if (ctx.rollingParametersWindowVisible != nullptr)
			{
				ImGui::Checkbox("Rolling Parameters##SubWindowOnOff", ctx.rollingParametersWindowVisible);
			}
			ImGui::SameLine();
			if (ctx.renderSettingsWindowVisible != nullptr)
			{
				ImGui::Checkbox("Render Settings##SubWindowOnOff", ctx.renderSettingsWindowVisible);
			}
			ImGui::SameLine();
			if (ctx.gamepadInputWindowVisible != nullptr)
			{
				ImGui::Checkbox("Gamepad & Input##SubWindowOnOff", ctx.gamepadInputWindowVisible);
			}
			ImGui::SameLine();
			if (ctx.outputWindowVisible != nullptr)
			{
				ImGui::Checkbox("Output##SubWindowOnOff", ctx.outputWindowVisible);
			}
		}
		if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button(
				*ctx.trackedVehiclePaused ? "Resume [P]" : "Pause [P]"))
			{
				*ctx.trackedVehiclePaused = !*ctx.trackedVehiclePaused;
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!*ctx.trackedVehiclePaused);
			if (ImGui::Button("Step 1 Frame [F]"))
			{
				*ctx.trackedVehicleSingleStep = true;
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Fire Assault [Space]"))
			{
				if (ctx.fireAssault) ctx.fireAssault();
			}
			ImGui::SameLine();
			if (ImGui::Button("Apply Recoil"))
			{
				if (ctx.fireRecoil) ctx.fireRecoil();
			}
			ImGui::TextDisabled("P: pause/resume   F: advance one frame while paused");
		}

		DrawStateSummary(ctx, state);

		UpdateAndDrawRollingTravelTelemetry(ctx, state);

        ImGui::BeginDisabled(!ctx.manifestMapActive);
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
            if (ctx.manifestMapActive && ctx.mapHitMeshOverlay != nullptr)
            {
                if (ctx.mapVisualMeshes != nullptr &&
                    ImGui::Checkbox("Show Visual Meshes", ctx.mapVisualMeshes))
                {
                    if (ctx.updateScene) ctx.updateScene();
                }
                if (ctx.mapMarkersVisible != nullptr &&
                    ImGui::Checkbox("Show Start / Goal Markers", ctx.mapMarkersVisible))
                {
                    if (ctx.updateScene) ctx.updateScene();
                }
                ImGui::TextUnformatted("Orange: start  Cyan: clear-area AABB");
                if (ImGui::Checkbox("Show HitMesh Overlay", ctx.mapHitMeshOverlay))
                {
                    if (ctx.updateScene) ctx.updateScene();
                }
                ImGui::TextUnformatted("Magenta: collision mesh used by physics");
                if (ctx.manifestMapMissingVisuals)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.72f, 0.2f, 1.0f));
                    ImGui::TextWrapped("Warning: Visual Mesh missing. Enable Hit Mesh display if needed.");
                    ImGui::PopStyleColor();
                }
                if (ctx.mapCleared && ctx.clearedAreaName != nullptr)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
                    ImGui::Text("MAP CLEAR: %s", ctx.clearedAreaName->c_str());
                    ImGui::PopStyleColor();
                }
                else if (ctx.manifestMapHasClearAreas)
                {
                    ImGui::TextUnformatted("Drive the tank center into a clear-area AABB.");
                }
            }
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(ctx.manifestMapActive);
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
            ColorEdit3WithPendingColor(
                "Ground Color",
                &ctx.envSettings->groundColor.r,
                IsPendingColor(ctx.envSettings->groundColor, ctx.appliedEnvSettings->groundColor));
            ColorEdit3WithPendingColor(
                "Grid Line Color",
                &ctx.envSettings->gridLineColor.r,
                IsPendingColor(ctx.envSettings->gridLineColor, ctx.appliedEnvSettings->gridLineColor));
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
        ImGui::EndDisabled();

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
		ImGui::SameLine();
		if (ImGui::Button("Reset Tank"))
		{
			if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
		}
		if (ctx.tankSettingsStatus && !ctx.tankSettingsStatus->empty())
		{
			ImGui::TextWrapped("%s", ctx.tankSettingsStatus->c_str());
		}
		ImGui::TextDisabled("Press ESC to return to the top menu.");

		ImGui::BeginChild(
			"TrackedVehicleControls",
			ImVec2(0.0f, 0.0f),
			ImGuiChildFlags_None);
		if (ctx.tankSettings && ImGui::CollapsingHeader("Assault Projectiles"))
		{
			auto& settings = ctx.tankSettings->assaultProjectiles;
			ImGui::SliderInt("Maximum simultaneous rounds", &settings.maximumCount, 0, 1024, "%d", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Bullet speed (m/s)", &settings.speedMetersPerSecond, 1.0f, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Damage per round", &settings.damagePerRound, 1.0f, 0.0f, 1000000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			bool infinite = settings.lifetimeSeconds <= 0.0f;
			if (ImGui::Checkbox("Infinite lifetime", &infinite))
				settings.lifetimeSeconds = infinite ? 0.0f : 5.0f;
			if (!infinite)
				ImGui::DragFloat("Lifetime (seconds)", &settings.lifetimeSeconds, 0.1f, 0.01f, 86400.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			if (ctx.state) ImGui::Text("Active rounds: %zu / %d", ctx.state->assaultProjectiles.size(), settings.maximumCount);
			ImGui::Checkbox("Expire at maximum distance", &settings.expireAtMaximumDistance);
			if (settings.expireAtMaximumDistance)
				ImGui::DragFloat("Maximum distance (m)", &settings.maximumDistanceMeters, 1.0f, 0.1f, 1000000.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::TextWrapped("Distance expiry is independent of infinite lifetime. Distance settings affect new rounds.");
			ImGui::DragFloat3("Muzzle local position (m)",
				&settings.muzzleLocalPosition.x, 0.01f, -100.0f, 100.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp);
			ImGui::TextDisabled("Tank local: +X right, +Y up, +Z forward. Saved with Tank Settings.");
			ImGui::SliderInt("Maximum static surface impact marks", &settings.maximumImpactMarks, 0, 1024, "%d", ImGuiSliderFlags_AlwaysClamp);
			if (ctx.state) ImGui::Text("Static surface marks: %zu / %d", ctx.state->assaultImpactMarks.Count(), settings.maximumImpactMarks);
			ImGui::TextWrapped("Marks apply to ground and static Map surfaces, overwrite oldest entries, and are cleared by Reset. Zero disables marks.");
			ImGui::TextWrapped("Applied live. Speed, damage and lifetime affect new rounds. At capacity, firing stops. Reducing capacity removes oldest rounds.");
		}



        if (ctx.gamepadInputWindowVisible != nullptr && *ctx.gamepadInputWindowVisible)
        {
            ImGui::SetNextWindowSize(ImVec2(440.0f, 640.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Gamepad & Input", ctx.gamepadInputWindowVisible))
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
                    if (gamepadState.axisCount > 0)
                    {
                        ImGui::TextUnformatted("Raw controller axes:");
                        for (std::uint32_t axis = 0;
                            axis < gamepadState.axisCount && axis < gamepadState.rawAxes.size();
                            ++axis)
                        {
                            ImGui::Text("Axis %u: %.3f", axis, gamepadState.rawAxes[axis]);
                        }
                    }
                    if (gamepadState.buttonCount > 0)
                    {
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
                    }
                    if (gamepadState.switchCount > 0)
                    {
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
                }
                DrawLeverInputMapping(ctx);
            }
            ImGui::End();
        }


		if (ImGui::CollapsingHeader("Tank Physics Settings"))
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
				0.30f,
				"%.2f",
				false);

		}

        if (ImGui::CollapsingHeader("Mortar Parameters"))
        {
            if (ctx.mortarProfileSlot)
            {
                ImGui::TextUnformatted("Mortar Profile Slot");
                for (int slot = 0; slot < 3; ++slot)
                {
                    const std::string label = "Slot " + std::to_string(slot + 1) + "##MortarProfile";
                    if (slot) ImGui::SameLine();
                    if (ImGui::RadioButton(label.c_str(), *ctx.mortarProfileSlot == slot))
                    {
                        const bool apply = ctx.mortarProfileAutoLoadAndReset &&
                            *ctx.mortarProfileAutoLoadAndReset && *ctx.mortarProfileSlot != slot;
                        *ctx.mortarProfileSlot = slot;
                        if (apply && ctx.loadAndApplyMortarProfile) ctx.loadAndApplyMortarProfile();
                    }
                }
                if (ctx.mortarProfileAutoLoadAndReset)
                    ImGui::Checkbox("Auto load & Reset when changed##MortarProfile", ctx.mortarProfileAutoLoadAndReset);
            }
            if (ctx.saveMortarProfile && ImGui::Button("Save Mortar Profile")) ctx.saveMortarProfile();
            ImGui::SameLine();
            if (ctx.loadMortarProfile && ImGui::Button("Load Mortar Profile")) ctx.loadMortarProfile();
            ImGui::SameLine();
            if (ctx.loadAndApplyMortarProfile && ImGui::Button("Load && Apply Mortar Profile")) ctx.loadAndApplyMortarProfile();
            if (ctx.mortarProfileStatus && !ctx.mortarProfileStatus->empty()) ImGui::TextWrapped("%s", ctx.mortarProfileStatus->c_str());
            ImGui::Separator();
            SliderFloatWithPendingColor("Min Fire Angle", &ctx.tankSettings->mortarMinimumFireAngleDegrees, 1.0f, 35.0f, 1.0f, 18.0f, "%.0f deg", IsPending(ctx.tankSettings->mortarMinimumFireAngleDegrees, ctx.appliedTankSettings->mortarMinimumFireAngleDegrees));
            SliderFloatWithPendingColor("Max Wheelie Angle", &ctx.tankSettings->mortarMaximumAngleDegrees, 10.0f, 60.0f, 1.0f, 40.0f, "%.0f deg", IsPending(ctx.tankSettings->mortarMaximumAngleDegrees, ctx.appliedTankSettings->mortarMaximumAngleDegrees));
            SliderFloatWithPendingColor("Raise Rate", &ctx.tankSettings->mortarRaiseRateDegreesPerSecond, 1.0f, 60.0f, 1.0f, 12.0f, "%.0f deg/s", IsPending(ctx.tankSettings->mortarRaiseRateDegreesPerSecond, ctx.appliedTankSettings->mortarRaiseRateDegreesPerSecond));
            SliderFloatWithPendingColor("Return Rate", &ctx.tankSettings->mortarReturnRateDegreesPerSecond, 1.0f, 60.0f, 1.0f, 10.0f, "%.0f deg/s", IsPending(ctx.tankSettings->mortarReturnRateDegreesPerSecond, ctx.appliedTankSettings->mortarReturnRateDegreesPerSecond));
            SliderFloatWithPendingColor("Maximum Range", &ctx.tankSettings->mortarMaximumRangeMeters, 1.0f, 200.0f, 1.0f, 40.0f, "%.0f m", IsPending(ctx.tankSettings->mortarMaximumRangeMeters, ctx.appliedTankSettings->mortarMaximumRangeMeters));
            SliderFloatWithPendingColor("Maximum Attack Radius", &ctx.tankSettings->mortarMaximumAttackRadiusMeters, 0.5f, 50.0f, 0.5f, 6.0f, "%.1f m", IsPending(ctx.tankSettings->mortarMaximumAttackRadiusMeters, ctx.appliedTankSettings->mortarMaximumAttackRadiusMeters));
            SliderFloatWithPendingColor("Stance Torque", &ctx.tankSettings->mortarStanceTorqueNm, 10000.0f, 1000000.0f, 10000.0f, 500000.0f, "%.0f Nm", IsPending(ctx.tankSettings->mortarStanceTorqueNm, ctx.appliedTankSettings->mortarStanceTorqueNm));
            SliderFloatWithPendingColor("Stance Damping", &ctx.tankSettings->mortarStanceDampingNms, 1000.0f, 250000.0f, 1000.0f, 80000.0f, "%.0f Nms", IsPending(ctx.tankSettings->mortarStanceDampingNms, ctx.appliedTankSettings->mortarStanceDampingNms));
            ImGui::TextDisabled("Load && Apply / Reset applies pending mortar changes.");
        }

        if (ctx.rollingParametersWindowVisible != nullptr &&
            *ctx.rollingParametersWindowVisible)
        {
            ImGui::SetNextWindowSize(ImVec2(620.0f, 760.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Rolling Parameters", ctx.rollingParametersWindowVisible))
            {
			if (ctx.rollingProfileSlot != nullptr)
			{
				ImGui::TextUnformatted("Rolling Profile Slot");
				for (int slot = 0; slot < 3; ++slot)
				{
					const std::string label = "Slot " + std::to_string(slot + 1) + "##RollingProfile";
					if (slot != 0) ImGui::SameLine();
					if (ImGui::RadioButton(label.c_str(), *ctx.rollingProfileSlot == slot))
					{
						const bool loadAndReset = ctx.rollingProfileAutoLoadAndReset != nullptr &&
							Tank::App::ShouldLoadAndResetRollingProfile(
								*ctx.rollingProfileSlot,
								slot,
								*ctx.rollingProfileAutoLoadAndReset);
						*ctx.rollingProfileSlot = slot;
						if (loadAndReset && ctx.loadAndApplyRollingProfile)
						{
							ctx.loadAndApplyRollingProfile();
						}
					}
				}
				if (ctx.rollingProfileAutoLoadAndReset != nullptr)
				{
					ImGui::Checkbox(
						"Auto load & Reset when changed##RollingProfile",
						ctx.rollingProfileAutoLoadAndReset);
				}
			}
			if (ctx.saveRollingProfile != nullptr && ImGui::Button("Save Rolling Profile"))
			{
				ctx.saveRollingProfile();
			}
			ImGui::SameLine();
			if (ctx.loadRollingProfile != nullptr && ImGui::Button("Load Rolling Profile"))
			{
				ctx.loadRollingProfile();
			}
			ImGui::SameLine();
			if (ctx.loadAndApplyRollingProfile != nullptr && ImGui::Button("Load && Apply Rolling Profile"))
			{
				ctx.loadAndApplyRollingProfile();
			}
			if (ctx.rollingProfileStatus != nullptr && !ctx.rollingProfileStatus->empty())
			{
				ImGui::TextWrapped("%s", ctx.rollingProfileStatus->c_str());
			}
			ImGui::Separator();
			if (ImGui::Button("Reset Tank##RollingParameters"))
			{
				if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
			}
			ImGui::SameLine();
			if (ctx.rollingCheatWindowVisible != nullptr &&
				ImGui::Button("Open Rolling CheatWindow##RollingParameters"))
			{
				*ctx.rollingCheatWindowVisible = true;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Apply rolling parameter changes");
			ImGui::Separator();

			if (ImGui::Checkbox("Rolling Input", &ctx.tankSettings->rollingInputEnabled))
			{
				if (ctx.resetTrackedVehicle) ctx.resetTrackedVehicle();
			}
			SliderFloatWithPendingColor(
				"Roll Speed Multiplier",
				&ctx.tankSettings->rollSpeedMultiplier,
				0.5f,
				2.0f,
				0.05f,
				1.0f,
				"%.2f x",
				IsPending(
					ctx.tankSettings->rollSpeedMultiplier,
					ctx.appliedTankSettings->rollSpeedMultiplier));
			if (ctx.rollingOptimizer && ctx.envSettings)
			{
				auto& optimizer = *ctx.rollingOptimizer;
				ImGui::BeginDisabled(optimizer.Running());
				if (ImGui::Button(reinterpret_cast<const char*>(u8"最適化 (x0.5 <- x1 -> x2)")))
					optimizer.Start(*ctx.tankSettings, *ctx.envSettings);
				ImGui::EndDisabled();
				if (optimizer.Running())
				{
					ImGui::SameLine();
					if (ImGui::Button("Cancel Optimization")) optimizer.Cancel();
					const int completed = optimizer.CompletedEvaluations();
					const float progress = optimizer.ProgressFraction();
					char label[64] = {};
					std::snprintf(label, sizeof(label), "Optimizing %.0f%% (%d/%d)",
						progress * 100.0f, completed,
						Tank::App::RollingSpeedOptimizationSession::kExpectedEvaluationCount);
					ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), label);
				}
				ImGui::TextWrapped(reinterpret_cast<const char*>(
					u8"現在のx1 Motionを基準に、x0.5からx2までのMotionが一致するよう内部パラメータを最適化します。完了後、倍率の選択とReset Tankは手動で行います。"));
				ImGui::TextWrapped("%s", optimizer.Status().c_str());
				ImGui::TextDisabled("Flat floor / current settings / rigid chassis Local BB (8 corners)");
				const auto& result = optimizer.Result();
				if (result && !result->cancelled && result->error.empty())
				{
					ImGui::Text("BB RMS x0.5: %.3f -> %.3f m | Maximum %.3f m",
						result->slowBefore.rmsMeters, result->slowAfter.rmsMeters,
						result->slowAfter.maximumMeters);
					ImGui::Text("BB RMS x2.0: %.3f -> %.3f m | Maximum: %.3f -> %.3f m",
						result->before.rmsMeters, result->after.rmsMeters,
						result->before.maximumMeters, result->after.maximumMeters);
					ImGui::Text("Interpolated x1.5 BB RMS: %.3f m",
						result->intermediateError.rmsMeters);
					ImGui::Text("Landing: target %.3f s / result %.3f s",
						result->reference.landingSeconds * 0.5f, result->optimized.landingSeconds);
					ImGui::Text("Finished: target %.3f s / result %.3f s",
						result->reference.finishedSeconds * 0.5f, result->optimized.finishedSeconds);
					const bool matches = optimizer.Matches(*ctx.tankSettings, *ctx.envSettings);
					if (!matches) ImGui::TextWrapped("Settings changed since calibration. Optimize again before applying.");
					if (ImGui::TreeNodeEx(
							"Before Optimization: Position / Rotation Graphs",
							ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::TextWrapped("Current tuning before optimization. All values are relative to the pose at Roll start.");
						DrawRollingComparisonPlot("Before Relative Local Z Roll", *result,
							RollingPlotMetric::RollDegrees, "degrees", true);
						DrawRollingComparisonPlot("Before Relative move X", *result,
							RollingPlotMetric::MoveX, "m", true);
						DrawRollingComparisonPlot("Before Relative move Y", *result,
							RollingPlotMetric::MoveY, "m", true);
						DrawRollingComparisonPlot("Before Relative move Z", *result,
							RollingPlotMetric::MoveZ, "m", true);
						ImGui::TreePop();
					}
					if (result->improved && ImGui::TreeNodeEx(
							"After Optimization: Position / Rotation Graphs",
							ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::TextWrapped("Optimized tuning. Curves should overlap when speed scaling preserves motion.");
						DrawRollingComparisonPlot("After Relative Local Z Roll", *result,
							RollingPlotMetric::RollDegrees, "degrees", false);
						DrawRollingComparisonPlot("After Relative move X", *result,
							RollingPlotMetric::MoveX, "m", false);
						DrawRollingComparisonPlot("After Relative move Y", *result,
							RollingPlotMetric::MoveY, "m", false);
						DrawRollingComparisonPlot("After Relative move Z", *result,
							RollingPlotMetric::MoveZ, "m", false);
						if (ImGui::Button("Save x0.5 / x1.0 / x1.5 / x2.0 CSV"))
							optimizer.ExportCsv("Reports/RollingSpeed");
						ImGui::TreePop();
					}
					if (ImGui::TreeNode("Local BB trajectory error"))
					{
						ImGui::TextUnformatted("Compare x0.5(t) with x1(0.5t), and x2(t) with x1(2t), in the starting tank frame.");
						const auto& slowSamples = result->slowAfter.sampleRmsMeters;
						if (!slowSamples.empty()) ImGui::PlotLines("x0.5 Error (m)", slowSamples.data(),
							static_cast<int>(slowSamples.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0, 100));
						const auto& fastSamples = result->after.sampleRmsMeters;
						if (!fastSamples.empty()) ImGui::PlotLines("x2.0 Error (m)", fastSamples.data(),
							static_cast<int>(fastSamples.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0, 100));
						for (int corner = 0; corner < 8; ++corner)
							ImGui::Text("Corner %d (%cX %cY %cZ): x0.5 %.3f / x2 %.3f m RMS", corner,
								corner & 1 ? '+' : '-', corner & 2 ? '+' : '-', corner & 4 ? '+' : '-',
								result->slowAfter.cornerRmsMeters[corner],
								result->after.cornerRmsMeters[corner]);
						ImGui::TreePop();
					}
				}
			}
			if (ImGui::TreeNode("Speed Multiplier Internal Coefficients"))
			{
				ImGui::TextWrapped("Multipliers at x2. At x1 all coefficients are 1, preserving the reference motion.");
				const Tank::Physics::RollingSpeedTuning defaults;
				for (const auto& coefficient : Tank::Physics::kRollingSpeedCoefficients)
				{
					float& value = ctx.tankSettings->rollSpeedTuning.*(coefficient.member);
					const float applied = ctx.appliedTankSettings->rollSpeedTuning.*(coefficient.member);
					SliderFloatWithPendingColor(coefficient.label, &value,
						coefficient.minimum, coefficient.maximum, 0.05f,
						defaults.*(coefficient.member), "%.3f x", IsPending(value, applied));
				}
				ImGui::TreePop();
			}
			SliderFloatWithPendingColor(
				"Roll Torque",
				&ctx.tankSettings->rollTorqueNm,
				20000.0f,
				300000.0f,
				5000.0f,
				200000.0f,
				"%.0f N m",
				IsPending(ctx.tankSettings->rollTorqueNm, ctx.appliedTankSettings->rollTorqueNm));
			SliderFloatWithPendingColor(
				"Torque Cutoff Angle",
				&ctx.tankSettings->rollTorqueCutoffDegrees,
				45.0f,
				180.0f,
				5.0f,
				90.0f,
				"%.0f deg",
				IsPending(
					ctx.tankSettings->rollTorqueCutoffDegrees,
					ctx.appliedTankSettings->rollTorqueCutoffDegrees));
			SliderFloatWithPendingColor(
				"Return Decision Angle",
				&ctx.tankSettings->rollReturnDecisionDegrees,
				45.0f,
				89.0f,
				1.0f,
				75.0f,
				"%.0f deg",
				IsPending(
					ctx.tankSettings->rollReturnDecisionDegrees,
					ctx.appliedTankSettings->rollReturnDecisionDegrees));
			SliderFloatWithPendingColor(
				"Approach Start Angle",
				&ctx.tankSettings->rollApproachStartDegrees,
				1.0f,
				89.0f,
				1.0f,
				80.0f,
				"%.0f deg",
				IsPending(ctx.tankSettings->rollApproachStartDegrees, ctx.appliedTankSettings->rollApproachStartDegrees));
			SliderFloatWithPendingColor(
				"Approach Damping (Start-90 deg)",
				&ctx.tankSettings->rollApproachDampingNms,
				0.0f,
				100000.0f,
				1000.0f,
				30000.0f,
				"%.0f N m s",
				IsPending(ctx.tankSettings->rollApproachDampingNms, ctx.appliedTankSettings->rollApproachDampingNms));
			SliderFloatWithPendingColor(
				"Commit Torque (90 deg)",
				&ctx.tankSettings->rollCommitTorqueNm,
				0.0f,
				250000.0f,
				5000.0f,
				100000.0f,
				"%.0f N m",
				IsPending(ctx.tankSettings->rollCommitTorqueNm, ctx.appliedTankSettings->rollCommitTorqueNm));
			ImGui::Checkbox(
				"Match Physical Vehicle Width",
				&ctx.tankSettings->rollDistanceMatchesVehicleWidth);
			const float physicalVehicleWidth = (std::max)(
				ctx.tankSettings->chassisWidthM,
				ctx.tankSettings->trackSpacingM + ctx.tankSettings->trackWidthM);
			ImGui::Text(
				"Physical Vehicle Width: %.2f m",
				physicalVehicleWidth);
			if (ctx.tankSettings->rollDistanceMatchesVehicleWidth)
			{
				SliderFloatWithPendingColor(
				"Roll Travel",
					&ctx.tankSettings->rollTravelVehicleWidths,
					1.0f,
					10.0f,
					0.05f,
					1.0f,
					"%.2f x",
					IsPending(
						ctx.tankSettings->rollTravelVehicleWidths,
						ctx.appliedTankSettings->rollTravelVehicleWidths));
				ImGui::Text(
					"Target Roll Travel: %.2f m",
					physicalVehicleWidth *
					ctx.tankSettings->rollTravelVehicleWidths);
			}
			if (!ctx.tankSettings->rollDistanceMatchesVehicleWidth)
			{
				SliderFloatWithPendingColor(
					"Manual Roll Distance",
					&ctx.tankSettings->rollDistanceM,
					0.5f,
					7.0f,
					0.1f,
					2.4f,
					"%.2f m",
					IsPending(
						ctx.tankSettings->rollDistanceM,
						ctx.appliedTankSettings->rollDistanceM));
			}
			SliderFloatWithPendingColor(
				"Post-90 Air Brake Torque",
				&ctx.tankSettings->rollAirBrakeTorqueNm,
				0.0f,
				300000.0f,
				5000.0f,
				150000.0f,
				"%.0f N m",
				IsPending(
					ctx.tankSettings->rollAirBrakeTorqueNm,
					ctx.appliedTankSettings->rollAirBrakeTorqueNm));
			SliderFloatWithPendingColor(
				"Air Brake Release Angle",
				&ctx.tankSettings->rollAirBrakeReleaseDegrees,
				0.0f,
				60.0f,
				5.0f,
				30.0f,
				"%.0f deg remaining",
				IsPending(
					ctx.tankSettings->rollAirBrakeReleaseDegrees,
					ctx.appliedTankSettings->rollAirBrakeReleaseDegrees));
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
            ImGui::End();
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
				"Pivot Throttle Scale",
				&ctx.tankSettings->pivotTurnThrottleScale,
				0.0f, 1.0f, 0.05f, 1.0f, "%.2f x",
				IsPending(ctx.tankSettings->pivotTurnThrottleScale,
					ctx.appliedTankSettings->pivotTurnThrottleScale));
			ImGui::TextWrapped("Pivot only: lower values soften acceleration. Try 0.3-0.5; 1.0 keeps full throttle.");
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
			ImGui::SeparatorText("Gear Ratios");
			for (size_t gear = 0; gear < ctx.tankSettings->forwardGearRatios.size(); ++gear)
			{
				const std::string label = "Forward " + std::to_string(gear + 1);
				SliderFloatWithPendingColor(
					label.c_str(),
					&ctx.tankSettings->forwardGearRatios[gear],
					0.1f,
					10.0f,
					0.1f,
					4.0f - static_cast<float>(gear),
					"%.2f x",
					IsPending(
						ctx.tankSettings->forwardGearRatios[gear],
						ctx.appliedTankSettings->forwardGearRatios[gear]));
			}
			for (size_t gear = 0; gear < ctx.tankSettings->reverseGearRatios.size(); ++gear)
			{
				const std::string label = "Reverse " + std::to_string(gear + 1);
				SliderFloatWithPendingColor(
					label.c_str(),
					&ctx.tankSettings->reverseGearRatios[gear],
					-10.0f,
					-0.1f,
					0.1f,
					-4.0f + static_cast<float>(gear),
					"%.2f x",
					IsPending(
						ctx.tankSettings->reverseGearRatios[gear],
						ctx.appliedTankSettings->reverseGearRatios[gear]));
			}
			ImGui::TextDisabled("Final Drive Ratio multiplies every gear ratio.");
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
				"Track Width", &ctx.tankSettings->trackWidthM, 0.15f, 1.0f, 0.01f, 0.3f, "%.2f m",
				IsPending(ctx.tankSettings->trackWidthM, ctx.appliedTankSettings->trackWidthM));
			SliderFloatWithPendingColor(
				"Track Spacing", &ctx.tankSettings->trackSpacingM, 1.8f, 6.0f, 0.1f, 2.4f, "%.2f m",
				IsPending(ctx.tankSettings->trackSpacingM, ctx.appliedTankSettings->trackSpacingM));
			SliderFloatWithPendingColor(
				"Ride Height Scale", &ctx.tankSettings->rideHeightScale, 0.5f, 1.1f, 0.05f, 0.8f, "%.2f x",
				IsPending(ctx.tankSettings->rideHeightScale, ctx.appliedTankSettings->rideHeightScale));
			SliderFloatWithPendingColor(
				"Suspension Frequency",
				&ctx.tankSettings->suspensionFrequencyHz,
				0.1f,
				10.0f,
				0.1f,
				1.0f,
				"%.1f Hz",
				IsPending(
					ctx.tankSettings->suspensionFrequencyHz,
					ctx.appliedTankSettings->suspensionFrequencyHz));
			SliderFloatWithPendingColor(
				"Suspension Damping",
				&ctx.tankSettings->suspensionDamping,
				0.0f,
				2.0f,
				0.05f,
				0.5f,
				"%.2f",
				IsPending(
					ctx.tankSettings->suspensionDamping,
					ctx.appliedTankSettings->suspensionDamping));
			if (ImGui::TreeNode("Suspension Stroke per Wheel"))
			{
				const char* trackNames[] = { "Left", "Right" };
				const char* surfaceNames[] = { "Lower", "Upper" };
				for (int track = 0; track < Tank::Physics::kTankTrackCount; ++track)
				{
					for (int surface = 0; surface < Tank::Physics::kTankSurfacesPerTrack; ++surface)
					{
						ImGui::PushID(track * Tank::Physics::kTankSurfacesPerTrack + surface);
						const std::string groupLabel =
							std::string(trackNames[track]) + " " + surfaceNames[surface];
						if (ImGui::TreeNode(groupLabel.c_str()))
						{
							for (int position = 0;
								position < Tank::Physics::kTankSuspensionPositionsPerSurface;
								++position)
							{
								const bool endWheel = position == 0 ||
									position == Tank::Physics::kTankSuspensionPositionsPerSurface - 1;
								if (!endWheel && position > ctx.tankSettings->roadWheelCount)
								{
									continue;
								}
								const int slot = Tank::Physics::TankSuspensionSlotIndex(
									track, surface, position);
								const char* positionLabel = position == 0
									? "Front End"
									: (position == Tank::Physics::kTankSuspensionPositionsPerSurface - 1
										? "Rear End"
										: nullptr);
								const std::string roadLabel = positionLabel == nullptr
									? "Road " + std::to_string(position)
									: positionLabel;
								SliderFloatWithPendingColor(
									roadLabel.c_str(),
									&ctx.tankSettings->suspensionStrokeMeters[static_cast<size_t>(slot)],
									0.0f,
									0.5f,
									0.01f,
									endWheel ? 0.0f : 0.2f * ctx.tankSettings->rideHeightScale,
									"%.2f m",
									IsPending(
										ctx.tankSettings->suspensionStrokeMeters[static_cast<size_t>(slot)],
										ctx.appliedTankSettings->suspensionStrokeMeters[static_cast<size_t>(slot)]));
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
				}
				ImGui::TreePop();
			}
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
			SliderFloatWithPendingColor(
				"Wheel Horizontal Offset",
				&ctx.tankSettings->wheelHorizontalOffsetM,
				-1.0f,
				1.0f,
				0.05f,
				0.0f,
				"%.2f m",
				IsPending(
					ctx.tankSettings->wheelHorizontalOffsetM,
					ctx.appliedTankSettings->wheelHorizontalOffsetM));
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
			ImGui::SeparatorText("Dummy Model");
			displayChanged |= ImGui::Checkbox("Body##Dummy", ctx.showDummyModel);
			ImGui::SameLine();
			displayChanged |= ImGui::Checkbox("Wheels##Dummy", ctx.showDummyWheels);
			ImGui::SameLine();
			displayChanged |= ImGui::Checkbox("Track Shoes##Dummy", ctx.trackShoeDisplay);
			ImGui::SeparatorText("glTF Overlay");
			displayChanged |= ImGui::SliderFloat(
				"Model Scale", &ctx.visualSettings->gltfModelScale, 0.1f, 3.0f, "%.3f");
			displayChanged |= ImGui::Checkbox("Body", ctx.showGltfBody);
			ImGui::SameLine();
			displayChanged |= ImGui::Checkbox("Cannon", ctx.showGltfCannon);
			ImGui::SameLine();
			displayChanged |= ImGui::Checkbox("Side", ctx.showGltfSide);
			if (displayChanged && ctx.updateScene)
			{
				ctx.updateScene();
			}

            if (ImGui::Checkbox("Physics Debug Overlay", ctx.physicsDebugOverlay))
            {
                if (ctx.updateScene) ctx.updateScene();
            }
            if (*ctx.physicsDebugOverlay)
            {
                ImGui::TextUnformatted("Cyan: suspension  Green/Orange: contact  Yellow: normal");
            }
            ImGui::Text("Controls: W/S drive, A/D skid turn, Z left / C right pivot");
            ImGui::Text("Q/E roll, Space fire, X mortar, B brake, P pause, F step fwd");

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

		if (ImGui::CollapsingHeader("UI"))
		{
			if (ImGui::Button("Reset"))
			{
				ImGui::SetWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
				ImGui::SetWindowSize(ImVec2(560.0f, 720.0f), ImGuiCond_Always);
			}
		}

		ImGui::EndChild();
		ImGui::End();
		DrawRollingCheatWindow(ctx);
	}
}
