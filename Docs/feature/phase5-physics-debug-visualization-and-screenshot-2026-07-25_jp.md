# Phase 5 検討案: 戦車物理の可視化とスクリーンショット

日付: 2026-07-25

## 目的

戦車の物理挙動を画面から理解しやすくする。

また、開発者がスマートフォンから Codex と会話している場合でも、TankSandbox の実行結果をスクリーンショットとして確認できるようにする。

本書はタスク案を保持するための検討メモであり、実装完了を示すものではない。

## 現状

- Tracked Vehicle シーンの戦車表示は単一の箱である。
- `TrackedVehicleTestState` が公開する描画向け情報は、主に車体の位置、回転、速度、sleeping 状態である。
- Jolt の転輪位置、接地状態、サスペンション長は Rendering 側へ公開されていない。
- 実行画面の確認には、開発 PC の画面を直接見る必要がある。

## タスク A: 戦車デバッグモデルの改善

### 目標

見栄えを優先した完成モデルではなく、車体姿勢、進行方向、左右履帯の動き、接地、サスペンションの状態を読み取れるデバッグモデルを作る。

### Increment A1: 車体姿勢を読みやすくする

Physics の公開 API は変更せず、現在の車体 transform だけを使用する。

- 車体を下部車体と上部構造に分ける。
- 車体前方を示すマーカーを追加する。
- 左右の履帯を別インスタンスとして表示する。
- 左右履帯に異なる色または明度を使用する。
- ロール、ピッチ、旋回方向を読み取りやすい形状にする。

この段階では、転輪や履帯は車体に固定された視覚表現とする。

### Increment A2: 転輪状態を公開する

Physics 側に Rendering 非依存の読み取り専用デバッグ状態を追加する。

候補:

```cpp
struct TrackedWheelState
{
    Vec3 position;
    Quat rotation;
    float suspensionLength;
    bool hasContact;
};
```

- Jolt 型を公開 API に出さない。
- `TankController` または `TrackedVehicleTest` から snapshot として取得する。
- Physics は Rendering に依存しない。
- CLI テストからも同じ値を検証できる構造にする。

### Increment A3: サスペンションと接地の可視化

- 左右各5個の転輪を物理位置に表示する。
- サスペンション長を転輪位置またはデバッグ線で表す。
- 接地中と非接地の転輪を色分けする。
- 左右履帯入力値を UI と色で確認できるようにする。
- 必要なら接地点と接地法線を Debug Rendering モジュールで描画する。

## タスク B: スクリーンショット機能

### 目標

TankSandbox の実行結果を画像ファイルへ保存し、Codex がその画像を会話内に表示できるようにする。

### Increment B1: 手動撮影

- ImGui にスクリーンショット用のボタンを追加する。
- `F12` キーでも撮影できるようにする。
- UI を含む最終 back buffer を PNG として保存する。
- 保存先を `Screenshots/` とする。
- 日時を含むファイル名を使用する。

ファイル名例:

```text
Screenshots/TankSandbox_2026-07-25_153012.png
```

### Increment B2: 自動撮影

スマートフォンからの確認と自動テストを支援するため、コマンドライン操作を追加する。

候補:

```text
TankSandbox.exe --capture-after-frames 120 --capture-path Screenshots/latest.png
```

- 指定フレーム数の経過後に撮影する。
- 必要に応じて撮影後に自動終了するオプションを追加する。
- 将来は起動する検証シーンも CLI から指定できるようにする。
- Codex は生成された画像を読み込み、会話内に表示する。

### 実装場所の検討

スクリーンショットは Renderer に関係するため Physics には置かない。

最初に RtPbrSurvey に既存の back-buffer capture または debug dump 機能があるか調査する。

- 汎用的な capture API が存在する場合、Tank は薄い UI/CLI 操作層だけを所有する。
- API が存在しない場合、RtPbrSurvey への改善リクエストを検討する。
- Tank 固有のファイル名、保存先、撮影タイミングは Tank 側が所有する。

## 推奨実装順

1. Increment A1: 車体、上部構造、左右履帯、前方マーカー。
2. Increment B1 の前提調査: RtPbrSurvey の capture 機能確認。
3. Increment B1: 手動 PNG 撮影。
4. Increment B2: CLI 自動撮影と撮影後終了。
5. Increment A2: 転輪状態 snapshot。
6. Increment A3: 転輪、接地、サスペンションの可視化。

## 変更単位

機能変更とリファクタリングを分離し、次のような小さいコミットを候補とする。

1. `Document tank debug visualization and screenshot plan`
2. `Improve tracked vehicle debug model`
3. `Expose tracked wheel debug state`
4. `Visualize wheel contact and suspension`
5. `Add manual screenshot capture`
6. `Add automated screenshot capture`

## 完了条件

- 戦車の前後、左右履帯、車体のロールとピッチを画面から判別できる。
- 転輪の接地とサスペンション変位を物理 snapshot に基づいて確認できる。
- UI または `F12` で PNG を保存できる。
- CLI から一定フレーム後のスクリーンショットを取得できる。
- 生成画像を Codex が読み込み、スマートフォン上の会話へ表示できる。
- Physics と Rendering の依存方向を崩さない。
- Debug ビルドと既存 CTest を維持する。
