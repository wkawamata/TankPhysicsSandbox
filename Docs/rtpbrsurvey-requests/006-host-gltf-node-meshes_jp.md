# Request 006: Host向けglTF Node Mesh

日付: 2026-08-13

## 日本語要約

- Tankでは`Assets/TankModels/TankModel-2026-08-13-v001.glb`をGit管理する。
- GLBには`Body`、`Cannon`、`Side`の3つのmesh nodeがある。
- アプリ起動時にassetを一度だけロードしたい。
- 各nodeを別々の`SceneMeshId`としてSceneへ追加したい。
- Dummy modelと重ねて表示し、Dummy、Body、Cannon、Sideを個別にON/OFFしたい。
- 現行`SceneBuilder::AddGltfMesh(path)`はdefault scene全体を1 meshへ平坦化するため、node別表示ができない。
- RtPbrSurveyにはTank固有APIではなく、backend-neutralなnode選択glTF APIを追加してほしい。
- node transform、material、textureを保持し、既存APIとの互換性も維持する。
- 複数nodeを含むfixtureでテストし、asset lifetimeを文書化する。

