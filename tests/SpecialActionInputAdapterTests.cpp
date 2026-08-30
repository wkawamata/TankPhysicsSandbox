#include "Physics/SpecialActionInputAdapter.h"

#include <iostream>

using namespace Tank::Physics;

int main()
{
    TankInput input;
    input.leftLeverX = -1.0f;
    input.rightLeverX = 1.0f;
    const auto adapted = SpecialActionInputAdapter::FromTankInput(input);
    const bool passed = adapted.leftLeverHorizontal == -1.0f &&
        adapted.rightLeverHorizontal == 1.0f;
    SpecialActionRecognizer recognizer;
    passed &= SpecialActionInputAdapter::Update(recognizer, input) ==
        SpecialAction::MortarRequested;
    if (!passed)
    {
        std::cerr << "FAIL SpecialActionInputAdapter\n";
        return 1;
    }
    std::cout << "PASS SpecialActionInputAdapter\n";
    return 0;
}
