#include "Rendering/MortarRangeGeometry.h"
#include <iostream>

int main()
{
    const auto vertices = Tank::Rendering::MortarRangeGeometry::BuildCircle({}, 10.0f, 16);
    const bool passed = vertices.size() == 16;
    if (!passed)
    {
        std::cerr << "FAIL MortarRangeGeometry\n";
        return 1;
    }
    std::cout << "PASS MortarRangeGeometry\n";
    return 0;
}
