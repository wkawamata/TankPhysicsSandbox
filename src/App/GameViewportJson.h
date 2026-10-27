#pragma once
#include "GameViewport.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Tank::App
{
    inline std::string SerializeGameViewport(const GameViewportSettings& s)
    {
        return nlohmann::json{{"version", 1}, {"enabled", s.enabled}, {"x", s.x}, {"y", s.y},
            {"width", s.width}, {"height", s.height}, {"aspect", s.aspect}}.dump(2);
    }

    inline bool DeserializeGameViewport(const std::string& text, GameViewportSettings& output, std::string& error)
    {
        try
        {
            const auto json = nlohmann::json::parse(text);
            if (json.at("version") != 1 || !json.at("enabled").is_boolean() || !json.at("aspect").is_number_integer())
                throw std::runtime_error("Invalid viewport settings version or type");
            GameViewportSettings s;
            s.enabled = json.at("enabled").get<bool>(); s.aspect = json.at("aspect").get<int>();
            if (s.aspect < 0 || s.aspect > 3) throw std::runtime_error("Invalid aspect selection");
            for (auto [name, value] : {std::pair{"x", &s.x}, {"y", &s.y}, {"width", &s.width}, {"height", &s.height}})
            {
                if (!json.at(name).is_number()) throw std::runtime_error("Viewport values must be numbers");
                *value = json.at(name).get<float>();
                if (!std::isfinite(*value) || *value < 0 || *value > 1)
                    throw std::runtime_error("Viewport values must be between 0 and 1");
            }
            if (s.width <= 0 || s.height <= 0) throw std::runtime_error("Viewport size must be positive");
            output = s; error.clear(); return true;
        }
        catch (const std::exception& e) {error = e.what(); return false;}
    }
}
