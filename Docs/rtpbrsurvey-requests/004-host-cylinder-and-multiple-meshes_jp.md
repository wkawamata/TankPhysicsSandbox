# 依頼 004: Host向け円柱PrimitiveとInstance単位のMesh選択

Status: requested

## 日本語要約

- Tankの転輪を16角形程度の低ポリゴン円柱で表示したい。
- 現在の`SceneBuilder`は、追加したPrimitiveを1つの共有`SceneMesh`へ結合する。
- この構造でCylinderを追加すると、既存Cube InstanceもCubeとCylinderの両方を描画してしまう。
- Cube、Sphere、Cylinder、ロードしたMeshを同じScene内で使い分けたい。
- 各Instanceが描画するMeshまたはPrimitiveを明示的に選択できる公開APIが必要。
- Cylinderは分割数をhost側で指定でき、16分割、上下cap付きで使用できること。
- Cylinder軸、法線、world transformとの規約を文書化すること。
- Tank、履帯、転輪、Jolt固有の概念はRtPbrSurveyへ追加しない。
- Tank側は寸法、配置、アニメーション、物理を所有する。
- CubeとCylinderを同時に配置するCMake hostテストを追加してほしい。
- upstream mainへmerge後、Tank側submoduleを更新して取り込む。
