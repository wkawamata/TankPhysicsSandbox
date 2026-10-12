#include "NavigationGrid.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include <stdexcept>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

namespace Tank::Physics
{
    namespace
    {
        bool Nonnegative(float value) { return std::isfinite(value) && value >= 0; }
        bool Positive(float value) { return std::isfinite(value) && value > 0; }
        bool ValidCell(const NavigationCell& cell)
        {
            return std::isfinite(cell.supportGradientXZ[0]) && std::isfinite(cell.supportGradientXZ[1]) &&
                (cell.supportHeightMeters || (cell.supportGradientXZ[0] == 0 && cell.supportGradientXZ[1] == 0)) &&
                (!cell.supportHeightMeters || std::isfinite(*cell.supportHeightMeters)) && !cell.surfaceId.empty() && std::isfinite(cell.position.x) &&
                std::isfinite(cell.position.y) && std::isfinite(cell.position.z) &&
                static_cast<size_t>(cell.terrain) < kNavigationTerrainCount &&
                Positive(cell.clearanceWidthMeters) && Positive(cell.clearanceHeightMeters) &&
                Nonnegative(cell.slopeDegrees) && cell.slopeDegrees <= 90 && Nonnegative(cell.waterDepthMeters);
        }
        bool ValidPassage(const NavigationPassage& passage)
        {
            return Positive(passage.clearanceWidthMeters) && Positive(passage.clearanceHeightMeters) &&
                Nonnegative(passage.stepMeters);
        }
    }

    bool IsValidNavigationMobility(const NavigationMobility& mobility)
    {
        return Positive(mobility.speedMetersPerSecond) && Positive(mobility.widthMeters) &&
            Positive(mobility.heightMeters) && Nonnegative(mobility.maximumSlopeDegrees) &&
            mobility.maximumSlopeDegrees < 90 && Nonnegative(mobility.maximumWaterDepthMeters) &&
            Nonnegative(mobility.maximumStepMeters) &&
            std::all_of(mobility.terrainCosts.begin(), mobility.terrainCosts.end(), Positive);
    }

    double NavigationDistance(const Vec3& from, const Vec3& to)
    {
        return std::hypot(double(to.x) - from.x, double(to.y) - from.y, double(to.z) - from.z);
    }

    bool CanTraverseNavigationCell(const NavigationCell& cell, const NavigationMobility& mobility)
    {
        const double planeSlope = std::atan(std::hypot(double(cell.supportGradientXZ[0]), double(cell.supportGradientXZ[1]))) *
            180.0 / 3.141592653589793;
        return ValidCell(cell) && IsValidNavigationMobility(mobility) && cell.passable &&
            cell.clearanceWidthMeters >= mobility.widthMeters && cell.clearanceHeightMeters >= mobility.heightMeters &&
            std::max(double(cell.slopeDegrees), planeSlope) <= mobility.maximumSlopeDegrees && cell.waterDepthMeters <= mobility.maximumWaterDepthMeters;
    }

    bool CanTraverseNavigationPassage(const NavigationCell& from, const NavigationCell& to,
        const NavigationPassage& passage, const NavigationMobility& mobility)
    {
        if (!ValidPassage(passage) || !CanTraverseNavigationCell(from, mobility) ||
            !CanTraverseNavigationCell(to, mobility)) return false;
        const double horizontal = std::hypot(double(to.position.x) - from.position.x,
            double(to.position.z) - from.position.z);
        const double slope = std::atan2(std::abs(double(to.position.y) - from.position.y), horizontal) *
            180.0 / 3.14159265358979323846;
        return NavigationDistance(from.position, to.position) > 0 &&
            passage.clearanceWidthMeters >= mobility.widthMeters && passage.clearanceHeightMeters >= mobility.heightMeters &&
            passage.stepMeters <= mobility.maximumStepMeters && slope <= mobility.maximumSlopeDegrees;
    }

