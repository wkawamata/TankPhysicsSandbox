#include <Jolt/Jolt.h>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <iostream>
#include <cstdarg>
#include <thread>

JPH_SUPPRESS_WARNINGS

using namespace JPH;
using namespace JPH::literals;
using namespace std;

static void TraceImpl(const char *inFMT, ...)
{
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    cout << buffer << endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, uint inLine)
{
    cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << endl;
    return true;
};
#endif

namespace Layers
{
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer NUM_LAYERS = 2;
};

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter
{
public:
    bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override
    {
        switch (inObject1)
        {
        case Layers::NON_MOVING: return inObject2 == Layers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

namespace BroadPhaseLayers
{
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS(2);
};

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override
    {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

    const char *GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override
    {
        switch ((BroadPhaseLayer::Type)inLayer)
        {
        case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        default:                                                  return "INVALID";
        }
    }

private:
    BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter
{
public:
    bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override
    {
        switch (inLayer1)
        {
        case Layers::NON_MOVING: return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:     return true;
        default:                 return false;
        }
    }
};

class BodyActivationListenerImpl : public BodyActivationListener
{
public:
    void OnBodyActivated(const BodyID &, uint64) override
    {
        cout << "Body activated" << endl;
    }

    void OnBodyDeactivated(const BodyID &, uint64) override
    {
        cout << "Body went to sleep" << endl;
    }
};

int main()
{
    RegisterDefaultAllocator();
    Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    Factory::sInstance = new Factory();
    RegisterTypes();

    TempAllocatorImpl temp_allocator(10 * 1024 * 1024);
    JobSystemThreadPool job_system(cMaxPhysicsJobs, cMaxPhysicsBarriers, thread::hardware_concurrency() - 1);

    const uint cMaxBodies = 1024;
    const uint cNumBodyMutexes = 0;
    const uint cMaxBodyPairs = 1024;
    const uint cMaxContactConstraints = 1024;

    BPLayerInterfaceImpl broad_phase_layer_interface;
    ObjectVsBroadPhaseLayerFilterImpl object_vs_broadphase_layer_filter;
    ObjectLayerPairFilterImpl object_vs_object_layer_filter;

    PhysicsSystem physics_system;
    physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
        broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

    BodyActivationListenerImpl body_activation_listener;
    physics_system.SetBodyActivationListener(&body_activation_listener);

    BodyInterface &body_interface = physics_system.GetBodyInterface();

    // Ground: large static box
    BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
    floor_shape_settings.SetEmbedded();
    ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    BodyCreationSettings floor_settings(floor_shape_result.Get().GetPtr(),
        RVec3(0.0_r, -1.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
    Body *floor = body_interface.CreateBody(floor_settings);
    body_interface.AddBody(floor->GetID(), EActivation::DontActivate);

    // Dynamic box
    BodyCreationSettings box_settings(new BoxShape(Vec3::sReplicate(0.5f)),
        RVec3(0.0_r, 5.0_r, 0.0_r), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
    BodyID box_id = body_interface.CreateAndAddBody(box_settings, EActivation::Activate);

    constexpr float cDeltaTime = 1.0f / 60.0f;

    int step = 0;
    while (body_interface.IsActive(box_id))
    {
        ++step;
        RVec3 pos = body_interface.GetCenterOfMassPosition(box_id);
        Vec3 vel = body_interface.GetLinearVelocity(box_id);
        cout << "Step " << step << ": pos = (" << pos.GetX() << ", " << pos.GetY() << ", " << pos.GetZ()
             << "), vel = (" << vel.GetX() << ", " << vel.GetY() << ", " << vel.GetZ() << ")" << endl;

        physics_system.Update(cDeltaTime, 1, &temp_allocator, &job_system);
    }

    cout << "Box settled at y = " << body_interface.GetCenterOfMassPosition(box_id).GetY() << endl;

    body_interface.RemoveBody(box_id);
    body_interface.DestroyBody(box_id);
    body_interface.RemoveBody(floor->GetID());
    body_interface.DestroyBody(floor->GetID());

    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    return 0;
}
