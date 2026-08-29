#include "Physics/RollingObstructionDetector.h"

#include <iostream>

int main()
{
    using namespace Tank::Physics;
    RollingObstructionDetector detector;
    const RollingObstructionInput blocked = { 0.0f, 0.0f, 2000.0f, 1.0f };
    bool passed = detector.Update(blocked, 0.1f) ==
            RollingObstructionState::Suspected &&
        detector.Update(blocked, 0.1f) == RollingObstructionState::Blocked;
    detector.Reset();
    const RollingObstructionInput moving = { 10.0f, 1.0f, 0.0f, 0.0f };
    passed &= detector.Update(moving, 0.2f) == RollingObstructionState::Clear;
    detector.Reset();
    passed &= detector.Update(blocked, 0.1f) == RollingObstructionState::Suspected;
    passed &= detector.Update(moving, 0.1f) == RollingObstructionState::Clear;
    if (!passed)
    {
        std::cerr << "FAIL RollingObstructionDetector\n";
        return 1;
    }
    std::cout << "PASS RollingObstructionDetector\n";
    return 0;
}
