#include "App/BoxDropPlayback.h"

#include <iostream>

namespace
{
    bool Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL BoxDropPlayback: " << message << '\n';
        }
        return condition;
    }
}

int main()
{
    BoxDropPlayback playback;
    bool passed = Check(playback.ConsumeSimulationStep(), "running playback advances");
    playback.TogglePaused();
    passed &= Check(playback.IsPaused() && !playback.ConsumeSimulationStep(), "paused playback holds simulation");
    playback.RequestSingleStep();
    passed &= Check(playback.ConsumeSimulationStep(), "one requested step advances");
    passed &= Check(!playback.ConsumeSimulationStep(), "requested step is consumed exactly once");
    playback.TogglePaused();
    passed &= Check(!playback.IsPaused() && playback.ConsumeSimulationStep(), "resumed playback advances");
    playback.TogglePaused();
    playback.RequestSingleStep();
    playback.Reset();
    passed &= Check(!playback.IsPaused() && playback.ConsumeSimulationStep(), "reset resumes and clears pending steps");
    return passed ? 0 : 1;
}
