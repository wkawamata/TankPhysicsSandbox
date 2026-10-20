#include "MapClearCondition.h"
#include "MapHierarchy.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

const Tank::Map::ClearArea* Tank::Map::FindContainingClearArea(
    const Manifest& manifest, const std::array<float, 3>& point)
{
    using namespace DirectX;
    for (const ClearArea& area : manifest.clearAreas)
    {
        const auto transform = area.worldMatrix ? ClearAreaTransform(area) : NodeWorldTransform(manifest, "area/" + area.id);
        XMFLOAT4X4 stored;
        std::copy(transform.worldMatrix->begin(), transform.worldMatrix->end(), &stored._11);
        XMVECTOR determinant;
        const auto inverse = XMMatrixInverse(&determinant, XMLoadFloat4x4(&stored));
        if (!std::isfinite(XMVectorGetX(determinant)) || std::abs(XMVectorGetX(determinant)) < 1.0e-12f) continue;
        XMFLOAT3 local;
        XMStoreFloat3(&local, XMVector3TransformCoord(XMVectorSet(point[0], point[1], point[2], 1), inverse));
        constexpr float halfExtent = 0.5f + 1.0e-5f;
        if (std::abs(local.x) <= halfExtent && std::abs(local.y) <= halfExtent && std::abs(local.z) <= halfExtent)
            return &area;
    }
    return nullptr;
}
