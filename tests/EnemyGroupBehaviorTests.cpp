#include "Physics/EnemyGroupBehavior.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
    using namespace Tank::Physics;
    try
    {
        const auto check = [](bool valid, const char* message)
            { if (!valid) throw std::runtime_error(message); };
        Tank::Map::EnemyPatrolSettings settings = {
            {{{10, 0, 0}, "ground"}, {{20, 0, 0}, "bridge"}}, 5, 7, 1, 3};
        EnemyGroupBehavior behavior;
        check(behavior.Initialize(settings), "Explicit settings accepted");
        behavior.Update(1, true, {}, false, {});
        check(behavior.State().action == EnemyGroupAction::Patrol && behavior.State().moveTarget.x == 10 &&
            behavior.State().targetSurfaceId == "ground", "Patrol starts at configured point");
        behavior.Update(1, true, {10, 0, 0}, false, {});
        check(behavior.State().patrolIndex == 1 && behavior.State().moveTarget.x == 20 &&
            behavior.State().targetSurfaceId == "bridge", "Arrival advances waypoint with surface identity");
        behavior.Update(1, true, {}, true, {30, 0, 0});
        check(behavior.State().action == EnemyGroupAction::Approach && behavior.State().moveTarget.x == 30 &&
            !behavior.State().attackRequested, "Detection interrupts patrol for approach");
        behavior.Update(1, true, {25, 0, 0}, true, {30, 0, 0});
        check(behavior.State().action == EnemyGroupAction::AttackStop && behavior.State().attackRequested &&
            !behavior.State().hasMoveTarget, "Stop boundary requests attack without movement");
        behavior.Update(1, true, {25, 0, 0}, true, {31, 0, 0});
        check(behavior.State().action == EnemyGroupAction::AttackStop, "Hysteresis prevents repeated starts");
        behavior.Update(1, true, {25, 0, 0}, true, {32, 0, 0});
        check(behavior.State().action == EnemyGroupAction::Approach, "Resume boundary restarts approach");
        behavior.Update(1, true, {25, 0, 0}, false, {999, 0, 0});
        check(behavior.State().action == EnemyGroupAction::Search && behavior.State().moveTarget.x == 32,
            "Loss follows last observation rather than unobserved player position");
        behavior.Update(1, true, {32, 0, 0}, false, {});
        check(behavior.State().action == EnemyGroupAction::SearchWait && behavior.State().searchWaitRemaining == 3 &&
            !behavior.State().attackRequested, "Arrival starts full wait and does not fire");
        behavior.Update(10, false, {32, 0, 0}, false, {});
        check(behavior.State().action == EnemyGroupAction::Hold && behavior.State().searchWaitRemaining == 3,
            "Unavailable command pauses phase and wait");
        behavior.Update(1, true, {32, 0, 0}, false, {});
        check(behavior.State().searchWaitRemaining == 2, "Wait continues after command resumes");
        behavior.Update(1, true, {32, 0, 0}, true, {40, 0, 0});
        check(behavior.State().action == EnemyGroupAction::Approach && behavior.State().searchWaitRemaining == 0,
            "Reacquisition interrupts wait");
        behavior.Update(1, true, {40, 0, 0}, false, {});
        behavior.Update(3, true, {40, 0, 0}, false, {});
        check(behavior.State().action == EnemyGroupAction::Patrol && behavior.State().patrolIndex == 1,
            "Wait expires and resumes interrupted patrol index");
        check(!behavior.Update(-1, true, {}, false, {}) && behavior.State().action == EnemyGroupAction::Patrol,
            "Invalid time leaves state unchanged");
        check(!behavior.Update(1, true, {}, true, {std::numeric_limits<float>::quiet_NaN(), 0, 0}),
            "Nonfinite observation rejected");
        auto invalid = settings;
        invalid.resumeApproachMeters = 5;
        check(!behavior.Initialize(invalid) && behavior.State().patrolIndex == 1,
            "Invalid settings preserve running state");
        settings.searchWaitSeconds = 0;
        check(behavior.Initialize(settings), "Zero search wait supported");
        behavior.Update(1, true, {}, true, {10, 0, 0});
        behavior.Update(1, true, {10, 0, 0}, false, {});
        check(behavior.State().action == EnemyGroupAction::Patrol, "Zero wait resumes patrol immediately");
        behavior.Update(1, true, {20, 10, 0}, false, {});
        check(behavior.State().patrolIndex == 1, "Different heights do not count as horizontal arrival");
        check(behavior.Initialize(settings), "Reset surface memory fixture");
        const Vec3 mapped = {4, 0, 0};
        behavior.Update(1, true, {}, true, {12, 0, 0}, "ground", &mapped);
        check(behavior.State().action == EnemyGroupAction::Approach && behavior.State().moveTarget.x == 4,
            "Attack distance uses actual observation while route targets mapped node");
        behavior.Update(1, true, {}, false, {});
        check(behavior.State().action == EnemyGroupAction::Search && behavior.State().targetSurfaceId == "ground" &&
            behavior.State().moveTarget.x == 4, "Lost target preserves mapped node and surface");
        behavior.Update(1, true, mapped, false, {});
        check(behavior.State().action == EnemyGroupAction::Patrol,
            "Search arrival uses mapped navigation position with zero wait");
        std::cout << "PASS EnemyGroupBehavior\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL EnemyGroupBehavior: " << error.what() << '\n';
        return 1;
    }
}
