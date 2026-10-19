#include "SceneTreeUI.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <set>

namespace Tank::Ui
{
    SceneTreeResult SceneTreeUI(const char* id, const std::vector<SceneTreeItem>& items,
        const std::string& rootId, const std::string& selectedId,
        const std::function<bool(const SceneTreeDrop&)>& canDrop, bool revealSelection)
    {
        SceneTreeResult result;
        ImGui::PushID(id);
        char payloadType[32];
        std::snprintf(payloadType, sizeof(payloadType), "SCENE_TREE_%08X", ImGui::GetID("Payload"));
        std::set<std::string> revealParents;
        if (revealSelection)
        {
            auto current = selectedId;
            while (true)
            {
                const auto node = std::find_if(items.begin(), items.end(), [&](const auto& n) {return n.id == current;});
                if (node == items.end() || node->parentId.empty() || !revealParents.insert(node->parentId).second) break;
                current = node->parentId;
            }
        }
        std::set<std::string> visited;
        std::function<void(const SceneTreeItem&)> draw = [&](const SceneTreeItem& item)
        {
            if (!visited.insert(item.id).second) return;
            const bool leaf = std::none_of(items.begin(), items.end(), [&](const auto& n) {return n.parentId == item.id;});
            ImGui::PushID(item.id.c_str());
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (item.id == selectedId) flags |= ImGuiTreeNodeFlags_Selected;
            if (item.id == rootId) flags |= ImGuiTreeNodeFlags_DefaultOpen;
            if (leaf) flags |= ImGuiTreeNodeFlags_Leaf;
            if (revealParents.contains(item.id)) ImGui::SetNextItemOpen(true);
            const bool open = ImGui::TreeNodeEx("##Node", flags, "%s", item.label.c_str());
            if (revealSelection && item.id == selectedId) ImGui::SetScrollHereY();
            const auto rowMin = ImGui::GetItemRectMin();
            const auto rowMax = ImGui::GetItemRectMax();
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) result.selectedId = item.id;
            if (item.id != rootId && ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload(payloadType, item.id.c_str(), item.id.size() + 1);
                ImGui::TextUnformatted(item.label.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                const auto* payload = ImGui::GetDragDropPayload();
                if (payload && payload->IsDataType(payloadType) && payload->DataSize > 0)
                {
                    const SceneTreeDrop command{static_cast<const char*>(payload->Data), item.id,
                        SceneTreeDropZone(ImGui::GetMousePos().y - rowMin.y, rowMax.y - rowMin.y)};
                    const bool allowed = SceneTreeDropAllowed(items, rootId, command) && canDrop(command);
                    auto* painter = ImGui::GetWindowDrawList();
                    if (allowed)
                    {
                        const auto* accepted = ImGui::AcceptDragDropPayload(payloadType,
                            ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
                        const auto color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
                        if (command.position == SceneTreeDropPosition::Child)
                        {
                            auto fill = ImGui::GetStyleColorVec4(ImGuiCol_DragDropTarget);
                            fill.w = 0.22f;
                            painter->AddRectFilled(rowMin, rowMax, ImGui::GetColorU32(fill));
                            painter->AddRect(rowMin, rowMax, color, 0, 0, 2.0f);
                        }
                        else
                        {
                            const float y = command.position == SceneTreeDropPosition::Before ? rowMin.y : rowMax.y;
                            painter->AddLine(ImVec2(rowMin.x, y), ImVec2(rowMax.x, y), color, 2.0f);
                            painter->AddCircleFilled(ImVec2(rowMin.x + 3.0f, y), 3.0f, color);
                        }
                        if (accepted && accepted->IsDelivery()) result.drop = command;
                    }
                    else
                    {
                        painter->AddRect(rowMin, rowMax, ImGui::GetColorU32(ImVec4(1.0f, 0.25f, 0.2f, 0.8f)));
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginPopupContextItem("NodeContext"))
            {
                if (selectedId != item.id) result.selectedId = item.id;
                if (ImGui::MenuItem("Delete", nullptr, false, item.id != rootId && leaf))
                    result.command = SceneTreeCommand{item.id, SceneTreeAction::Delete};
                if (ImGui::MenuItem("Duplicate", nullptr, false, item.id != rootId))
                    result.command = SceneTreeCommand{item.id, SceneTreeAction::Duplicate};
                if (ImGui::MenuItem("Rename"))
                    result.command = SceneTreeCommand{item.id, SceneTreeAction::Rename};
                ImGui::EndPopup();
            }
            if (open)
            {
                for (const auto& child : items) if (child.parentId == item.id) draw(child);
                ImGui::TreePop();
            }
            ImGui::PopID();
        };
        const auto root = std::find_if(items.begin(), items.end(), [&](const auto& n) {return n.id == rootId;});
        if (root != items.end()) draw(*root);
        ImGui::PopID();
        return result;
    }
}
