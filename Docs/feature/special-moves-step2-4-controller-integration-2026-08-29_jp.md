# Step 2.4 TankController統合

`TankController::PostStep` の物理観測更新後に `MobilityStateMachine` を呼び出し、結果を `TankState.mobility` と `TrackedVehicleTestState.mobility` へ公開した。

```text
Physics step → 観測更新 → TankMotionObservation構築
             → 走行要求とdeltaTimeを状態機械へ入力
             → MobilityStateSnapshotをTankStateへ保存
```

走行要求はスロットル、左右履帯の基準値から構成する。ブレーキ単独は停止へ収束するため、走行要求としては扱わない。状態機械はTankSettingsから構築される。

確認結果:

- 安定後に `Stopped` を公開
- 走行要求直後に `Moving` を公開
- 入力解除後の惰性中は `Moving` を維持
- 制動後に `Stopped` へ戻る
- 関連3テスト成功、TankSandbox Debugビルド成功

次はStep 2.5で状態名、滞在時間、停止候補進行度、遷移理由を固定サマリーへ表示する。
