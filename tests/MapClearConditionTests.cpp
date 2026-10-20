#include "Map/MapHierarchy.h"
#include <DirectXMath.h>
#include "Map/MapClearCondition.h"

#include <iostream>

int main()
{
    Tank::Map::Manifest manifest;
    manifest.clearAreas = {
        { "clear-area-1", "First", { 5.0f, 2.0f, -3.0f }, { 4.0f, 2.0f, 6.0f } },
        { "clear-area-2", "Second", { -5.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } }
    };
    const Tank::Map::ClearArea* center =
        Tank::Map::FindContainingClearArea(manifest, { 5.0f, 2.0f, -3.0f });
    const Tank::Map::ClearArea* boundary =
        Tank::Map::FindContainingClearArea(manifest, { 7.0f, 3.0f, 0.0f });
    const Tank::Map::ClearArea* second =
        Tank::Map::FindContainingClearArea(manifest, { -5.0f, 0.0f, 0.0f });
    const Tank::Map::ClearArea* outside =
        Tank::Map::FindContainingClearArea(manifest, { 7.01f, 3.0f, 0.0f });
    if (center == nullptr || center->name != "First" || boundary != center ||
        second == nullptr || second->name != "Second" || outside != nullptr)
    {
        std::cerr << "Map clear condition tests failed.\n";
        return 1;
    }
    Tank::Map::Manifest rotated;
    rotated.clearAreas.push_back({"rotated", "Rotated", {10,2,0}, {4,2,1}, "root", {0,90,0}});
    if (!Tank::Map::FindContainingClearArea(rotated, {10,2,1.9f}) ||
        Tank::Map::FindContainingClearArea(rotated, {11,2,0}))
    { std::cerr << "Rotated unit-box scale axes do not match hit region\n"; return 1; }
    rotated.nodes.push_back({"parent", "Parent", "root", {{4,2,-6}, {10,30,0}, {2,3,4}}});
    rotated.clearAreas[0].parentId = "parent";
    rotated.clearAreas[0].rotationDegrees = {20,45,10};
    const auto transform = Tank::Map::NodeWorldTransform(rotated, "area/rotated");
    DirectX::XMFLOAT4X4 matrix;
    std::copy(transform.worldMatrix->begin(), transform.worldMatrix->end(), &matrix._11);
    auto point = [&](float x)
    {
        DirectX::XMFLOAT3 world;
        DirectX::XMStoreFloat3(&world, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(x,0.2f,-0.3f,1),
            DirectX::XMLoadFloat4x4(&matrix)));
        return std::array<float,3>{world.x,world.y,world.z};
    };
    Tank::Map::Manifest resolved;
    std::string error;
    if (!Tank::Map::ResolveHierarchy(rotated, resolved, error) ||
        !Tank::Map::FindContainingClearArea(resolved, point(0.49f)) ||
        Tank::Map::FindContainingClearArea(resolved, point(0.51f)))
    { std::cerr << "Parented/sheared boundary must use the same unit-box transform\n"; return 1; }
    std::cout << "Map clear condition tests passed.\n";
    return 0;
}
