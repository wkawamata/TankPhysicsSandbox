# Tank Physics Sandbox

[English](README.md)

Tank Physics Sandbox は、リアルで説得力のある戦車車両挙動を開発するための C++20 物理サンドボックスです。

ゲーム性より先に、車両物理の基礎を作ることを重視します。将来自作 DirectX 12 エンジンや別ホストへ移植できるように、物理と描画は分離します。

## 目標

- Jolt Physics を使って戦車サンドボックスを作る。
- 物理を描画から独立させる。
- `TankController` を車両設計の中心にする。
- JSON でチューニングできるようにする。
- `RtPbrSurvey` を任意の DirectX 12 テスト描画システムとして使う。

## 現在の状態

- 最小 CMake アプリケーション shell がある。
- `RtPbrSurvey` は `External/RtPbrSurvey` に Git submodule として追加済み。
- Jolt 車両作業へ入る前に、renderer host integration を検討中。

## ビルド

Visual Studio 2022 CMake で configure/build します。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target TankSandbox
```

`cmake` が `PATH` に無い場合は、Visual Studio 付属の CMake 実行ファイルを使います。

## ドキュメント

- [プロジェクトとエージェント向けルール](AGENTS.md)
- [機能別ドキュメント](Docs/feature/)
