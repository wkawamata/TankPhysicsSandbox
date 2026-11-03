#include "MapSelection.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace Tank::Map
{
    std::vector<std::string> SelectNode(std::vector<std::string> selection, const std::string& id, bool extend)
    {
        if (!extend || id == "root") return {id};
        std::erase(selection, "root");
        const auto found = std::find(selection.begin(), selection.end(), id);
        if (found == selection.end()) selection.push_back(id);
        else selection.erase(found);
        return selection;
    }
    std::vector<std::string> ValidSelection(const Manifest& manifest, const std::vector<std::string>& ids)
    {
        const auto tree = MapTree(manifest);
        std::vector<std::string> result;
        for (const auto& id : ids)
            if (std::find(result.begin(), result.end(), id) == result.end() &&
                std::any_of(tree.begin(), tree.end(), [&](const auto& node) { return node.id == id; })) result.push_back(id);
        return result;
    }
    bool CanScaleSelection(const Manifest& manifest, const std::vector<std::string>& ids)
    {
        const auto tree = MapTree(manifest);
        return !ids.empty() && std::all_of(ids.begin(), ids.end(), [&](const auto& id)
        {
            const auto found = std::find_if(tree.begin(), tree.end(), [&](const auto& node) {return node.id == id;});
            return found != tree.end() && found->kind != NodeKind::Enemy && found->kind != NodeKind::PlayerStartPoint;
        });
    }
    namespace
    {
        std::map<std::string,std::string> Parents(const Manifest& manifest)
        {
            std::map<std::string,std::string> result;
            for (const auto& node : MapTree(manifest)) result[node.id] = node.parentId;
            return result;
        }
        int Depth(const std::map<std::string,std::string>& parents, std::string id)
        {
            int depth = 0;
            while (parents.contains(id) && !parents.at(id).empty() && depth < 129)
            {id = parents.at(id); ++depth;}
            return depth;
        }
    }
    bool TransformSelection(Manifest& manifest, const std::vector<std::string>& ids,
        const std::array<float,16>& referenceBefore, const std::array<float,16>& referenceAfter,
        SelectionTransform operation, bool localAxes, std::string& error)
    {
        using namespace DirectX;
        auto selection = ValidSelection(manifest, ids);
        if (selection.empty()) {error = "No selection"; return false;}
        if (operation == SelectionTransform::Scale && !CanScaleSelection(manifest, selection))
        {error = "Enemy and StartPoint sizes are fixed"; return false;}
        const auto load = [](const auto& data) {XMFLOAT4X4 matrix; std::copy(data.begin(), data.end(), &matrix._11); return XMLoadFloat4x4(&matrix);};
        XMVECTOR beforeScale, beforeRotation, beforePosition, afterScale, afterRotation, afterPosition;
        if (!XMMatrixDecompose(&beforeScale, &beforeRotation, &beforePosition, load(referenceBefore)) ||
            !XMMatrixDecompose(&afterScale, &afterRotation, &afterPosition, load(referenceAfter)))
        {error = "Gizmo transform cannot be decomposed"; return false;}
        const auto beforeR = XMMatrixRotationQuaternion(beforeRotation);
        const auto afterR = XMMatrixRotationQuaternion(afterRotation);
        const auto rotationDelta = localAxes ? afterR * XMMatrixTranspose(beforeR) : XMMatrixTranspose(beforeR) * afterR;
        const auto scaleRatio = XMVectorDivide(afterScale, beforeScale);
        XMFLOAT3 ratios; XMStoreFloat3(&ratios, scaleRatio);
        if (!(ratios.x > 0 && ratios.y > 0 && ratios.z > 0) ||
            !std::isfinite(ratios.x) || !std::isfinite(ratios.y) || !std::isfinite(ratios.z))
        {error = "Scale must remain positive and finite"; return false;}
        const auto parents = Parents(manifest);
        std::stable_sort(selection.begin(), selection.end(), [&](const auto& a, const auto& b) {return Depth(parents,a)<Depth(parents,b);});
        auto updated = manifest;
        for (const auto& id : selection)
        {
            auto matrix = load(*NodeWorldTransform(manifest, id).worldMatrix);
            const auto position = matrix.r[3];
            matrix.r[3] = XMVectorSet(0,0,0,1);
            if (operation == SelectionTransform::Translate) matrix.r[3] = position + afterPosition - beforePosition;
            else if (operation == SelectionTransform::Rotate)
            {
                XMVECTOR ownScale, ownRotation, ownPosition;
                if (!XMMatrixDecompose(&ownScale,&ownRotation,&ownPosition,matrix))
                {error = "Selection has a sheared world transform"; return false;}
                const auto rotation = XMMatrixRotationQuaternion(ownRotation);
                matrix = XMMatrixScalingFromVector(ownScale) * (localAxes ? rotationDelta * rotation : rotation * rotationDelta);
                matrix.r[3] = position;
            }
            else
            {matrix.r[0] *= ratios.x; matrix.r[1] *= ratios.y; matrix.r[2] *= ratios.z; matrix.r[3] = position;}
            XMFLOAT4X4 stored; XMStoreFloat4x4(&stored,matrix);
            std::array<float,16> desired; std::copy(&stored._11,&stored._11+16,desired.begin());
            Transform transform;
            if (!NodeTransformFromWorld(updated,id,desired,transform,error) || !SetNodeLocalTransform(updated,id,transform)) return false;
        }
        if (!ValidateHierarchy(updated,error)) return false;
        manifest = std::move(updated); error.clear(); return true;
    }
    bool DuplicateSelection(Manifest& manifest, const std::vector<std::string>& ids,
        std::vector<std::string>& duplicates, std::string& error)
    {
        const auto selection = ValidSelection(manifest,ids);
        if (selection.empty() || std::find(selection.begin(),selection.end(),"root") != selection.end())
        {error = "Select elements below Root"; return false;}
        const auto parents = Parents(manifest);
        const std::set<std::string> selected(selection.begin(),selection.end());
        auto updated = manifest;
        std::vector<std::string> result;
        for (const auto& id : selection)
        {
            bool covered = false;
            auto parent = parents.at(id);
            while (!parent.empty()) {if (selected.contains(parent)) {covered = true; break;} parent = parents.at(parent);}
            if (covered) continue;
            const auto copy = DuplicateNode(updated,id,error);
            if (!copy) return false;
            result.push_back(*copy);
        }
        manifest = std::move(updated); duplicates = std::move(result); error.clear(); return true;
    }
    bool RemoveSelection(Manifest& manifest, const std::vector<std::string>& ids, std::string& error)
    {
        auto selection = ValidSelection(manifest,ids);
        if (selection.empty() || std::find(selection.begin(),selection.end(),"root") != selection.end())
        {error = "Select elements below Root"; return false;}
        const auto parents = Parents(manifest);
        std::stable_sort(selection.begin(),selection.end(),[&](const auto& a,const auto& b) {return Depth(parents,a)>Depth(parents,b);});
        auto updated = manifest;
        if (std::find(selection.begin(),selection.end(),"player-start") != selection.end())
        {
            // Promote an unselected start so remaining selected IDs stay stable.
            const auto survivor = std::find_if(updated.startPoints.begin(),updated.startPoints.end(),[&](const auto& point)
                {return std::find(selection.begin(),selection.end(),"start/"+point.id) == selection.end();});
            if (survivor == updated.startPoints.end()) {error = "The last StartPoint cannot be removed"; return false;}
            std::rotate(updated.startPoints.begin(),survivor,survivor+1);
        }
        for (const auto& id : selection) if (!RemoveNode(updated,id,error)) return false;
        manifest = std::move(updated); error.clear(); return true;
    }
}
