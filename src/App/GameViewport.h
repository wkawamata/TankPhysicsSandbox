#pragma once
#include <algorithm>
#include <cmath>

namespace Tank::App
{
    struct GameViewportSettings
    {
        bool enabled = false;
        float x = 0, y = 0, width = 1, height = 1;
        // 0: free rectangle; 1: 16:9; 2: 4:3; 3: 21:9; 4: 3:4.
        int aspect = 0;
        // 0: manual position (legacy); 1: centered; 2: top left.
        int alignment = 0;
        bool operator==(const GameViewportSettings&) const = default;
    };

    struct GameViewportRect
    {
        int x = 0, y = 0, width = 1, height = 1;
        bool Contains(float px, float py) const
        { return px >= x && py >= y && px < x + width && py < y + height; }
    };

    inline GameViewportRect ResolveGameViewport(const GameViewportSettings& settings, int width, int height)
    {
        width = std::max(1, width); height = std::max(1, height);
        if (!settings.enabled) return {0, 0, width, height};
        const auto unit = [](float v, float fallback) {return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : fallback;};
        GameViewportRect result;
        result.x = settings.alignment == 0 ? std::min(width - 1, int(std::round(unit(settings.x, 0) * width))) : 0;
        result.y = settings.alignment == 0 ? std::min(height - 1, int(std::round(unit(settings.y, 0) * height))) : 0;
        result.width = std::clamp(int(std::round(unit(settings.width, 1) * width)), 1, width - result.x);
        result.height = std::clamp(int(std::round(unit(settings.height, 1) * height)), 1, height - result.y);
        const float aspect = settings.aspect == 1 ? 16.0f / 9 : settings.aspect == 2 ? 4.0f / 3 :
            settings.aspect == 3 ? 21.0f / 9 : settings.aspect == 4 ? 3.0f / 4 : 0;
        if (aspect > 0)
        {
            if (float(result.width) / result.height > aspect)
                result.width = std::max(1, int(std::round(result.height * aspect)));
            else result.height = std::max(1, int(std::round(result.width / aspect)));
        }
        if (settings.alignment == 1)
        {
            result.x = (width - result.width) / 2;
            result.y = (height - result.height) / 2;
        }
        return result;
    }

}
