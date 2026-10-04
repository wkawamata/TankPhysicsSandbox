#pragma once

class BoxDropPlayback
{
public:
    void Reset()
    {
        m_paused = false;
        m_singleStepRequested = false;
    }

    void TogglePaused()
    {
        m_paused = !m_paused;
    }

    bool IsPaused() const
    {
        return m_paused;
    }

    void RequestSingleStep()
    {
        if (m_paused)
        {
            m_singleStepRequested = true;
        }
    }

    bool ConsumeSimulationStep()
    {
        if (!m_paused)
        {
            return true;
        }
        if (!m_singleStepRequested)
        {
            return false;
        }
        m_singleStepRequested = false;
        return true;
    }

private:
    bool m_paused = false;
    bool m_singleStepRequested = false;
};
