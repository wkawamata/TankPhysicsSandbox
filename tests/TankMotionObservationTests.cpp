#include "Physics/TankMotionObservation.h"
#include "Physics/TrackedVehicleTest.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace
{
    bool NearlyEqual(float lhs, float rhs, float tolerance = 0.0001f)
    {
        return std::abs(lhs - rhs) <= tolerance;
    }

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL TankMotionObservation: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    bool passed = true;

    Tank::Physics::TankState identityState;
    identityState.stepIndex = 7;
    identityState.timeSeconds = 0.5f;
    identityState.linearVelocity = {3.0f, 4.0f, 12.0f};
    identityState.angularVelocity = {1.0f, 2.0f, 2.0f};
    const Tank::Physics::TankMotionObservation identity =
        Tank::Physics::BuildTankMotionObservation(identityState);
    passed &= Check(identity.stepIndex == 7, "step index must be copied");
    passed &= Check(NearlyEqual(identity.timeSeconds, 0.5f),
        "time must be copied");
    passed &= Check(NearlyEqual(identity.bodyRight.x, 1.0f) &&
            NearlyEqual(identity.bodyRight.y, 0.0f) &&
            NearlyEqual(identity.bodyRight.z, 0.0f),
        "identity rotation must preserve the right axis");
    passed &= Check(NearlyEqual(identity.bodyUp.x, 0.0f) &&
            NearlyEqual(identity.bodyUp.y, 1.0f) &&
            NearlyEqual(identity.bodyUp.z, 0.0f),
        "identity rotation must preserve the up axis");
    passed &= Check(NearlyEqual(identity.bodyForward.x, 0.0f) &&
            NearlyEqual(identity.bodyForward.y, 0.0f) &&
            NearlyEqual(identity.bodyForward.z, 1.0f),
        "identity rotation must preserve the forward axis");
    passed &= Check(NearlyEqual(identity.localLinearVelocity.x, 3.0f) &&
            NearlyEqual(identity.localLinearVelocity.y, 4.0f) &&
            NearlyEqual(identity.localLinearVelocity.z, 12.0f),
        "identity local linear velocity must match world velocity");
    passed &= Check(NearlyEqual(identity.localAngularVelocity.x, 1.0f) &&
            NearlyEqual(identity.localAngularVelocity.y, 2.0f) &&
            NearlyEqual(identity.localAngularVelocity.z, 2.0f),
        "identity local angular velocity must match world angular velocity");
    passed &= Check(NearlyEqual(identity.linearSpeedMetersPerSecond, 13.0f),
        "linear speed must include all three axes");
    passed &= Check(NearlyEqual(identity.horizontalSpeedMetersPerSecond,
            std::sqrt(153.0f)),
        "horizontal speed must include world X and Z");
    passed &= Check(NearlyEqual(identity.angularSpeedRadiansPerSecond, 3.0f),
        "angular speed must be the vector length");
    passed &= Check(identity.allFinite,
        "finite identity input must produce a finite observation");

    Tank::Physics::TankState rotatedState;
    constexpr float halfSqrtTwo = 0.70710678118f;
    rotatedState.body.rotation = {0.0f, halfSqrtTwo, 0.0f, halfSqrtTwo};
    rotatedState.linearVelocity = {2.0f, 0.0f, 0.0f};
    const Tank::Physics::TankMotionObservation rotated =
        Tank::Physics::BuildTankMotionObservation(rotatedState);
    passed &= Check(NearlyEqual(rotated.bodyForward.x, 1.0f) &&
            NearlyEqual(rotated.bodyForward.z, 0.0f),
        "positive 90 degree yaw must rotate forward toward world right");
    passed &= Check(NearlyEqual(rotated.bodyRight.x, 0.0f) &&
            NearlyEqual(rotated.bodyRight.z, -1.0f),
        "positive 90 degree yaw must rotate right toward world backward");
    passed &= Check(NearlyEqual(rotated.localLinearVelocity.x, 0.0f) &&
            NearlyEqual(rotated.localLinearVelocity.z, 2.0f),
        "local velocity must use the rotated body axes");

    Tank::Physics::TankState contactState;
    contactState.wheelCount = 4;
    contactState.wheels[0].trackIndex = 0;
    contactState.wheels[0].hasContact = true;
    contactState.wheels[0].contactNormal = {0.0f, 1.0f, 0.0f};
    contactState.wheels[0].longitudinalSlipMetersPerSecond = -2.0f;
    contactState.wheels[0].suspensionVelocityMetersPerSecond = -3.0f;
    contactState.wheels[0].suspensionAtHardPoint = true;
    contactState.wheels[1].trackIndex = 0;
    contactState.wheels[1].upperSurface = true;
    contactState.wheels[1].hasContact = true;
    contactState.wheels[1].contactNormal = {0.0f, 1.0f, 0.0f};
    contactState.wheels[1].longitudinalSlipMetersPerSecond = 4.0f;
    contactState.wheels[1].suspensionVelocityMetersPerSecond = 1.0f;
    contactState.wheels[2].trackIndex = 1;
    contactState.wheels[2].hasContact = true;
    contactState.wheels[2].contactNormal = {0.0f, 1.0f, 0.0f};
    contactState.wheels[2].longitudinalSlipMetersPerSecond = 1.0f;
    contactState.wheels[2].suspensionVelocityMetersPerSecond = 0.5f;
    contactState.wheels[3].trackIndex = 1;
    contactState.wheels[3].longitudinalSlipMetersPerSecond = 100.0f;
    const Tank::Physics::TankMotionObservation contacts =
        Tank::Physics::BuildTankMotionObservation(contactState);
    passed &= Check(contacts.tracks[0].contactCount == 2 &&
            contacts.tracks[1].contactCount == 1,
        "contact counts must be aggregated by track");
    passed &= Check(contacts.tracks[0].lowerSurfaceContactCount == 1 &&
            contacts.tracks[0].upperSurfaceContactCount == 1,
        "lower and upper surface contacts must remain separate");
    passed &= Check(contacts.totalContactCount == 3 &&
            contacts.totalLowerSurfaceContactCount == 2 &&
            contacts.totalUpperSurfaceContactCount == 1,
        "total contact counts must match track aggregates");
    passed &= Check(NearlyEqual(
            contacts.tracks[0].maximumAbsoluteLongitudinalSlipMetersPerSecond,
            4.0f) &&
            NearlyEqual(
                contacts.tracks[0].averageAbsoluteLongitudinalSlipMetersPerSecond,
                3.0f),
        "slip aggregates must use absolute contacting-wheel values");
    passed &= Check(NearlyEqual(
            contacts.tracks[0].maximumAbsoluteSuspensionVelocityMetersPerSecond,
            3.0f),
        "maximum suspension velocity must use its absolute value");
    passed &= Check(contacts.tracks[0].hasSuspensionHardPoint,
        "track hard point state must aggregate contacting wheels");
    passed &= Check(NearlyEqual(contacts.averageContactNormal.y, 1.0f),
        "average contact normal must include all contacts");
    passed &= Check(contacts.hasLeftDriveContact &&
            contacts.hasRightDriveContact && contacts.hasRequiredDriveContact,
        "lower contacts on both tracks must report required drive contact");

    Tank::Physics::TankState invertedContactState;
    invertedContactState.wheelCount = 2;
    invertedContactState.wheels[0].trackIndex = 0;
    invertedContactState.wheels[0].upperSurface = true;
    invertedContactState.wheels[0].hasContact = true;
    invertedContactState.wheels[0].contactNormal = {0.0f, 1.0f, 0.0f};
    invertedContactState.wheels[1].trackIndex = 1;
    invertedContactState.wheels[1].upperSurface = true;
    invertedContactState.wheels[1].hasContact = true;
    invertedContactState.wheels[1].contactNormal = {0.0f, 1.0f, 0.0f};
    const Tank::Physics::TankMotionObservation invertedContacts =
        Tank::Physics::BuildTankMotionObservation(invertedContactState);
    passed &= Check(invertedContacts.hasRequiredDriveContact,
        "upper contacts on both tracks must support inverted drive contact");
    passed &= Check(contacts.allFinite,
        "finite contact input must produce a finite observation");

    Tank::Physics::TankState noContactState;
    const Tank::Physics::TankMotionObservation noContacts =
        Tank::Physics::BuildTankMotionObservation(noContactState);
    passed &= Check(noContacts.totalContactCount == 0 &&
            NearlyEqual(noContacts.averageContactNormal.x, 0.0f) &&
            NearlyEqual(noContacts.averageContactNormal.y, 0.0f) &&
            NearlyEqual(noContacts.averageContactNormal.z, 0.0f),
        "no-contact averages must remain finite zero values");
    passed &= Check(noContacts.allFinite,
        "no-contact observation must remain finite");

    Tank::Physics::TankState invalidState;
    invalidState.linearVelocity.x = std::numeric_limits<float>::infinity();
    const Tank::Physics::TankMotionObservation invalid =
        Tank::Physics::BuildTankMotionObservation(invalidState);
    passed &= Check(!invalid.allFinite,
        "non-finite source values must be reported");

    Tank::Physics::TrackedVehicleTest vehicleTest;
    vehicleTest.Initialize();
    constexpr float deltaTimeSeconds = 1.0f / 60.0f;
    for (int step = 0; step < 180; ++step)
    {
        vehicleTest.Step(deltaTimeSeconds);
    }
    const Tank::Physics::TrackedVehicleTestState& vehicleState =
        vehicleTest.State();
    const Tank::Physics::TankMotionObservation& vehicleObservation =
        vehicleState.motionObservation;
    passed &= Check(vehicleObservation.stepIndex == vehicleState.stepIndex &&
            NearlyEqual(vehicleObservation.timeSeconds, vehicleState.timeSeconds),
        "vehicle test bridge must preserve the observation step and time");
    passed &= Check(vehicleObservation.allFinite,
        "settled vehicle observation must be finite");
    passed &= Check(vehicleObservation.totalContactCount > 0,
        "settled vehicle observation must expose wheel contacts");
    passed &= Check(vehicleObservation.hasRequiredDriveContact,
        "settled vehicle must expose lower drive contact on both tracks");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS TankMotionObservation\n";
    return 0;
}
