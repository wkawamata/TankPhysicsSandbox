# Jolt Physics Discussion Chat

OpenCode Jolt Physics 検討用チャット。

---

[2026-07-15] **初期調査**

Jolt Physics を Tank Physics Sandbox に導入する方針を整理する。

## 現状

- Jolt はまだプロジェクトに未導入。
- `External/JoltPhysics` に submodule として配置する方向。
- 優先事項は `TrackedVehicleController` の検証。

## 検討課題

- submodule / FetchContent / package manager どちらで導入するか。
- 最小統合: world + ground body + 1 dynamic body。
- `TrackedVehicleController` の前に、まず basic rigid body 動作を確認する。

## OpenCode タスク候補

- Jolt の CMake 組み込み調査。
- 最小物理シーン (ground + cube) の C++ 実装。
- TankController のインターフェース設計。
