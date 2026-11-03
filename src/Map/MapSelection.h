#pragma once
#include "MapHierarchy.h"

namespace Tank::Map
{
    enum class SelectionTransform { Translate, Rotate, Scale };
    // The final entry is the active element / gizmo origin.
    std::vector<std::string> SelectNode(std::vector<std::string> selection,
        const std::string& id, bool extend);
    std::vector<std::string> ValidSelection(const Manifest& manifest, const std::vector<std::string>& ids);
    bool CanScaleSelection(const Manifest& manifest, const std::vector<std::string>& ids);
    bool TransformSelection(Manifest& manifest, const std::vector<std::string>& ids,
        const std::array<float,16>& referenceBefore, const std::array<float,16>& referenceAfter,
        SelectionTransform operation, bool localAxes, std::string& error);
    bool DuplicateSelection(Manifest& manifest, const std::vector<std::string>& ids,
        std::vector<std::string>& duplicates, std::string& error);
    bool RemoveSelection(Manifest& manifest, const std::vector<std::string>& ids, std::string& error);
}
