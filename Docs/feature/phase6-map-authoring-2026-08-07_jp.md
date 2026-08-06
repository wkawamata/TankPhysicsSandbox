# Phase 6: Map Authoring

日付: 2026-08-07

## 目的

Tank Physics SandboxのMapは、UIとheadless CLIで共有するデータ駆動の検証Sceneである。Map定義はPhysicsが所有し、Renderingは同じprimitive dataを可視化するだけとする。

Map fileは`Config/Maps/`に置き、version付きJSONを使用する。現在のschema versionは`3`である。

## 座標系

- 単位はmeter。
- Y正方向が上。
- spawn yawが0のとき、戦車はZ正方向を向く。
- `position`はboxまたはtriangular prismの中心位置。
- `yawRadians`はprimitiveまたはspawn poseをY正軸の周りに回転する。

## Document fields

- `version`: Schema version。新規fileは`3`を使用する。
- `name`: UIとCLIに表示する空ではない名前。
- `environment`: 床、摩擦、方眼、生成障害物の設定。
- `spawn.position`: 戦車の初期位置 `[x, y, z]`。
- `spawn.yawRadians`: 戦車の初期方向。単位はradian。
- `primitives`: static collisionと表示に使うprimitiveの配列。

互換性のためversion 1と2も読み込み可能。version 3で明示的なspawn poseを追加した。

## Primitive types

### Box

`shape: "box"`を使用する。`size`配列はX、Y、Z方向の全長を表す。

### Triangular Prism

`shape: "triangularPrism"`を使用する。`size`配列は全幅、高さ、長さを表す。Rampやwedgeに使用する。

### Height Field

`shape: "heightField"`を使用し、次を指定する。

- `sampleCount`: 各辺のsample数。2から256。
- `cellSizeM`: 隣接sample間の水平距離。
- `heights`: `sampleCount * sampleCount`個の有限な高さをrow-major順で格納する配列。

Height Field形状はこの3項目で決まる。`size`は共通primitive schemaとの互換用に保持する。

## 摩擦

各primitiveは`0.0`から`2.0`までの`friction`を持つ。Debug表示では次の色を使用する。

- 青: `0.45`未満。
- 緑: `0.45`以上`0.80`未満。
- 赤: `0.80`以上。

環境の床は独立した`floorFriction`を持ち、Tank設定とは別に保存する。

## 検証

JSONを編集または追加した後は、登録された全Mapを検証する。

```powershell
build\Debug\TankPhysicsCli.exe --test maps --map-directory Config\Maps
```

Mapに明確な走破目標がある場合は、決定的な走行検証も実行する。

```powershell
build\Debug\TankPhysicsCli.exe --test map --map Config\Maps\mobility_course.json --settle-steps 180 --steps 480 --dt 0.0166667 --throttle 1 --min-forward-distance 45 --min-final-y 0
```

`TankPhysics.AllMapFilesCli`はCTestからdirectory全体を検証する。コース固有のCTestは、期待する走行距離または到達高度が安定し、検証目的として有効な場合だけ追加する。

## Authoring rules

- Mapは決定的にし、physics testを反復できる規模に保つ。
- Surfaceの違いが見えるようにprimitiveの摩擦色を利用する。
- Spawnは障害物のない床の上に置き、沈下が収まる余裕を確保する。
- 個別Mapでは一つの障害物概念に集中し、統合検証だけでcombined courseを使う。
- Physics定義からrenderer APIおよびassetへの依存を入れない。
