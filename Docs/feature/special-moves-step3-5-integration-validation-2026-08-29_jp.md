# Step 3.5 統合テスト

`TrackedVehicleTest` の実測Mobility状態と、`SpecialActionRecognizer`、`SpecialMoveStateMachine` を組み合わせた統合テストを追加した。

- 安定停止後の外開き入力は迫撃開始要求として受理される。
- 走行要求後のMobility状態は `Moving` となる。
- `Moving` 中のローリング要求は `MobilityNotStopped` で拒否される。
- 保持入力は再発火せず、中立復帰後に再アームされる。
- `TankPhysics.SpecialMoveIntegration` は成功した。

このStepでは、ローリングや迫撃の物理トルク自体はまだ接続しない。実動作との接続はStep 4以降で行う。
