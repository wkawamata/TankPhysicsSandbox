#include "Physics/SpecialActionRecognizer.h"

#include <iostream>
#include <limits>

namespace
{
    using namespace Tank::Physics;

    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL SpecialActionRecognizer: " << message << "\n";
        }
        return condition;
    }
}

int main()
{
    using namespace Tank::Physics;
    bool passed = true;

    SpecialActionRecognizer recognizer;
    passed &= Check(recognizer.Update({ -0.8f, 0.8f }) ==
            SpecialAction::MortarRequested,
        "outward lever input must request mortar");
    passed &= Check(recognizer.Update({ -0.8f, 0.8f }) == SpecialAction::None,
        "held mortar input must not repeat");
    passed &= Check(recognizer.Update({ 0.0f, 0.0f }) == SpecialAction::None,
        "neutral input must re-arm without emitting an action");
    passed &= Check(recognizer.IsArmed(), "neutral input must re-arm");

    passed &= Check(recognizer.Update({ -0.8f, -0.8f }) ==
            SpecialAction::RollLeftRequested,
        "same negative direction must request left roll");
    passed &= Check(recognizer.Update({ 0.0f, 0.0f }) == SpecialAction::None,
        "roll input must require neutral before re-arm");
    passed &= Check(recognizer.Update({ 0.8f, 0.8f }) ==
            SpecialAction::RollRightRequested,
        "same positive direction must request right roll");

    recognizer.Reset();
    passed &= Check(recognizer.Update({ 0.8f, -0.8f }) == SpecialAction::None,
        "inward opposing input must not request a special action");
    passed &= Check(recognizer.Update({ -0.8f, 0.8f }) ==
            SpecialAction::MortarRequested,
        "outward input must have priority over non-action input");

    recognizer.Reset();
    passed &= Check(recognizer.Update({
            std::numeric_limits<float>::quiet_NaN(), 0.0f }) ==
            SpecialAction::None,
        "non-finite input must be rejected");
    passed &= Check(!recognizer.IsArmed(),
        "non-finite input must require neutral re-arm");
    passed &= Check(recognizer.Update({ 0.0f, 0.0f }) == SpecialAction::None,
        "neutral input must recover after invalid input");

    if (!passed)
    {
        return 1;
    }

    std::cout << "PASS SpecialActionRecognizer\n";
    return 0;
}
