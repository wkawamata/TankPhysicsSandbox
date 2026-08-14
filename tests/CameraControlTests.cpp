#include <Windows.h>

#include "App/CameraController.h"
#include "Physics/TrackedVehicleTest.h"

#include <Camera/DebugCameraController.h>
#include <Scene/CameraProjection.h>
#include <Scene/CameraView.h>
#include <Scene/Scene.h>

#include <DirectXMath.h>

#include <cmath>
#include <iostream>

namespace
{
    constexpr float kTolerance = 0.001f;

    bool NearlyEqual(float left, float right, float tolerance = kTolerance)
    {
        return std::abs(left - right) <= tolerance;
    }

    bool NearlyEqual(
        const DirectX::XMFLOAT3& left,
        const DirectX::XMFLOAT3& right,
        float tolerance = kTolerance)
    {
        return NearlyEqual(left.x, right.x, tolerance) &&
            NearlyEqual(left.y, right.y, tolerance) &&
            NearlyEqual(left.z, right.z, tolerance);
    }

    DirectX::XMFLOAT3 ExpectedOrbitPosition(
        float yaw, float pitch, float distance)
    {
        return {
            std::sin(yaw) * std::cos(pitch) * distance,
            std::sin(pitch) * distance,
            std::cos(yaw) * std::cos(pitch) * distance,
        };
    }

    DirectX::XMFLOAT3 ExpectedWorldUp(const Engine::CameraState& camera)
    {
        using namespace DirectX;
        const XMVECTOR forward = XMVector3Normalize(
            XMLoadFloat3(&camera.gazePoint) - XMLoadFloat3(&camera.pos));
        const XMVECTOR worldY = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMFLOAT3 result = {};
        XMStoreFloat3(&result, XMVector3Normalize(
            worldY - forward * XMVectorGetX(XMVector3Dot(worldY, forward))));
        return result;
    }

    DirectX::XMFLOAT2 ProjectToScreen(
        const Engine::CameraState& camera,
        const DirectX::XMFLOAT3& worldPoint)
    {
        using namespace DirectX;
        constexpr float width = 1280.0f;
        constexpr float height = 720.0f;
        const XMMATRIX viewProjection =
            Engine::CreateCameraViewMatrix(camera) *
            Engine::CreateCameraProjectionMatrix(camera, width / height);
        const XMVECTOR ndc = XMVector3TransformCoord(
            XMLoadFloat3(&worldPoint), viewProjection);
        return {
            (XMVectorGetX(ndc) + 1.0f) * 0.5f * width,
            (1.0f - XMVectorGetY(ndc)) * 0.5f * height,
        };
    }

