# Step 2.3 停止判定設定とJSON

停止判定の進入・離脱閾値、姿勢条件、停止確認時間を `TankSettings` に追加し、`TankSettingsJson` のschema version 17で保存・読込できるようにした。

追加項目:

- 線速度、角速度、履帯スリップの進入・離脱閾値
- サスペンション速度の進入・離脱閾値
- body upと平均接触法線の最低dot値
- 停止確認時間

既存JSONに項目がない場合は `TankSettings` の既定値を維持するため、旧JSONとの後方互換性を保つ。未知の将来versionは従来どおり拒否する。

`MobilityStateMachine(const TankSettings&)` を追加し、状態機械が `TankSettings` の値で構築できる境界も用意した。実際の `TankController` 統合はStep 2.4で行う。

`TankPhysics.TankSettingsJson` と `TankPhysics.MobilityStateMachine` は成功した。

次はStep 2.4で `TankController` のphysics step後に状態機械を更新し、`TankState`へsnapshotを公開する。
