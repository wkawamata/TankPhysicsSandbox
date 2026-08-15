# Tank-1 サスペンション基準比較

## 目的

Tank-1基準設定で平地走行中のサスペンションが最短長へ張り付く原因を、Spring FrequencyとDampingの一項目比較で切り分ける。

## 条件

- Tank設定: `tests/data/tank_mobility.json`
- CLI: `TankPhysicsCli --test mobility`
- timestep: `0.0166667 s`
- 車高: `rideHeightScale = 1.1`
- ストローク: 旧Version 14設定から生成される`0.22 m`
- 基準値: Frequency `1.0 Hz`、Damping `0.5`

`suspension_hard_point_ratio`は、加速開始から制動終了までの「接地中かつストロークを持つ車輪サンプル」に対し、Joltが最短長のハードポイントへ到達した割合である。

## 結果

| Frequency | Damping | Hard point ratio | Max impulse | Max compression speed | Max extension speed |
|---:|---:|---:|---:|---:|---:|
| 1.0 Hz | 0.5 | 90.65% | 389.9 Ns | 0.0579 m/s | 0.0485 m/s |
| 1.5 Hz | 0.5 | 0.71% | 616.1 Ns | 0.3538 m/s | 0.3259 m/s |
| 2.0 Hz | 0.5 | 0.00% | 263.6 Ns | 0.2528 m/s | 0.2529 m/s |
| 3.0 Hz | 0.5 | 0.00% | 252.5 Ns | 0.1287 m/s | 0.1282 m/s |
| 1.0 Hz | 1.0 | 93.77% | 389.6 Ns | 0.0552 m/s | 0.0321 m/s |

## 判断

- 基準の`1.0 Hz`は8000 kgのTank-1に対して柔らかすぎ、平地でもほぼ常時底付きしている。
- Dampingだけを`1.0`へ上げても底付きは改善しない。主因は減衰不足ではなくSpring Frequency不足である。
- `1.5 Hz`で底付きは大幅に減るが、過渡時の最大Impulseと伸縮速度が大きい。
- `2.0 Hz`は底付きを解消し、`1.5 Hz`より最大Impulseも小さい。
- `3.0 Hz`も安定するが、まずは硬くしすぎない`2.0 Hz / 0.5`をTank-1の暫定候補とする。

## 次の確認

実画面で`2.0 Hz / 0.5`を適用し、段差、Slope、旋回、制動時の車体姿勢と接地を確認する。問題がなければTank Slot 1へ保存し、Git管理用snapshotを更新する。
