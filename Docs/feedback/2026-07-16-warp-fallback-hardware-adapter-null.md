# WARP Fallback for GetHardwareAdapter Null / D3D12 DLL Deployment

## What Failed
`TankSandbox.exe` 起動時に `GraphicsDevice.cpp:162` の `GetHardwareAdapter()` が `nullptr` を返し、`D3D12CreateDevice(nullptr, ...)` でクラッシュした。

## Root Cause (RtPbrSurvey 側)
`GetHardwareAdapter()` 内で D3D12 対応ハードウェアアダプターが見つからなかった場合のパスが未処理だった。関数はアダプターが見つからなくても `void` を返し、呼び出し元が nullptr のまま `D3D12CreateDevice` に渡していた。

## Root Cause (TankProject 側)
vcpkg の `winpixevent` パッケージ (`Microsoft::WinPixEventRuntime`)、および D3D12 Agility SDK のランタイム DLL 群が EXE 出力先に配置されていないと、D3D12 デバイス作成や PIX マーカー機能が正しく動作しない可能性がある。

ビルド後の出力先に必要な DLL:
- `WinPixEventRuntime.dll` — vcpkg の CMake 統合により自動コピーされる（確認済み）
- `D3D12/D3D12Core.dll` — Agility SDK ランタイム（明示的配置が必要）
- `D3D12/D3D12SDKLayers.dll` — デバッグレイヤー（Debug ビルドのみ）
- `dxcompiler.dll`, `dxil.dll` — シェーダーコンパイル用

## Fix

### RtPbrSurvey 側: WARP フォールバック
`GraphicsDevice.cpp:159-164` に WARP フォールバックを追加。`GetHardwareAdapter` が nullptr を返した場合、`EnumWarpAdapter` で WARP デバイスを作成する。

```cpp
if (hardwareAdapter.Get() == nullptr)
{
    ComPtr<IDXGIAdapter> warpAdapter;
    ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
    ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
}
```

### TankProject 側: DLL 配置
D3D12 Agility SDK の DLL (`D3D12Core.dll`, `D3D12SDKLayers.dll`) を EXE 出力先の `D3D12/` サブフォルダにコピーする仕組みが必要（プロジェクト設定、CMake post-build イベント、または手動配置）。

## Prevention Checklist
- `GetHardwareAdapter` の戻り値（nullptr の可能性）を常にチェックする
- D3D12 デバイス作成には必ずフォールバックパス（WARP）を用意する
- Remote Desktop / VM 環境でも起動するよう、ハードウェアアダプター不在を想定する
- D3D12 Agility SDK の DLL は EXE 出力先の `D3D12/` に常駐させる
- `WinPixEventRuntime.dll` の自動コピーが機能しているか CI 確認する
