#pragma once

#include "Map/MapManifest.h"
#include <DirectXMath.h>
#include <cmath>

namespace Tank::Rendering
{
    inline DirectX::XMFLOAT4X4 EnemyPlacementMatrix(const Map::EnemyPlacement& enemy)
    {
        DirectX::XMFLOAT4X4 matrix;
        DirectX::XMStoreFloat4x4(&matrix,
            DirectX::XMMatrixRotationRollPitchYaw(
                DirectX::XMConvertToRadians(enemy.rotationDegrees[0]),
                DirectX::XMConvertToRadians(enemy.rotationDegrees[1]),
                DirectX::XMConvertToRadians(enemy.rotationDegrees[2])) *
            DirectX::XMMatrixTranslation(enemy.position[0], enemy.position[1], enemy.position[2]));
        return matrix;
    }

    // ImGuizmo's Euler helpers use a different order. Match the map's
    // DirectX roll/pitch/yaw convention, including equivalent gimbal-lock poses.
    inline Map::EnemyPlacement EnemyPlacementFromGizmo(const Map::EnemyPlacement& source,
        const DirectX::XMFLOAT4X4& matrix, bool rotationChanged)
    {
        auto result = source;
        result.position = { matrix._41, matrix._42, matrix._43 };
        if (rotationChanged)
        {
            const float horizontal = std::sqrt(matrix._31 * matrix._31 + matrix._33 * matrix._33);
            const float pitch = std::atan2(-matrix._32, horizontal);
            const float yaw = horizontal > 1.0e-5f ? std::atan2(matrix._31, matrix._33)
                : std::atan2(-matrix._13, matrix._11);
            const float roll = horizontal > 1.0e-5f ? std::atan2(matrix._12, matrix._22) : 0.0f;
            result.rotationDegrees = { DirectX::XMConvertToDegrees(pitch),
                DirectX::XMConvertToDegrees(yaw), DirectX::XMConvertToDegrees(roll) };
        }
        return result;
    }
}
