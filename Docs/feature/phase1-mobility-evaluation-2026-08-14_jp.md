# Phase 1 車両機動性評価

日付: 2026-08-14

## 目的

車両の調整を感覚だけで行わず、同じ条件で繰り返し比較できる数値を残す。
評価処理は描画に依存せず、`TankPhysicsCli`から実行する。

## 実行方法

```bat
build\Debug\TankPhysicsCli.exe --test mobility --tank-settings tests\data\tank_mobility.json --settle-steps 180 --dt 0.0166667
```

全評価を一括実行する場合:

```bat
build\Debug\TankPhysicsCli.exe --test mobility-all --tank-settings tests\data\tank_mobility.json --settle-steps 180 --dt 0.0166667
```

`mobility-all`は基礎機動性、坂道保持・横滑り、段差乗越え、正立・反転比較を順番に実行し、最後に各評価の終了コードをまとめて表示する。

## レポート保存

Git管理snapshotを評価する:

```bat
scripts\run-mobility-evaluation.bat Snapshot
```

現在のTank Slot 1を評価する:

```bat
scripts\run-mobility-evaluation.bat Slot1
```

レポートは`build/Reports/Mobility/mobility-<settings>-<timestamp>.txt`へUTF-8で保存される。実行日時、設定ファイルの絶対パス、Git commit、全評価出力、終了コードを含む。調整前後でレポートを残すことで、性能値の変化を比較できる。

`tests/data/tank_mobility.json`は、2026-08-14時点の`build/Config/tank_physics_slot1.json`をGit管理用にsnapshotしたTank Slot 1基準データである。現在保存されているSlot 1を直接評価する場合は、build directoryをworking directoryとして次を実行する。

```bat
Debug\TankPhysicsCli.exe --test mobility-all --tank-settings Config\tank_physics_slot1.json --settle-steps 180 --dt 0.0166667
```

評価では1000m四方の平坦な床を使用する。これは最高速・制動試験中に車両が既定床の端から落下することを防ぐためであり、車両物理パラメータは変更しない。

## 測定項目

- 最高速度
- 0-10 m/s加速時間
- 全ブレーキで0.2 m/s未満になるまでの制動距離と時間
- 通常旋回、片履帯入力、超信地旋回の走行距離と累積Yaw変化から求めた旋回半径
- 超信地旋回中の最大Yaw速度
- 左右履帯の接地点間距離
- 接地面の平均傾斜角
- サスペンション長の最小値と最大値
- 接地輪の平均サスペンションImpulse

## 初期基準値

`tests/data/tank_mobility.json`を使用した初回測定値:

- 最高速度: 27.87 m/s（約100.3 km/h）
- 0-10 m/s: 1.48 s
- 制動距離: 36.24 m
- 制動時間: 2.65 s
- 通常旋回半径（Left 0.6 / Right 1.0）: 10.04 m
- 片履帯入力半径（Left 0.0 / Right 1.0）: 23.16 m
- 超信地旋回半径（Left -1.0 / Right 1.0）: 0.33 m
- 超信地旋回の最大Yaw速度: 100.00 degrees/s
- 左右接地長: 2.35 m / 2.35 m
- 接地角度: 0.00 degrees
- サスペンション範囲: 0.33-0.55 m
- 平均サスペンションImpulse: 163.37 Ns

### 坂道保持と横滑り

坂道保持と横滑りは必要な傾斜条件が異なるため、別条件で測定する。

- 坂道保持: 20.6 degrees（高さ9m / 長さ24m）、全ブレーキ、5秒
- 横滑り: 36.9 degrees（高さ18m / 長さ24m）、既定Neutral Brake、車体を斜面に対して横向き、5秒

| Ramp friction | Full-brake hold slide | Lateral slide |
| --- | ---: | ---: |
| 0.3 | 0.053 m | 0.00445 m |
| 0.6 | 0.049 m | 0.000315 m |
| 1.0 | 0.048 m | 0.000172 m |

全ブレーキ保持ではTerrain frictionによる単調な差は出ておらず、ブレーキトルクが支配的と考えられる。横滑りでは低摩擦時の変位が明確に大きい。

### 段差乗越え

幅20m、進行方向厚さ4m、摩擦1.0のBox段差へ全開前進し、車体中心が段差の奥端を2m越えるまでを測定する。各高さは独立したPhysics Worldで評価し、制限時間は8秒とする。

| Step height | Result | Traversal time |
| --- | --- | ---: |
| 0.25 m | PASS | 1.97 s |
| 0.50 m | PASS | 2.47 s |
| 0.75 m | FAIL | - |
| 1.00 m | FAIL | - |
| 1.25 m | FAIL | - |
| 1.50 m | FAIL | - |

現在の最大成功高さは0.50m。これは0.25m刻みの探索結果であり、厳密な限界高さではない。

### 正立・反転性能

Assault固有要件として、正立状態と上下反転状態を別々のPhysics Worldで評価する。5秒の全開直進と3秒の超信地旋回を行い、反転時は上面側Wheel surfaceの接地も確認する。

| Metric | Upright | Inverted | Inverted / Upright |
| --- | ---: | ---: | ---: |
| Forward distance | 58.20 m | 58.20 m | 1.0000 |
| Maximum speed | 17.81 m/s | 17.81 m/s | 1.0000 |
| 0-10 m/s | 1.48 s | 1.48 s | 1.0000 |
| Pivot yaw travel | 322.93 degrees | 322.93 degrees | 1.0000 |
| Contact wheels | lower 8 | upper 8 | - |

直進距離比と超信地旋回量比は0.9-1.1を回帰条件とする。現在の車両は正立・反転でほぼ対称な機動性能を持つ。

数値は現時点の比較用baselineであり、実車として妥当な目標値を意味しない。

## 旋回入力の定義

- 通常旋回: 両履帯を同方向へ動かし、内側を遅くする。
- 片履帯入力: 一方を0、他方を前進入力にする。
- 信地旋回: `stationaryTurnInnerTrackRatio`が0の片履帯入力。内側履帯は停止する。
- 緩和した信地旋回: `stationaryTurnInnerTrackRatio`を0より大きくし、内側履帯にも同方向の駆動を残す。
- 超信地旋回: 左右履帯を互いに逆方向へ動かす。

旋回半径は評価中のXZ走行距離を累積Yaw変化量（radians）で割って求める。超信地旋回のようにほぼその場で回転する場合は0に近づく。

## 判定方針

現在のCTestは、シミュレーション結果が有限値で、加速測定、旋回半径、接地情報を取得できることを検証する。調整値そのものを狭い範囲へ固定するとチューニングを妨げるため、性能目標の上下限は実機評価方針が決まった後に追加する。

## 次の評価

Phase 1の基礎数値評価は完了した。今後は各値の実車・ゲーム要件上の目標範囲を決め、設定変更前後の比較レポートを保存する。
