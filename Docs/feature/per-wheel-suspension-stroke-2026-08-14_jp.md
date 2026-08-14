# 車輪別Suspension Stroke

日付: 2026-08-14

## 目的

各車輪のSuspension strokeを個別に調整し、車体姿勢、接地長、段差走破性、正立・反転時の挙動を比較できるようにする。

## Stable wheel slot

設定は現在のJolt wheel配列順ではなく、次の安定したslotで所有する。

- Track: Left / Right
- Surface: Lower / Upper
- Position: Front End / Road 1 / Road 2 / Road 3 / Road 4 / Rear End

最大24 slotとする。Road wheel数が2または3の場合も未使用slotの値を保存し、レイアウト切替で調整値を失わない。

## 設定値

各slotは`strokeMeters`を持つ。

- `suspensionMinLength = 0.3 * rideHeightScale`
- `suspensionMaxLength = suspensionMinLength + strokeMeters`
- 設定範囲: 0.0-0.5m

既存挙動を維持する初期値:

- End wheel: 0.0m
- Road wheel: `0.2 * rideHeightScale`

## JSON互換性

Tank settings schemaをVersion 15へ更新する。Version 14以前、または車輪別配列が無いJSONは、読み込んだ`rideHeightScale`から従来と同じstrokeを生成する。

## UI

`Suspension Stroke per Wheel` groupで使用中のslotを表示する。変更はPhysics再構築が必要なため、既存のPending色とApply & Resetを使用する。

## 検証

- JSON round tripと旧JSON migration
- Left/Right、Lower/Upper、Front/Road/Rearのslot mapping
- 個別strokeがJolt wheel stateの許容範囲へ反映されること
- 2/3/4 Road wheel layout
- 正立・反転の接地
- Debug buildとCTest
