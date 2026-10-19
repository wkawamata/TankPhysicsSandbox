#pragma once

#include "MapManifest.h"

namespace Tank::Map
{
    enum class NodeKind { Empty, Mesh, PlayerStartPoint, Enemy, ClearArea };
    struct TreeNode
    {
        std::string id;
        std::string name;
        std::string parentId;
        NodeKind kind;
        std::string objectId;
    };

    std::vector<TreeNode> MapTree(const Manifest& manifest);
    Manifest::NodeInfo GetNodeInfo(const Manifest& manifest, const std::string& id);
    bool NodeIsEnabled(const Manifest& manifest, const std::string& id, bool visibility = false);
    bool SetNodeActive(Manifest& manifest, const std::string& id, bool active, std::string& error);
    std::optional<std::string> DuplicateNode(Manifest& manifest, const std::string& id, std::string& error);
    bool RenameNode(Manifest& manifest, const std::string& id, const std::string& name, std::string& error);
    bool RemoveNode(Manifest& manifest, const std::string& id, std::string& error);
    Transform NodeLocalTransform(const Manifest& manifest, const std::string& id);
    bool SetNodeLocalTransform(Manifest& manifest, const std::string& id, const Transform& transform);
    bool SetNodeParent(Manifest& manifest, const std::string& id, const std::string& parentId, std::string& error);
    enum class NodePlacement { Before, Child, After };
    bool MoveNode(Manifest& manifest, const std::string& sourceId, const std::string& targetId,
        NodePlacement placement, std::string& error);
    bool ValidateHierarchy(const Manifest& manifest, std::string& error);
    Transform NodeWorldTransform(const Manifest& manifest, const std::string& id);
    bool NodeTransformFromWorld(const Manifest& manifest, const std::string& id,
        const std::array<float, 16>& worldMatrix, Transform& local, std::string& error);
    bool ResolveHierarchy(const Manifest& local, Manifest& world, std::string& error, bool editor = false);
}