    bool ScreenProjectionMatchesExpectedOrbit(
        const Engine::CameraState& actual,
        float expectedYaw,
        float expectedPitch,
        float distance)
    {
        Engine::CameraState expected = actual;
        expected.pos = ExpectedOrbitPosition(
            expectedYaw, expectedPitch, distance);
        expected.gazePoint = {};
        expected.up = ExpectedWorldUp(expected);

        constexpr DirectX::XMFLOAT3 testPoints[] = {
            { 0.0f, 0.0f, 0.0f },
            { 2.0f, 0.0f, 0.0f },
            { 0.0f, 2.0f, 0.0f },
            { 0.0f, 0.0f, 2.0f },
            { -1.0f, 0.5f, -3.0f },
        };
        for (const DirectX::XMFLOAT3& point : testPoints)
        {
            const DirectX::XMFLOAT2 actualScreen =
                ProjectToScreen(actual, point);
            const DirectX::XMFLOAT2 expectedScreen =
                ProjectToScreen(expected, point);
            if (!NearlyEqual(actualScreen.x, expectedScreen.x, 0.01f) ||
                !NearlyEqual(actualScreen.y, expectedScreen.y, 0.01f))
            {
                return false;
            }
        }

        const DirectX::XMFLOAT2 pivotScreen =
            ProjectToScreen(actual, { 0.0f, 0.0f, 0.0f });
        const DirectX::XMFLOAT2 worldUpScreen =
            ProjectToScreen(actual, { 0.0f, 2.0f, 0.0f });
        if (!NearlyEqual(pivotScreen.x, worldUpScreen.x, 0.01f) ||
            worldUpScreen.y >= pivotScreen.y)
        {
            return false;
        }

        using namespace DirectX;
        const Engine::CameraBasis actualBasis =
            Engine::ResolveCameraBasis(actual);
        const XMVECTOR expectedZ = XMVector3Normalize(
            XMLoadFloat3(&actual.gazePoint) - XMLoadFloat3(&actual.pos));
        const XMVECTOR expectedX = XMVector3Normalize(XMVector3Cross(
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), expectedZ));
        const XMVECTOR expectedY = XMVector3Normalize(
            XMVector3Cross(expectedZ, expectedX));
        const auto vectorMatches = [](FXMVECTOR left, FXMVECTOR right)
        {
            return XMVectorGetX(XMVector3Length(left - right)) <= kTolerance;
        };
        return vectorMatches(actualBasis.forward, expectedZ) &&
            vectorMatches(actualBasis.right, expectedX) &&
            vectorMatches(actualBasis.up, expectedY);
    }

    bool TestHorizontalDragUsesWorldYaw(float initialPitch)
    {
        Engine::CameraState camera = {};
        RtPbrSurvey::DebugCameraController controller;
        controller.SetCameraState(&camera);
        controller.SetWindowSize(1280, 720);
        controller.SetMode(RtPbrSurvey::DebugCameraController::Mode::Arcball);
        controller.SetObjectViewerState(0.3f, initialPitch, 12.0f, {});

        const float initialY = camera.pos.y;
        const float initialYaw = controller.ObjectViewerYaw();
        const float initialOrbitPitch = controller.ObjectViewerPitch();
        controller.OnMouseDown(VK_LBUTTON, 640, 360);
        controller.OnMouseMove(740, 360);
        controller.OnMouseUp(VK_LBUTTON, 740, 360);
        const DirectX::XMFLOAT3 expectedUp = ExpectedWorldUp(camera);
        Tank::App::CameraController::StabilizeWorldUp(camera);

        const float expectedYaw = initialYaw + 0.5f;
        const DirectX::XMFLOAT3 expectedPosition =
            ExpectedOrbitPosition(expectedYaw, initialOrbitPitch, 12.0f);
        return NearlyEqual(controller.ObjectViewerYaw(), expectedYaw) &&
            NearlyEqual(controller.ObjectViewerPitch(), initialOrbitPitch) &&
            NearlyEqual(camera.pos.y, initialY) &&
            NearlyEqual(camera.pos, expectedPosition) &&
            NearlyEqual(camera.up, expectedUp) &&
            ScreenProjectionMatchesExpectedOrbit(
                camera, expectedYaw, initialOrbitPitch, 12.0f);
    }

    bool TestVerticalDragPreservesWorldYaw(float initialPitch, int deltaY)
    {
        Engine::CameraState camera = {};
        RtPbrSurvey::DebugCameraController controller;
        controller.SetCameraState(&camera);
        controller.SetWindowSize(1280, 720);
        controller.SetMode(RtPbrSurvey::DebugCameraController::Mode::Arcball);
        controller.SetObjectViewerState(0.7f, initialPitch, 12.0f, {});

        const float initialYaw = controller.ObjectViewerYaw();
        const float initialOrbitPitch = controller.ObjectViewerPitch();
        controller.OnMouseDown(VK_LBUTTON, 640, 360);
        controller.OnMouseMove(640, 360 + deltaY);
        controller.OnMouseUp(VK_LBUTTON, 640, 360 + deltaY);
        const DirectX::XMFLOAT3 expectedUp = ExpectedWorldUp(camera);
        Tank::App::CameraController::StabilizeWorldUp(camera);

        const float expectedPitch = initialOrbitPitch +
            static_cast<float>(deltaY) * 0.005f;
        const DirectX::XMFLOAT3 expectedPosition =
            ExpectedOrbitPosition(initialYaw, expectedPitch, 12.0f);
        return NearlyEqual(controller.ObjectViewerYaw(), initialYaw) &&
            NearlyEqual(controller.ObjectViewerPitch(), expectedPitch) &&
            NearlyEqual(camera.pos, expectedPosition) &&
            NearlyEqual(camera.up, expectedUp) &&
            ScreenProjectionMatchesExpectedOrbit(
                camera, initialYaw, expectedPitch, 12.0f);
    }

    bool TestDebugFollowTranslatesWithoutYawChase()
    {
        Tank::App::CameraController controller;
        controller.SetFollowEnabled(true);
        controller.SetTankYawChaseEnabled(false);

        Engine::CameraState camera = {};
        camera.pos = { 5.0f, 4.0f, 8.0f };
        camera.gazePoint = { 0.0f, 0.5f, 0.0f };
        const DirectX::XMFLOAT3 initialOffset = {
            camera.pos.x - camera.gazePoint.x,
            camera.pos.y - camera.gazePoint.y,
            camera.pos.z - camera.gazePoint.z,
        };

        Tank::Physics::TrackedVehicleTestState state = {};
        state.bodyRotation.w = 1.0f;
        controller.UpdateFollowCamera(state, 1.0f / 60.0f, camera);

        state.bodyPosition = { 3.0f, 1.0f, -2.0f };
        state.bodyRotation = { 0.0f, 0.7071068f, 0.0f, 0.7071068f };
        controller.UpdateFollowCamera(state, 1.0f / 60.0f, camera);

        const DirectX::XMFLOAT3 translatedOffset = {
            camera.pos.x - camera.gazePoint.x,
            camera.pos.y - camera.gazePoint.y,
            camera.pos.z - camera.gazePoint.z,
        };
        return NearlyEqual(camera.pos.x, 8.0f) &&
            NearlyEqual(camera.pos.y, 5.0f) &&
            NearlyEqual(camera.pos.z, 6.0f) &&
            NearlyEqual(camera.gazePoint.x, 3.0f) &&
            NearlyEqual(camera.gazePoint.y, 1.5f) &&
            NearlyEqual(camera.gazePoint.z, -2.0f) &&
            NearlyEqual(translatedOffset.x, initialOffset.x) &&
            NearlyEqual(translatedOffset.y, initialOffset.y) &&
            NearlyEqual(translatedOffset.z, initialOffset.z);
    }

    bool TestDebugAdoptionUsesTankAsOrbitPivot()
    {
        Tank::App::CameraController controller;
        Engine::CameraState camera = {};
        camera.pos = { 8.0f, 6.0f, 12.0f };
        camera.gazePoint = { -4.0f, 2.0f, 3.0f };

        Tank::Physics::TrackedVehicleTestState state = {};
        state.bodyPosition = { 3.0f, 1.0f, -2.0f };
        state.bodyRotation.w = 1.0f;
        controller.AdoptCurrentFollowPose(state, camera);

        return NearlyEqual(camera.gazePoint.x, 3.0f) &&
            NearlyEqual(camera.gazePoint.y, 1.5f) &&
            NearlyEqual(camera.gazePoint.z, -2.0f) &&
            NearlyEqual(camera.pos.x, 8.0f) &&
            NearlyEqual(camera.pos.y, 6.0f) &&
            NearlyEqual(camera.pos.z, 12.0f);
    }

    bool TestLookDownMatchesRtPbrLimit()
    {
        Tank::App::CameraController controller;
        controller.SetLookDownDegrees(88.0f);
        return NearlyEqual(
            controller.LookDownDegrees(),
            Tank::App::CameraController::kMaximumLookDownDegrees);
    }

    bool TestOrthographicToPerspectiveSwitchesAtTransitionStart()
    {
        Tank::App::CameraController controller;
        Engine::CameraState camera = {};
        camera.pos = { 0.0f, 10.0f, -10.0f };
        camera.gazePoint = {};
        camera.projection = Engine::CameraProjection::Orthographic;
        camera.orthographicHeight = 20.0f;

        Tank::Rendering::CameraSettings target =
            controller.CaptureSettings(camera, true);
        target.projection = static_cast<int>(Engine::CameraProjection::Perspective);
        target.fovDegrees = 35.0f;
        controller.ApplySettings(target, true, camera);

        return camera.projection == Engine::CameraProjection::Perspective &&
            controller.IsTransitioning();
    }

    bool TestPerspectiveToOrthographicSwitchesAtTransitionEnd()
    {
        Tank::App::CameraController controller;
        Engine::CameraState camera = {};
        camera.pos = { 0.0f, 10.0f, -10.0f };
        camera.gazePoint = {};
        camera.projection = Engine::CameraProjection::Perspective;
        camera.fov = 35.0f;

        Tank::Rendering::CameraSettings target =
            controller.CaptureSettings(camera, true);
        target.projection = static_cast<int>(Engine::CameraProjection::Orthographic);
        target.orthographicHeight = 20.0f;
        controller.ApplySettings(target, true, camera);
        const bool remainsPerspectiveAtStart =
            camera.projection == Engine::CameraProjection::Perspective;
        controller.UpdateTransition(0.5f, camera);
        const bool remainsPerspectiveDuringTransition =
            camera.projection == Engine::CameraProjection::Perspective;
        controller.UpdateTransition(0.5f, camera);

        return remainsPerspectiveAtStart && remainsPerspectiveDuringTransition &&
            camera.projection == Engine::CameraProjection::Orthographic &&
            !controller.IsTransitioning();
    }

    bool TestProjectionTransitionDoesNotOverwriteSlotCache()
    {
        Tank::App::CameraController controller;
        Engine::CameraState camera = {};
        camera.pos = { 0.0f, 10.0f, -10.0f };
        camera.gazePoint = {};
        camera.projection = Engine::CameraProjection::Perspective;

        Tank::Rendering::CameraSettings orthographic =
            controller.CaptureSettings(camera, true);
        orthographic.projection =
            static_cast<int>(Engine::CameraProjection::Orthographic);
        orthographic.orthographicHeight = 50.0f;
        controller.SetSlotSettings(2, orthographic);
        controller.SelectSlot(2, false);
        controller.ApplySettings(orthographic, true, camera);

        controller.UpdateSlotCache(camera);
        const Tank::Rendering::CameraSettings* cached =
            controller.GetCachedSettings();
        return cached != nullptr &&
            cached->projection ==
                static_cast<int>(Engine::CameraProjection::Orthographic) &&
            !controller.IsSlotDirty(2);
    }

    bool TestDebugCameraKeepsTankFocusAcrossRepeatedResets()
    {
        Tank::App::CameraController controller;
        controller.SetFollowEnabled(true);
        controller.SetTankYawChaseEnabled(false);
        Engine::CameraState camera = {};
        camera.pos = { 8.0f, 6.0f, 12.0f };
        camera.gazePoint = { 3.0f, 1.5f, -2.0f };

        Tank::Physics::TrackedVehicleTestState previous = {};
        previous.bodyPosition = { 3.0f, 1.0f, -2.0f };
        previous.bodyRotation.w = 1.0f;
        Tank::Physics::TrackedVehicleTestState reset = {};
        reset.bodyPosition = { -4.0f, 0.5f, 5.0f };
        reset.bodyRotation.w = 1.0f;

        controller.OnTankTeleported(previous, reset, camera);
        controller.UpdateFollowCamera(reset, 1.0f / 60.0f, camera);
        const bool firstResetKeepsFocus =
            NearlyEqual(camera.gazePoint.x, -4.0f) &&
            NearlyEqual(camera.gazePoint.y, 1.0f) &&
            NearlyEqual(camera.gazePoint.z, 5.0f);

        controller.OnTankTeleported(reset, previous, camera);
        controller.UpdateFollowCamera(previous, 1.0f / 60.0f, camera);
        return firstResetKeepsFocus &&
            NearlyEqual(camera.gazePoint.x, 3.0f) &&
            NearlyEqual(camera.gazePoint.y, 1.5f) &&
            NearlyEqual(camera.gazePoint.z, -2.0f);
    }

    bool TestChaseOrbitOffsetReturnsToRear()
    {
        Tank::App::CameraController controller;
        controller.SetFollowEnabled(true);
        controller.SetTankYawChaseEnabled(true);
        controller.SetLookDownDegrees(45.0f);
        constexpr float dt = 1.0f / 60.0f;
        for (int frame = 0; frame < 180; ++frame)
        {
            controller.UpdateChaseOrbitInput(1.0f, 1.0f, dt);
        }
        const bool reachesOffset =
            controller.ChaseOrbitYawOffsetDegrees() > 55.0f &&
            controller.ChaseOrbitYawOffsetDegrees() <= 90.0f &&
            controller.ChaseOrbitPitchOffsetDegrees() > 30.0f &&
            controller.LookDownDegrees() +
                controller.ChaseOrbitPitchOffsetDegrees() <=
                Tank::App::CameraController::kMaximumLookDownDegrees;
        for (int frame = 0; frame < 180; ++frame)
        {
            controller.UpdateChaseOrbitInput(0.0f, 0.0f, dt);
        }
        return reachesOffset &&
            std::abs(controller.ChaseOrbitYawOffsetDegrees()) < 0.01f &&
            std::abs(controller.ChaseOrbitPitchOffsetDegrees()) < 0.01f;
    }
}