    bool FindNavigationPath(const NavigationGrid& grid, size_t start, size_t goal,
        std::span<const NavigationMobility> members, NavigationPath& path, std::string& error)
    {
        const auto fail = [&](const char* message) { error = message; return false; };
        if (start >= grid.cells.size() || goal >= grid.cells.size() || members.empty() ||
            !std::all_of(members.begin(), members.end(), IsValidNavigationMobility))
            return fail("Invalid navigation endpoints or moving members");
        std::set<std::tuple<int, int, std::string>> keys;
        for (const auto& cell : grid.cells)
            if (!ValidCell(cell) || !keys.emplace(cell.x, cell.z, cell.surfaceId).second)
                return fail("Invalid or duplicate navigation cell");
        std::vector<std::vector<size_t>> outgoing(grid.cells.size());
        for (size_t i = 0; i < grid.links.size(); ++i)
        {
            const auto& link = grid.links[i];
            if (link.from >= grid.cells.size() || link.to >= grid.cells.size() || link.from == link.to ||
                !ValidPassage(link.passage)) return fail("Invalid navigation link");
            const auto& from = grid.cells[link.from];
            const auto& to = grid.cells[link.to];
            const auto dx = std::abs(static_cast<long long>(from.x) - to.x);
            const auto dz = std::abs(static_cast<long long>(from.z) - to.z);
            if (dx > 1 || dz > 1 || NavigationDistance(from.position, to.position) <= 0)
                return fail("Navigation links must join neighboring cells at distinct positions");
            outgoing[link.from].push_back(i);
        }
        const auto cellAllowed = [&](const NavigationCell& cell)
            { return std::all_of(members.begin(), members.end(), [&](const auto& member)
                { return CanTraverseNavigationCell(cell, member); }); };
        if (!cellAllowed(grid.cells[start]) || !cellAllowed(grid.cells[goal]))
            return fail("Navigation endpoint is not traversable");
        const auto timePerMeter = [&](NavigationTerrain terrain)
        {
            double time = 0;
            for (const auto& member : members)
                time = std::max(time, double(member.terrainCosts[static_cast<size_t>(terrain)]) / member.speedMetersPerSecond);
            return time;
        };
        const auto none = (std::numeric_limits<size_t>::max)();
        std::vector<double> distances(grid.cells.size(), (std::numeric_limits<double>::infinity)());
        std::vector<size_t> previousLink(grid.cells.size(), none);
        using Entry = std::pair<double, size_t>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        distances[start] = 0;
        queue.push({0, start});
        while (!queue.empty())
        {
            const auto [time, index] = queue.top();
            queue.pop();
            if (time != distances[index]) continue;
            if (index == goal) break;
            for (const size_t linkIndex : outgoing[index])
            {
                const auto& link = grid.links[linkIndex];
                const auto& from = grid.cells[index];
                const auto& to = grid.cells[link.to];
                if (!std::all_of(members.begin(), members.end(), [&](const auto& member)
                    { return CanTraverseNavigationPassage(from, to, link.passage, member); })) continue;
                const double candidate = time + NavigationDistance(from.position, to.position) * 0.5 *
                    (timePerMeter(from.terrain) + timePerMeter(to.terrain));
                if (candidate >= distances[link.to]) continue;
                distances[link.to] = candidate;
                previousLink[link.to] = linkIndex;
                queue.push({candidate, link.to});
            }
        }
        if (!std::isfinite(distances[goal])) return fail("No traversable navigation path");
        std::vector<size_t> reversed;
        for (size_t index = goal; index != start;)
        {
            const size_t link = previousLink[index];
            reversed.push_back(link);
            index = grid.links[link].from;
        }
        NavigationPath result;
        result.cells.push_back(grid.cells[start]);
        for (auto it = reversed.rbegin(); it != reversed.rend(); ++it)
        {
            result.cells.push_back(grid.cells[grid.links[*it].to]);
            result.passages.push_back(grid.links[*it].passage);
        }
        result.estimatedSeconds = distances[goal];
        result.gridRevision = grid.revision;
        path = std::move(result);
        error.clear();
        return true;
    }

    bool ValidateNavigationGrid(const NavigationGrid& grid, std::string& error)
    {
        std::set<std::tuple<int, int, std::string>> keys;
        for (const auto& cell : grid.cells)
            if (!ValidCell(cell) || !keys.emplace(cell.x, cell.z, cell.surfaceId).second)
            { error = "Invalid or duplicate navigation cell"; return false; }
        for (const auto& link : grid.links)
        {
            if (link.from >= grid.cells.size() || link.to >= grid.cells.size() || link.from == link.to ||
                !ValidPassage(link.passage)) { error = "Invalid navigation link"; return false; }
            const auto& from = grid.cells[link.from];
            const auto& to = grid.cells[link.to];
            if (std::abs(static_cast<long long>(from.x) - to.x) > 1 ||
                std::abs(static_cast<long long>(from.z) - to.z) > 1 || NavigationDistance(from.position, to.position) <= 0)
            { error = "Navigation links must join neighboring cells at distinct positions"; return false; }
        }
        error.clear();
        return true;
    }

