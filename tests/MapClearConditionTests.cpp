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
    std::cout << "Map clear condition tests passed.\n";
    return 0;
}
