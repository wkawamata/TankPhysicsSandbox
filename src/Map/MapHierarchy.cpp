#include "MapHierarchy.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace Tank::Map
{
    namespace
    {
        using namespace DirectX;
        XMMATRIX Matrix(const Transform& t)
        {
            if (t.worldMatrix)
            {
                XMFLOAT4X4 m;
                std::copy(t.worldMatrix->begin(), t.worldMatrix->end(), &m._11);
                return XMLoadFloat4x4(&m);
            }
            return XMMatrixScaling(t.scale[0], t.scale[1], t.scale[2]) *
                XMMatrixRotationRollPitchYaw(XMConvertToRadians(t.rotationDegrees[0]),
                    XMConvertToRadians(t.rotationDegrees[1]), XMConvertToRadians(t.rotationDegrees[2])) *
                XMMatrixTranslation(t.position[0], t.position[1], t.position[2]);
        }
        Transform FromMatrix(FXMMATRIX matrix)
        {
            XMFLOAT4X4 m;
            XMStoreFloat4x4(&m, matrix);
            for (float value : std::array<float, 16>{m._11, m._12, m._13, m._14, m._21, m._22, m._23, m._24,
                m._31, m._32, m._33, m._34, m._41, m._42, m._43, m._44})
                if (!std::isfinite(value)) throw std::runtime_error("Hierarchy produced a non-finite transform");
            Transform result;
            result.worldMatrix.emplace();
            std::copy(&m._11, &m._11 + 16, result.worldMatrix->begin());
            result.position = {m._41, m._42, m._43};
            XMVECTOR scale, rotation, translation;
            if (!XMMatrixDecompose(&scale, &rotation, &translation, matrix))
            {
                // Meshes retain the exact affine matrix. Markers use an orthogonal orientation.
                const auto x = XMVector3Normalize(matrix.r[0]);
                const auto z = XMVector3Normalize(XMVector3Cross(x, matrix.r[1]));
                const auto y = XMVector3Cross(z, x);
                rotation = XMQuaternionRotationMatrix(XMMATRIX(x, y, z, XMVectorSet(0, 0, 0, 1)));
                scale = XMVectorSet(XMVectorGetX(XMVector3Length(matrix.r[0])),
                    XMVectorGetX(XMVector3Length(matrix.r[1])), XMVectorGetX(XMVector3Length(matrix.r[2])), 1);
            }
            result.scale = {XMVectorGetX(scale), XMVectorGetY(scale), XMVectorGetZ(scale)};
            XMStoreFloat4x4(&m, XMMatrixRotationQuaternion(rotation));
            const float pitch = std::asin(std::clamp(-m._32, -1.0f, 1.0f));
            const bool regular = std::abs(std::cos(pitch)) > 0.00001f;
            result.rotationDegrees = {XMConvertToDegrees(pitch),
                XMConvertToDegrees(regular ? std::atan2(m._31, m._33) : std::atan2(-m._13, m._11)),
                XMConvertToDegrees(regular ? std::atan2(m._12, m._22) : 0.0f)};
            return result;
        }
    }

    std::vector<TreeNode> MapTree(const Manifest& m)
    {
        std::vector<TreeNode> result;
        for (const auto& n : m.nodes) result.push_back({n.id, n.name, n.parentId, NodeKind::Empty, n.id});
        for (const auto& n : m.instances)
            result.push_back({"mesh/" + n.id, n.id + " (" + n.asset + ")", n.parentId, NodeKind::Mesh, n.id});
        result.push_back({"player-start", "PlayerStartPoint", m.playerSpawnParentId, NodeKind::PlayerStartPoint, ""});
        for (const auto& n : m.startPoints)
            result.push_back({"start/" + n.id, "PlayerStartPoint (" + n.id + ")", n.parentId, NodeKind::PlayerStartPoint, n.id});
        for (const auto& n : m.enemies)
            result.push_back({"enemy/" + n.id, n.id + " (" + n.unitType + ")", n.parentId, NodeKind::Enemy, n.id});
        for (const auto& n : m.clearAreas)
            result.push_back({"area/" + n.id, n.name.empty() ? n.id : n.name, n.parentId, NodeKind::ClearArea, n.id});
        for (auto& node : result)
            if (const auto info = m.nodeInfo.find(node.id); info != m.nodeInfo.end() && !info->second.name.empty())
                node.name = info->second.name;
        std::map<std::string, size_t> rank;
        for (size_t i = 0; i < m.treeOrder.size(); ++i) rank.emplace(m.treeOrder[i], i);
        std::stable_sort(result.begin(), result.end(), [&](const auto& a, const auto& b)
        {
            const auto ar = rank.find(a.id), br = rank.find(b.id);
            return (ar == rank.end() ? m.treeOrder.size() : ar->second) <
                (br == rank.end() ? m.treeOrder.size() : br->second);
        });
        return result;
    }

    Manifest::NodeInfo GetNodeInfo(const Manifest& m, const std::string& id)
    {
        if (const auto found = m.nodeInfo.find(id); found != m.nodeInfo.end()) return found->second;
        return { !id.starts_with("start/"), true };
    }

    bool RenameNode(Manifest& m, const std::string& id, const std::string& name, std::string& error)
    {
        const auto tree = MapTree(m);
        if (std::none_of(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;}))
        { error = "Node does not exist"; return false; }
        if (name.find_first_not_of(" \t\r\n") == std::string::npos)
        { error = "Name must not be empty"; return false; }
        auto info = GetNodeInfo(m, id);
        info.name = name;
        m.nodeInfo[id] = std::move(info);
        error.clear();
        return true;
    }

    bool NodeIsEnabled(const Manifest& m, const std::string& id, bool visibility)
    {
        const auto tree = MapTree(m);
        auto current = id;
        std::set<std::string> visited;
        while (!current.empty())
        {
            if (!visited.insert(current).second) return false;
            const auto info = GetNodeInfo(m, current);
            if (!(visibility ? info.visible : info.active)) return false;
            const auto node = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == current;});
            if (node == tree.end()) return false;
            current = node->parentId;
        }
        return true;
    }

    bool SetNodeActive(Manifest& m, const std::string& id, bool active, std::string& error)
    {
        const auto tree = MapTree(m);
        const auto node = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;});
        if (node == tree.end()) {error = "Unknown node"; return false;}
        auto updated = m;
        if (active && node->kind == NodeKind::PlayerStartPoint)
            for (const auto& n : tree) if (n.kind == NodeKind::PlayerStartPoint)
            {auto info = GetNodeInfo(updated, n.id); info.active = false; updated.nodeInfo[n.id] = info;}
        auto info = GetNodeInfo(updated, id);
        info.active = active;
        updated.nodeInfo[id] = info;
        if (!ValidateHierarchy(updated, error)) return false;
        m = std::move(updated);
        return true;
    }

    std::optional<std::string> DuplicateNode(Manifest& m, const std::string& id, std::string& error)
    {
        const auto original = m;
        const auto tree = MapTree(original);
        const auto source = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;});
        if (source == tree.end() || id == "root") {error = "Root cannot be duplicated"; return std::nullopt;}
        auto updated = m;
        updated.treeOrder.clear();
        for (const auto& n : tree) updated.treeOrder.push_back(n.id);
        std::function<std::string(const TreeNode&, const std::string&)> copy = [&](const auto& node, const auto& parent)
        {
            std::string rawId;
            for (int i = 1;; ++i)
            {
                rawId = "copy-" + std::to_string(i);
                const auto current = MapTree(updated);
                if (std::none_of(current.begin(), current.end(), [&](const auto& n) {return n.objectId == rawId || n.id == rawId;})) break;
            }
            std::string newId;
            switch (node.kind)
            {
            case NodeKind::Empty:
                updated.nodes.push_back({rawId, node.name + " Copy", parent, NodeLocalTransform(original, node.id)});
                newId = rawId; break;
            case NodeKind::Mesh:
                for (auto n : original.instances) if (n.id == node.objectId)
                {n.id = rawId; n.parentId = parent; updated.instances.push_back(n);}
                newId = "mesh/" + rawId; break;
            case NodeKind::Enemy:
                for (auto n : original.enemies) if (n.id == node.objectId)
                {n.id = rawId; n.parentId = parent; updated.enemies.push_back(n);}
                newId = "enemy/" + rawId; break;
            case NodeKind::ClearArea:
                for (auto n : original.clearAreas) if (n.id == node.objectId)
                {n.id = rawId; n.name += " Copy"; n.parentId = parent; updated.clearAreas.push_back(n);}
                newId = "area/" + rawId; break;
            case NodeKind::PlayerStartPoint:
                updated.startPoints.push_back({rawId, NodeLocalTransform(original, node.id), parent});
                newId = "start/" + rawId; break;
            }
            auto info = GetNodeInfo(original, node.id);
            if (!info.name.empty()) info.name += " Copy";
            if (node.kind == NodeKind::PlayerStartPoint) info.active = false;
            updated.nodeInfo[newId] = info;
            updated.treeOrder.push_back(newId);
            for (const auto& child : tree) if (child.parentId == node.id) copy(child, newId);
            return newId;
        };
        const auto duplicate = copy(*source, source->parentId);
        if (!MoveNode(updated, duplicate, id, NodePlacement::After, error)) return std::nullopt;
        m = std::move(updated);
        return duplicate;
    }

    bool RemoveNode(Manifest& m, const std::string& id, std::string& error)
    {
        const auto tree = MapTree(m);
        const auto node = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == id;});
        if (id == "root" || node == tree.end()) {error = "Root cannot be removed"; return false;}
        if (std::any_of(tree.begin(), tree.end(), [&](const auto& n) {return n.parentId == id;}))
        {error = "Move or remove child nodes first"; return false;}
        auto updated = m;
        std::erase_if(updated.nodes, [&](const auto& n) {return n.id == id;});
        std::erase_if(updated.instances, [&](const auto& n) {return "mesh/" + n.id == id;});
        std::erase_if(updated.enemies, [&](const auto& n) {return "enemy/" + n.id == id;});
        std::erase_if(updated.clearAreas, [&](const auto& n) {return "area/" + n.id == id;});
        std::erase_if(updated.startPoints, [&](const auto& n) {return "start/" + n.id == id;});
        updated.nodeInfo.erase(id);
        std::erase(updated.treeOrder, id);
        if (id == "player-start")
        {
            if (updated.startPoints.empty()) {error = "The last StartPoint cannot be removed"; return false;}
            const auto promoted = updated.startPoints.front();
            const auto oldId = "start/" + promoted.id;
            updated.playerSpawn = promoted.transform;
            updated.playerSpawnParentId = promoted.parentId;
            updated.nodeInfo["player-start"] = GetNodeInfo(updated, oldId);
            updated.nodeInfo.erase(oldId);
            updated.startPoints.erase(updated.startPoints.begin());
            for (const auto& n : MapTree(updated))
                if (n.parentId == oldId)
                {
                    for (auto& a : updated.nodes) if (a.id == n.id) a.parentId = "player-start";
                    for (auto& a : updated.instances) if ("mesh/" + a.id == n.id) a.parentId = "player-start";
                    for (auto& a : updated.enemies) if ("enemy/" + a.id == n.id) a.parentId = "player-start";
                    for (auto& a : updated.clearAreas) if ("area/" + a.id == n.id) a.parentId = "player-start";
                    for (auto& a : updated.startPoints) if ("start/" + a.id == n.id) a.parentId = "player-start";
                }
            for (auto& entry : updated.treeOrder) if (entry == oldId) entry = "player-start";
        }
        for (auto& group : updated.enemyGroups)
            std::erase_if(group.members, [&](const auto& member) {return "enemy/" + member.enemyId == id;});
        std::erase_if(updated.enemyGroups, [](const auto& g) {return g.members.empty();});
        bool hasActiveStart = false;
        for (const auto& n : MapTree(updated))
            if (n.kind == NodeKind::PlayerStartPoint && GetNodeInfo(updated, n.id).active) hasActiveStart = true;
        if (!hasActiveStart)
        {
            for (const auto& n : MapTree(updated))
                if (n.kind == NodeKind::PlayerStartPoint && SetNodeActive(updated, n.id, true, error))
                {hasActiveStart = true; break;}
        }
        if (!hasActiveStart || !ValidateHierarchy(updated, error)) return false;
        m = std::move(updated);
        return true;
    }

    Transform NodeLocalTransform(const Manifest& m, const std::string& id)
    {
        for (const auto& n : m.nodes) if (n.id == id) return n.transform;
        for (const auto& n : m.instances) if ("mesh/" + n.id == id) return n.transform;
        if (id == "player-start") return m.playerSpawn;
        for (const auto& n : m.startPoints) if ("start/" + n.id == id) return n.transform;
        for (const auto& n : m.enemies) if ("enemy/" + n.id == id) return {n.position, n.rotationDegrees};
        for (const auto& n : m.clearAreas) if ("area/" + n.id == id) return ClearAreaTransform(n);
        throw std::runtime_error("Unknown map node: " + id);
    }

    bool SetNodeLocalTransform(Manifest& m, const std::string& id, const Transform& input)
    {
        auto t = input;
        t.worldMatrix.reset();
        for (auto& n : m.nodes) if (n.id == id) {n.transform = t; return true;}
        for (auto& n : m.instances) if ("mesh/" + n.id == id) {n.transform = t; return true;}
        if (id == "player-start") {m.playerSpawn = t; return true;}
        for (auto& n : m.startPoints) if ("start/" + n.id == id) {n.transform = t; return true;}
        for (auto& n : m.enemies) if ("enemy/" + n.id == id)
        {n.position = t.position; n.rotationDegrees = t.rotationDegrees; return true;}
        for (auto& n : m.clearAreas) if ("area/" + n.id == id)
        {n.center = t.position; n.size = t.scale; n.rotationDegrees = t.rotationDegrees; n.worldMatrix.reset(); return true;}
        return false;
    }

    bool ValidateHierarchy(const Manifest& m, std::string& error)
    {
        const auto tree = MapTree(m);
        std::map<std::string, const TreeNode*> byId;
        for (const auto& n : tree)
            if (n.id.empty() || !byId.emplace(n.id, &n).second)
            {error = "Map node IDs must be unique and nonempty"; return false;}
        if (!byId.contains("root") || byId.at("root")->kind != NodeKind::Empty || !byId.at("root")->parentId.empty())
        {error = "Root must be an empty node without a parent"; return false;}
        for (const auto& n : tree)
        {
            std::set<std::string> visited;
            auto id = n.id;
            while (id != "root")
            {
                if (!byId.contains(id) || !visited.insert(id).second || visited.size() > 128)
                {error = "Map hierarchy has a missing parent, cycle, or exceeds 128 levels"; return false;}
                id = byId.at(id)->parentId;
            }
        }
        size_t activeStarts = 0;
        for (const auto& n : tree)
            if (n.kind == NodeKind::PlayerStartPoint && GetNodeInfo(m, n.id).active)
            {
                ++activeStarts;
                if (!NodeIsEnabled(m, n.id)) {error = "Active StartPoint must have active ancestors"; return false;}
            }
        if (activeStarts != 1) {error = "Exactly one StartPoint must be active"; return false;}
        error.clear();
        return true;
    }

    bool SetNodeParent(Manifest& m, const std::string& id, const std::string& parentId, std::string& error)
    {
        Manifest updated = m;
        bool found = false;
        for (auto& n : updated.nodes) if (n.id == id && id != "root") {n.parentId = parentId; found = true;}
        for (auto& n : updated.instances) if ("mesh/" + n.id == id) {n.parentId = parentId; found = true;}
        if (id == "player-start") {updated.playerSpawnParentId = parentId; found = true;}
        for (auto& n : updated.startPoints) if ("start/" + n.id == id) {n.parentId = parentId; found = true;}
        for (auto& n : updated.enemies) if ("enemy/" + n.id == id) {n.parentId = parentId; found = true;}
        for (auto& n : updated.clearAreas) if ("area/" + n.id == id) {n.parentId = parentId; found = true;}
        if (!found) {error = "Cannot reparent Root or an unknown node"; return false;}
        if (!ValidateHierarchy(updated, error)) return false;
        m = std::move(updated);
        return true;
    }

    bool MoveNode(Manifest& m, const std::string& sourceId, const std::string& targetId,
        NodePlacement placement, std::string& error)
    {
        if (sourceId == "root" || sourceId == targetId ||
            (targetId == "root" && placement != NodePlacement::Child))
        { error = "Root cannot move or have siblings; a node cannot target itself"; return false; }
        const auto tree = MapTree(m);
        const auto target = std::find_if(tree.begin(), tree.end(), [&](const auto& n) {return n.id == targetId;});
        if (target == tree.end()) {error = "Unknown drop target"; return false;}
        auto updated = m;
        const auto parent = placement == NodePlacement::Child ? targetId : target->parentId;
        if (!SetNodeParent(updated, sourceId, parent, error)) return false;
        std::vector<std::string> order;
        for (const auto& n : tree) if (n.id != sourceId) order.push_back(n.id);
        auto anchor = targetId;
        if (placement == NodePlacement::Child)
            for (const auto& n : tree)
                if (n.parentId == targetId && n.id != sourceId) anchor = n.id;
        auto insert = std::find(order.begin(), order.end(), anchor);
        if (placement != NodePlacement::Before) ++insert;
        order.insert(insert, sourceId);
        updated.treeOrder = std::move(order);
        m = std::move(updated);
        error.clear();
        return true;
    }

    Transform NodeWorldTransform(const Manifest& m, const std::string& id)
    {
        std::string error;
        if (!ValidateHierarchy(m, error)) throw std::runtime_error(error);
        const auto tree = MapTree(m);
        auto matrix = Matrix(NodeLocalTransform(m, id));
        std::string parent;
        for (const auto& n : tree) if (n.id == id) parent = n.parentId;
        while (!parent.empty())
        {
            matrix *= Matrix(NodeLocalTransform(m, parent));
            const auto n = std::find_if(tree.begin(), tree.end(), [&](const auto& value) {return value.id == parent;});
            parent = n->parentId;
        }
        return FromMatrix(matrix);
    }

    bool NodeTransformFromWorld(const Manifest& m, const std::string& id,
        const std::array<float, 16>& worldMatrix, Transform& local, std::string& error)
    {
        try
        {
            const auto tree = MapTree(m);
            const auto n = std::find_if(tree.begin(), tree.end(), [&](const auto& value) {return value.id == id;});
            if (n == tree.end()) throw std::runtime_error("Unknown node");
            Transform t; t.worldMatrix = worldMatrix;
            auto matrix = Matrix(t);
            if (!n->parentId.empty()) matrix *= XMMatrixInverse(nullptr, Matrix(NodeWorldTransform(m, n->parentId)));
            XMVECTOR scale, rotation, translation;
            if (!XMMatrixDecompose(&scale, &rotation, &translation, matrix))
                throw std::runtime_error("This operation would introduce shear into the local transform");
            local = FromMatrix(matrix);
            local.worldMatrix.reset();
            error.clear(); return true;
        }
        catch (const std::exception& e) {error = e.what(); return false;}
    }

    bool ResolveHierarchy(const Manifest& local, Manifest& world, std::string& error, bool editor)
    {
        try
        {
            if (!ValidateHierarchy(local, error)) return false;
            auto result = local;
            for (auto& n : result.instances) {n.transform = NodeWorldTransform(local, "mesh/" + n.id); n.parentId = "root";}
            result.playerSpawn = NodeWorldTransform(local, "player-start");
            result.playerSpawnParentId = "root";
            for (auto& n : result.startPoints)
            {n.transform = NodeWorldTransform(local, "start/" + n.id); n.parentId = "root";}
            for (auto& n : result.enemies)
            {
                n.supportPlacementId.clear();
                if (n.supportedByParent)
                {
                    const auto tree = MapTree(local);
                    auto parent = n.parentId;
                    while (!parent.empty())
                    {
                        const auto support = std::find_if(local.instances.begin(), local.instances.end(),
                            [&](const auto& mesh) {return "mesh/" + mesh.id == parent && mesh.breakable;});
                        if (support != local.instances.end()) {n.supportPlacementId = support->id; break;}
                        const auto node = std::find_if(tree.begin(), tree.end(), [&](const auto& item) {return item.id == parent;});
                        if (node == tree.end()) break;
                        parent = node->parentId;
                    }
                }
                const auto t = NodeWorldTransform(local, "enemy/" + n.id);
                n.position = t.position; n.rotationDegrees = t.rotationDegrees; n.parentId = "root";
            }
            for (auto& n : result.clearAreas)
            {
                const auto t = NodeWorldTransform(local, "area/" + n.id);
                n.center = t.position; n.size = t.scale; n.rotationDegrees = t.rotationDegrees;
                n.worldMatrix = t.worldMatrix; n.parentId = "root";
            }
            std::erase_if(result.instances, [&](const auto& n) {return !NodeIsEnabled(local, "mesh/" + n.id, editor);});
            std::erase_if(result.enemies, [&](const auto& n) {return !NodeIsEnabled(local, "enemy/" + n.id, editor);});
            std::erase_if(result.clearAreas, [&](const auto& n) {return !NodeIsEnabled(local, "area/" + n.id, editor);});
            if (editor)
            {
                for (const auto& n : MapTree(local))
                {
                    auto info = GetNodeInfo(local, n.id);
                    info.visible = NodeIsEnabled(local, n.id, true);
                    result.nodeInfo[n.id] = info;
                }
            }
            else
            {
                for (const auto& n : local.startPoints)
                    if (GetNodeInfo(local, "start/" + n.id).active)
                        result.playerSpawn = NodeWorldTransform(local, "start/" + n.id);
                result.startPoints.clear();
                result.nodeInfo.clear();
                for (auto& group : result.enemyGroups)
                    std::erase_if(group.members, [&](const auto& member)
                    {return std::none_of(result.enemies.begin(), result.enemies.end(), [&](const auto& e) {return e.id == member.enemyId;});});
                std::erase_if(result.enemyGroups, [](const auto& group) {return group.members.empty();});
            }
            result.nodes = {{"root", "Root", "", {}}};
            world = std::move(result);
            error.clear(); return true;
        }
        catch (const std::exception& e) {error = e.what(); return false;}
    }
}