int main()
{
    const bool camera2Horizontal = TestHorizontalDragUsesWorldYaw(0.5f);
    const bool camera2Vertical = TestVerticalDragPreservesWorldYaw(0.5f, 20);
    const bool nearPolarHorizontal = TestHorizontalDragUsesWorldYaw(1.39f);
    const bool nearPolarVertical = TestVerticalDragPreservesWorldYaw(1.39f, -10);
    const bool positionOnlyFollow = TestDebugFollowTranslatesWithoutYawChase();
    const bool tankOrbitPivot = TestDebugAdoptionUsesTankAsOrbitPivot();
    const bool lookDownLimit = TestLookDownMatchesRtPbrLimit();
    const bool orthoToPerspective =
        TestOrthographicToPerspectiveSwitchesAtTransitionStart();
    const bool perspectiveToOrtho =
        TestPerspectiveToOrthographicSwitchesAtTransitionEnd();
    const bool projectionCachePreserved =
        TestProjectionTransitionDoesNotOverwriteSlotCache();
    const bool debugResetFocus =
        TestDebugCameraKeepsTankFocusAcrossRepeatedResets();
    const bool chaseOrbitOffset = TestChaseOrbitOffsetReturnsToRear();

    if (!camera2Horizontal || !camera2Vertical ||
        !nearPolarHorizontal || !nearPolarVertical ||
        !positionOnlyFollow || !tankOrbitPivot || !lookDownLimit ||
        !orthoToPerspective || !perspectiveToOrtho ||
        !projectionCachePreserved || !debugResetFocus || !chaseOrbitOffset)
    {
        std::cerr << "Camera control contract failed:"
                  << " camera2Horizontal=" << camera2Horizontal
                  << " camera2Vertical=" << camera2Vertical
                  << " nearPolarHorizontal=" << nearPolarHorizontal
                  << " nearPolarVertical=" << nearPolarVertical
                  << " positionOnlyFollow=" << positionOnlyFollow
                  << " tankOrbitPivot=" << tankOrbitPivot
                  << " lookDownLimit=" << lookDownLimit
                  << " orthoToPerspective=" << orthoToPerspective
                  << " perspectiveToOrtho=" << perspectiveToOrtho
                  << " projectionCachePreserved=" << projectionCachePreserved
                  << " debugResetFocus=" << debugResetFocus
                  << " chaseOrbitOffset=" << chaseOrbitOffset
                  << '\n';
        return 1;
    }

    std::cout << "PASS camera-control-contract\n";
    return 0;
}