    bool DeserializeNavigationGrid(const std::string& text, NavigationGrid& grid, std::string& error)
    {
        try
        {
            using Json = nlohmann::json;
            const auto json = Json::parse(text);
            if (!json.at("version").is_number_integer() || json.at("version") != 1)
                throw std::runtime_error("Unsupported navigation version");
            const auto number = [](const Json& value)
            {
                if (!value.is_number()) throw std::runtime_error("Navigation value must be numeric");
                const double n = value.get<double>();
                if (!std::isfinite(n) || std::abs(n) > (std::numeric_limits<float>::max)())
                    throw std::runtime_error("Navigation value must be a finite float");
                return static_cast<float>(n);
            };
            const auto integer = [](const Json& value)
            {
                if (!value.is_number_integer()) throw std::runtime_error("Cell coordinate must be an integer");
                const double n = value.get<double>();
                if (n < (std::numeric_limits<int>::min)() || n > (std::numeric_limits<int>::max)())
                    throw std::runtime_error("Cell coordinate exceeds int range");
                return value.get<int>();
            };
            const auto index = [](const Json& value)
            {
                if (!value.is_number_integer() || (value.is_number_integer() && !value.is_number_unsigned() && value.get<long long>() < 0))
                    throw std::runtime_error("Link index must be a nonnegative integer");
                return value.get<size_t>();
            };
            if (!json.at("cells").is_array() || !json.at("links").is_array())
                throw std::runtime_error("Navigation cells and links must be arrays");
            NavigationGrid candidate;
            for (const auto& entry : json.at("cells"))
            {
                NavigationCell cell;
                cell.x = integer(entry.at("x")); cell.z = integer(entry.at("z"));
                cell.surfaceId = entry.at("surfaceId").get<std::string>();
                const auto& position = entry.at("position");
                if (!position.is_array() || position.size() != 3) throw std::runtime_error("Navigation position must be XYZ");
                cell.position = {number(position[0]), number(position[1]), number(position[2])};
                const auto terrain = entry.at("terrain").get<std::string>();
                const std::array<std::string, 4> names = {"road", "ground", "rough", "marsh"};
                const auto found = std::find(names.begin(), names.end(), terrain);
                if (found == names.end()) throw std::runtime_error("Unknown navigation terrain");
                cell.terrain = static_cast<NavigationTerrain>(found - names.begin());
                cell.clearanceWidthMeters = number(entry.at("clearanceWidthMeters"));
                cell.clearanceHeightMeters = number(entry.at("clearanceHeightMeters"));
                cell.slopeDegrees = number(entry.at("slopeDegrees"));
                cell.waterDepthMeters = number(entry.at("waterDepthMeters"));
                if (!entry.at("passable").is_boolean()) throw std::runtime_error("passable must be boolean");
                cell.passable = entry.at("passable").get<bool>();
                if (entry.contains("supportHeightMeters")) cell.supportHeightMeters = number(entry.at("supportHeightMeters"));
                if (entry.contains("supportGradientXZ"))
                {
                    const auto& gradient = entry.at("supportGradientXZ");
                    if (!gradient.is_array() || gradient.size() != 2) throw std::runtime_error("Support gradient must be XZ");
                    cell.supportGradientXZ = {number(gradient[0]), number(gradient[1])};
                }
                candidate.cells.push_back(std::move(cell));
            }
            for (const auto& entry : json.at("links"))
                candidate.links.push_back({index(entry.at("from")), index(entry.at("to")),
                    {number(entry.at("clearanceWidthMeters")), number(entry.at("clearanceHeightMeters")),
                        number(entry.at("stepMeters"))}});
            if (!ValidateNavigationGrid(candidate, error)) return false;
            grid = std::move(candidate);
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool SerializeNavigationGrid(const NavigationGrid& grid, std::string& text, std::string& error)
    {
        if (!ValidateNavigationGrid(grid, error)) return false;
        using Json = nlohmann::json;
        Json json = {{"version", 1}, {"cells", Json::array()}, {"links", Json::array()}};
        const std::array<const char*, 4> names = {"road", "ground", "rough", "marsh"};
        for (const auto& cell : grid.cells)
        {
            json["cells"].push_back({{"x", cell.x}, {"z", cell.z}, {"surfaceId", cell.surfaceId},
                {"position", {cell.position.x, cell.position.y, cell.position.z}},
                {"terrain", names[static_cast<size_t>(cell.terrain)]}, {"passable", cell.passable},
                {"clearanceWidthMeters", cell.clearanceWidthMeters}, {"clearanceHeightMeters", cell.clearanceHeightMeters},
                {"slopeDegrees", cell.slopeDegrees}, {"waterDepthMeters", cell.waterDepthMeters}});
            if (cell.supportHeightMeters) json["cells"].back()["supportHeightMeters"] = *cell.supportHeightMeters;
            if (cell.supportGradientXZ[0] != 0 || cell.supportGradientXZ[1] != 0)
                json["cells"].back()["supportGradientXZ"] = cell.supportGradientXZ;
        }
        for (const auto& link : grid.links)
            json["links"].push_back({{"from", link.from}, {"to", link.to},
                {"clearanceWidthMeters", link.passage.clearanceWidthMeters},
                {"clearanceHeightMeters", link.passage.clearanceHeightMeters}, {"stepMeters", link.passage.stepMeters}});
        text = json.dump(2);
        error.clear();
        return true;
    }

    bool DeserializeNavigationMobility(const std::string& text, NavigationMobility& mobility, std::string& error)
    {
        try
        {
            const auto json = nlohmann::json::parse(text);
            if (!json.is_object()) throw std::runtime_error("Mobility must be an object");
            const auto number = [&](const char* name)
            {
                const auto& value = json.at(name);
                if (!value.is_number()) throw std::runtime_error(std::string(name) + " must be numeric");
                const double result = value.get<double>();
                if (!std::isfinite(result) || std::abs(result) > (std::numeric_limits<float>::max)())
                    throw std::runtime_error(std::string(name) + " must be a finite float");
                return static_cast<float>(result);
            };
            NavigationMobility candidate;
            candidate.speedMetersPerSecond = number("speedMetersPerSecond");
            candidate.widthMeters = number("widthMeters");
            candidate.heightMeters = number("heightMeters");
            candidate.maximumSlopeDegrees = number("maximumSlopeDegrees");
            candidate.maximumWaterDepthMeters = number("maximumWaterDepthMeters");
            candidate.maximumStepMeters = number("maximumStepMeters");
            const auto& costs = json.at("terrainCosts");
            const std::array<const char*, kNavigationTerrainCount> names = {"road", "ground", "rough", "marsh"};
            for (size_t i = 0; i < names.size(); ++i)
            {
                const auto& value = costs.at(names[i]);
                if (!value.is_number()) throw std::runtime_error("Terrain cost must be numeric");
                const double cost = value.get<double>();
                if (!std::isfinite(cost) || cost > (std::numeric_limits<float>::max)() || cost <= 0)
                    throw std::runtime_error("Terrain cost must be a positive finite float");
                candidate.terrainCosts[i] = static_cast<float>(cost);
            }
            if (!IsValidNavigationMobility(candidate)) throw std::runtime_error("Invalid mobility settings");
            mobility = candidate;
            error.clear();
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool SerializeNavigationMobility(const NavigationMobility& mobility, std::string& text, std::string& error)
    {
        if (!IsValidNavigationMobility(mobility)) { error = "Invalid mobility settings"; return false; }
        const nlohmann::json json = {
            {"speedMetersPerSecond", mobility.speedMetersPerSecond}, {"widthMeters", mobility.widthMeters},
            {"heightMeters", mobility.heightMeters}, {"maximumSlopeDegrees", mobility.maximumSlopeDegrees},
            {"maximumWaterDepthMeters", mobility.maximumWaterDepthMeters}, {"maximumStepMeters", mobility.maximumStepMeters},
            {"terrainCosts", {{"road", mobility.terrainCosts[0]}, {"ground", mobility.terrainCosts[1]},
                {"rough", mobility.terrainCosts[2]}, {"marsh", mobility.terrainCosts[3]}}}};
        text = json.dump(2);
        error.clear();
        return true;
    }
}
