#pragma once

#include <functional>
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Tank::Ui
{
    enum class SceneTreeDropPosition { Before, Child, After };

    struct SceneTreeItem
    {
        std::string id;
        std::string parentId;
        std::string label;
    };

    struct SceneTreeDrop
    {
        std::string sourceId;
        std::string targetId;
        SceneTreeDropPosition position;
    };

    enum class SceneTreeAction { Delete, Duplicate, Rename };
    struct SceneTreeCommand
    {
        std::string nodeId;
        SceneTreeAction action;
    };

    struct SceneTreeResult
    {
        std::optional<std::string> selectedId;
        std::optional<SceneTreeDrop> drop;
        std::optional<SceneTreeCommand> command;
    };

    // Rows use the top/bottom quarter for sibling insertion, and the middle half for parenting.
    constexpr SceneTreeDropPosition SceneTreeDropZone(float y, float height)
    {
        return y < height * 0.25f ? SceneTreeDropPosition::Before :
            y > height * 0.75f ? SceneTreeDropPosition::After : SceneTreeDropPosition::Child;
    }

    // Enforce tree invariants even when a caller's model callback is permissive.
    inline bool SceneTreeDropAllowed(const std::vector<SceneTreeItem>& items,
        const std::string& rootId, const SceneTreeDrop& drop)
    {
        if (drop.sourceId == rootId || drop.sourceId == drop.targetId ||
            (drop.targetId == rootId && drop.position != SceneTreeDropPosition::Child)) return false;
        const auto find = [&](const std::string& id)
        { return std::find_if(items.begin(), items.end(), [&](const auto& n) {return n.id == id;}); };
        const auto target = find(drop.targetId);
        if (target == items.end() || find(drop.sourceId) == items.end()) return false;
        std::string parent = drop.position == SceneTreeDropPosition::Child ? target->id : target->parentId;
        std::set<std::string> visited;
        while (parent != rootId)
        {
            if (parent == drop.sourceId || !visited.insert(parent).second) return false;
            const auto node = find(parent);
            if (node == items.end()) return false;
            parent = node->parentId;
        }
        return find(rootId) != items.end();
    }

    // No model mutation during traversal. The caller validates and applies delivered commands.
    SceneTreeResult SceneTreeUI(const char* id, const std::vector<SceneTreeItem>& items,
        const std::string& rootId, const std::string& selectedId,
        const std::function<bool(const SceneTreeDrop&)>& canDrop, bool revealSelection = false);
}
