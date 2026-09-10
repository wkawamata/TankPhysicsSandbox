# Rolling 後続TODO

作成日: 2026-09-11

## 保留中の実装

### 1. Roll Speed Multiplier

- `0.5` から `2.0` の範囲で、現行ローリング全体の時間感を連動調整する。
- `1.0` は現在の挙動を維持する。
- Roll Torque だけでなく、接近時ダンピング、90度以降の制動、姿勢安定、横移動制御を連動補正する。
- 75度の逆入力判断角と、車体幅基準の目標移動距離は倍率変更の対象外とする。
- CheatWindow 修正の完了後に着手する。

### 2. Rolling Parameters の独立プロファイル保存

- Rolling Parameters を Tank Parameters とは独立した JSON として保存する。
- `tank_physics_slot1.json` から `slot3.json` と同様に、3つの Rolling Profile Slot を用意する。
- JSON は `Config/` 配下に置き、Git管理する。`build/Config` のローカル生成物には置かない。
- 各スロットを UI から Save / Load / Apply で切り替えられるようにする。
- Tank の寸法・履帯・車高などは Tank Parameters に残し、Rolling Profile にはローリング専用の調整値だけを含める。

## 実装順

1. CheatWindow の導線と説明を完成する。
2. Roll Speed Multiplier を追加し、1.0 の既存挙動維持と 0.5 / 1.5 / 2.0 の回帰確認を行う。
3. Rolling Profile Slot 1-3 の JSON 保存・UI切替・回帰テストを追加する。
