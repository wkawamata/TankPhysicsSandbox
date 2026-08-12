# Phase 7: 車両機動性評価セット

日付: 2026-08-12

## 目的

複数のversion付きTank設定を、同じ決定論的Mapと入力列で走行させる。作成済みの機動性コースとheadless車両Telemetryを組み合わせ、描画に依存せずチューニング変更を比較できるようにする。

## 実行方法

```powershell
build\Debug\TankPhysicsCli.exe `
  --test mobility-suite `
  --map Config\Maps\mobility_course.json `
  --tank-settings-directory tests\data\mobility `
  --settle-steps 180 `
  --steps 480 `
  --dt 0.0166667 `
  --throttle 1
```

入力JSONはファイル名順に評価する。各`RESULT mobility-suite`行には次を出力する。

- 前進距離
- 最終高度
- 最高速度
- 0–10 m/s到達時間
- 最終Engine RPM
- Gear

入力不正または非有限な物理状態があればsuite全体を失敗させる。車両ごとの性能合格値は、目的が明確な既存Map test側で管理する。

## 初期比較設定

- `high_torque.json`: 900 Nm基準設定
- `low_torque.json`: 600 Nm比較設定

初回実測では、前進距離が93.32 m対79.27 mとなった。この値は観測値であり、固定スコアや合格閾値ではない。

## 検証

CTest target: `TankPhysics.MobilityEvaluationSuite`。

