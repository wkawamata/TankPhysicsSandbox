# Map Editor 実装ステップ・進捗

- 最終更新: 2026-09-09
- workspace: `C:\work\TankPhysicsSandbox`
- 作業ブランチ: `codex/map-editor`（`main` から作成）
- 仕様: [Map Editor 仕様](map-editor-spec-2026-09-07_jp.md)

## 運用ルール

- 本書をMap Editor開発の進捗管理元とする。作業しながら調査結果に合わせて項目を分割・具体化・改変する。
- 状態は `TODO`（未着手）、`IN_PROGRESS`（作業中）、`DONE`（実装・必要な検証が完了）、`BLOCKED`（依存事項により進められない）を使用する。
- 着手時に状態と対象範囲を更新し、終了時に結果・検証・残作業を記録する。
- 作業済み項目は削除せず `DONE` とする。段階全体をDONEにするのは、その段階の完了条件を満たした時点とする。
- コードを書いただけでDONEにしない。関連テストとビルド、画面変更にはGUI確認を行い、未検証ならその旨を残す。
- 未確定の仕様を合意済みとして扱わない。決定した仕様は仕様書にも反映し、進捗との整合を保つ。
- 項目追加・順序変更時は、理由を更新履歴に短く記録する。
- 小さな変更単位でビルド可能な状態を保ち、無関係なリファクタリングは分離する。
- 本書を含め、プロジェクトのテキストファイルはCRLFとする。

## 全体の状態

| 段階 | 内容 | 状態 |
| --- | --- | --- |
| 0 | 仕様整理・Manifestデータ層 | DONE |
| 1 | エディタ画面とフォルダ操作 | DONE |
| 2 | glTFの読み込みと表示 | DONE |
| 3 | モデル配置の編集 | DONE |
| 4 | HitMeshの物理判定 | DONE |
| 5 | 開始地点とクリアエリアの編集 | DONE |
| 6 | マップを選択してプレイ | DONE |

段階0〜6の実装、自動テスト、必要なGUI確認が完了。

## 0. 仕様整理・Manifestデータ層 — DONE

- [x] DONE: 要件と未確定事項を仕様書に整理。
- [x] DONE: `main` から `codex/map-editor` を作成。
- [x] DONE: 描画・物理ライブラリに依存しない `TankMapCore` を追加。
- [x] DONE: モデル配置・戦車の開始地点・クリア判定AABBのデータ構造を追加。
- [x] DONE: Manifest version 1のJSONシリアライズ／デシリアライズを追加。
- [x] DONE: 有限座標、正のAABBサイズ、相対glTFパス、一意なIDを検証。
- [x] DONE: 読み込み失敗時に既存データが保持されることをテスト。
- [x] DONE: 新規テストと既存マップ関連テスト、アプリのビルドを確認。

検証記録（2026-09-07）:

- Debugビルド: `MapManifestTests`、`TankSandbox` 成功。
- テスト: `TankMap.Manifest`、`TankPhysics.MapDefinition`、`TankPhysics.MapDefinitionJson` の3件成功。
- 変更したソース・仕様書のCRLFを確認。

範囲の注意: 完了したのはメモリ上のデータとJSON文字列の変換。実ファイルの読み書き、フォルダ選択、glTF読み込み、画面機能は含まない。

## 1. エディタ画面とフォルダ操作 — DONE

- [x] DONE: トップメニューに `Map Editor` を追加し、エディタへの遷移とトップ画面への復帰を実装。
- [x] DONE: エディタ画面左上に `Open` を配置し、Mapフォルダを選択するダイアログを接続。
- [x] DONE: 選択中のフォルダと読み込み・保存の結果を画面に表示。
- [x] DONE: `Manifest.json` があれば読み込み、なければ新規作成。
- [x] DONE: `Save` によるManifestのファイル保存を実装。
- [x] DONE: 未保存変更の追跡を実装・テスト。遷移時は保存／破棄／キャンセル方式とし、確認ダイアログを接続。編集UIと組み合わせた動作検証は段階3で行う。
- [x] DONE: キャンセル、不正なManifest、読み書き失敗時の扱いを実装。既存データを誤って破棄・上書きしない。
- [x] DONE: ファイル入出力テスト、ビルド、GUI操作確認。

完了条件: トップ画面から入ったエディタで、空フォルダへのManifest新規作成と既存Manifestの読み込み・保存・再読み込みができる。

検証記録（2026-09-07）:

- Debugビルド: `MapFolderTests`、`MapManifestTests`、`TankSandbox` 成功。
- 関連テスト4件成功: `TankMap.Folder`、`TankMap.Manifest`、`TankPhysics.MapDefinition`、`TankPhysics.MapDefinitionJson`。
- ファイルテスト: 日本語・空白を含むフォルダ、新規作成、全項目の保存・復元、不正JSON、存在しないフォルダ、Manifestがディレクトリの場合、ロックによる読み書き失敗、失敗後の再試行、既存ファイル・未保存データの保持。
- GUI確認: トップメニューからエディタへ遷移、左上Open、空フォルダ選択によるManifest作成、Save、同フォルダ再読み込み、フォルダ選択キャンセル、Back to Menuでの復帰。
- GUI検証用データ: `build/MapEditorVerification/Empty/Manifest.json`（製品用マップとしては登録しない）。
- 保存は同じフォルダ内の一時ファイルへ書き込み・フラッシュ後に置換。新規作成時には既存ファイルを上書きしない。
- 未検証: 実際のモデル編集後の確認ダイアログ操作、アプリの閉じるボタン／Alt+F4での未保存保護。編集可能になる段階3の残作業として追跡する。

## 2. glTFの読み込みと表示 — DONE

### 2-A. モデル一覧 — DONE

- [x] DONE: フォルダ内の `.gltf` / `.glb` 一覧、選択、Refresh Modelsによる再検索。サブフォルダも再帰検索し、リンク・ジャンクション等のreparse pointは除外。UTF-8相対パスをソートして表示する。
- [x] DONE: 検索層の自動テストとTankSandboxのDebugビルド。空フォルダ・日本語パス・混在拡張子・入れ子・追加削除・無効なルート・Manifestとのパス互換性を確認。
- [x] DONE: モデル一覧と選択・再検索のGUI確認。サブフォルダ、大文字拡張子、`##`を含む名前の表示、追加反映、選択維持、削除後の選択解除を確認。

検証記録（2026-09-07）:

- Debugビルド: `MapAssetCatalogTests`、`MapFolderTests`、`MapManifestTests`、`TankSandbox` 成功。関連CTest 5件成功。
- シンボリックリンク除外のテストケースは、実行環境にリンク作成権限がなくスキップ。リンク・ジャンクションの実環境検証は残す。
- GUI検証フォルダ: `build/MapEditorVerification/Catalog`。一覧検証用の空ファイルを使用しており、有効な3Dモデルではない。
- 選択操作ではManifestを変更しない。glTFファイル内容の検証・3D表示・追加は2-Bの対象。

### 2-B. 役割識別・3D読み込み — DONE

- [x] DONE: `Visual` / `Hit` 親ノード方式をversion 1として確定し、仕様書へ反映。
- [x] DONE: 役割識別の仮規約を具体化。完全一致のVisual/Hit、両役割必須、入れ子禁止、ノード番号による識別。最終合意とは区別して仕様書へ記載。
- [x] DONE: glTF/GLBのJSON階層検査をTankMapCoreへ追加。循環・複数親・不正参照・壊れたGLBのエラーと出力保持をテスト。
- [x] DONE: Inspect Roles画面で正常時のVisual/Hit各1件とノード内訳、規約外Meshのエラー、選択変更時の旧結果クリアを確認。
- [x] DONE: GltfRolesTestsとTankSandboxのDebugビルド、関連CTest 6件成功。
- [x] DONE: 検査済みモデルを `Add to Map` でManifestの新規配置へ追加する。Visualノードはノード番号でレンダラーへ渡し、無名・重複名にも対応する。Hitノードは物理段階まで非表示とする。Triangle.gltfのInspect Roles、Add、Placed Modelsへの追加、SaveをGUI確認済み。
- [x] DONE: 配置済みモデルのVisualメッシュを再構築して3Dプレビューへ表示する。表示件数とテスト三角形の表裏を修正し、回転なしの初期姿勢で表示されることをGUI確認済み。
- [x] DONE: レンダラー側へノード番号によるglTF Mesh追加を追加し、既存の名前APIとの互換性を維持。Tank側CMakeから回帰テストを生成・実行できるようにし、重複名・祖先変換・回転・左右座標変換を確認。
- 範囲: JSON階層の検査のみ。GUI検証データは `build/MapEditorVerification/Roles` にあり、形状を持たないメタデータ用fixture。実モデルの3D読み込み成功を示すものではない。
- [x] DONE: 左手World座標、メートル単位、ManifestのXYZ回転順、glTFの右手系からの変換と階層変換をversion 1として確定。
- [x] DONE: 追加前に外部バッファ・テクスチャ・ファイル欠落などの読み込み失敗を判定し、Manifestを変更せずエラー表示する。
- [x] DONE: 役割識別と階層変換を自動テストし、規約に従うTriangle.gltfの役割表示と3D表示をGUI確認。

完了条件: 規約に従うglTFを選択して表示でき、表示用MeshとHitMeshの役割を区別して取り出せる。

## 3. モデル配置の編集 — DONE

- [x] DONE: World原点の水平グリッドをプレビューへ表示する。間隔・範囲・線幅を画面で調整でき、X=0軸を赤、Z=0軸を緑で強調する。グリッドと軸色をGUI確認済み。
- [x] DONE: 配置済みモデルの一覧と選択状態を実装。
- [x] DONE: World位置XYZ・回転XYZをDrag入力で編集し、Manifestの未保存データとVisualプレビューを即時更新する。変更、Save、再Openによる復元をGUI確認済み。
- [x] DONE: 同じglTFの複数配置、配置の複製・削除、一意なIDの発行を実装。自動テストに加え、Duplicate、2配置表示、Remove SelectedをGUI確認済み。
- [x] DONE: 編集内容をManifestへ保存し、再読み込み時に復元。
- [x] DONE: 未保存のモデル編集後、Open／Back to Menu／ESCで保存・破棄・キャンセルを選ぶ処理を実装。OpenからDiscard後にフォルダ選択をキャンセルすると最終保存状態へ戻ること、ESCからCancelすると編集状態を維持すること、BackからDiscardすると保存状態へ戻ることをGUI確認。保存失敗時に遷移せずデータを保持することも自動テスト済み。
- [x] DONE: アプリの閉じるボタン／Alt+F4にも未保存保護を接続。保存済みなら即時終了し、未保存なら保存／破棄／キャンセルの結果を待つ。Cancelで編集画面に残ること、Save and Continueで保存後に終了すること、Discardで保存せず終了して再起動後に最終保存状態へ戻ることをGUI確認。
- [x] DONE: 変換・複数配置・保存復元をテストし、GUIで確認。

完了条件: 複数の配置を個別に編集でき、保存・再読み込み後も位置と向きが一致する。

補足: 3D画面での直接選択やドラッグ操作の範囲は未確定。まず数値編集を対象にする。

## 4. HitMeshの物理判定 — DONE

- [x] DONE: Joltの静的Mesh形状を採用し、対応するHitMeshの形式・制約をversion 1として仕様書へ記載。
- [x] DONE: Renderingに依存しないTankMapCoreで、glTF/GLBのHitノードから物理用の頂点・三角形データを抽出。外部BIN、base64、GLB、byte stride、8/16/32bit indexに対応し、階層変換・左右座標変換・頂点順反転を適用。
- [x] DONE: 抽出した頂点・三角形からJoltの静的Mesh Shapeを生成。非有限頂点、範囲外index、重複index、同一直線上の退化三角形を理由付きで拒否し、失敗時は既存Shapeを保持する。Manifestの位置・XYZ回転をVisualと同じ行列で頂点へ適用し、PhysicsWorldへ非移動Bodyとして登録する。
- [x] DONE: 走行画面のMap欄にHitMeshのマゼンタ表示切り替えを追加。物理へ渡した変換済み三角形を直接描画し、Visualとの重なりとON/OFFをGUI確認。
- [x] DONE: glTF親子変換とManifestの位置・回転を数値テストし、配置済みHitMesh上へ動的Boxを落として接触・静止を確認。Visualとの重なりもGUI確認。
- [x] DONE: `MapEditorPreview`へ20m四方のVisual/Hit Groundを追加し、既定床なしで戦車がHitMesh上に静止・前進する統合テストに成功。トップ画面からManifest Mapを選択し、戦車の静止・W/A/D走行と接触をGUI確認済み。

完了条件: Visualは表示、Hitは衝突に使用され、位置と回転が一致することを確認できる。

## 5. 開始地点とクリアエリアの編集 — DONE

- [x] DONE: 主人公戦車の開始位置XYZ・回転XYZの数値編集を実装し、編集した位置・yawでの開始をGUI確認。
- [x] DONE: 開始地点の位置と+Z向きが分かるオレンジ色マーカーを表示。変換の自動テストとGUI主観評価に成功。
- [x] DONE: クリア判定AABBの追加・選択・名前・中心XYZ・サイズXYZの編集・削除を実装。ID発行・追加・削除の自動テストと、編集したGoalでのクリアをGUI確認。
- [x] DONE: AABBをシアン色の12辺ワイヤーボックスで可視化。World軸に沿わせ、回転は持たせない。インスタンス生成テストとGUI主観評価に成功。
- [x] DONE: 開始地点とAABBは既存Manifest入出力・サイズ検証を使用。編集、Save、再Openによる復元をGUI確認。

完了条件: 開始地点と複数のAABBを編集・表示・保存・復元できる。クリア動作そのものは段階6で接続する。

## 6. マップを選択してプレイ — DONE

- [x] DONE: エディタから戻ったとき、編集元MapフォルダのManifestを再読込してトップ画面へ登録・選択する。任意フォルダを `Config/map_editor_folders.json` へ保存し、再起動時に同名のビルド出力内コピーより優先して復元。自動テストとGUI再起動確認に成功。
- [x] DONE: `Config/Maps`直下のMapフォルダを検索し、トップ画面へManifest Mapの選択肢と走行開始を追加。`MapEditorPreview (Manifest)`の選択と開始をGUI確認済み。
- [x] DONE: ManifestからVisual Mesh・HitMesh・戦車の開始位置とyawを読み込み、既定床を使わずプレイを開始する処理を実装。自動物理テストとGUI走行確認に成功。
- [x] DONE: クリア判定version 1を「戦車の車体中心がいずれかのAABB境界内へ入る」に決定。最初に入ったエリア名を保持し、物理シミュレーションは継続する。
- [x] DONE: AABBへの進入判定を走行へ接続し、Map欄へ`MAP CLEAR`とエリア名を表示。中心・境界・範囲外・複数エリアの自動テストとGUI走行確認に成功。
- [x] DONE: Goal達成時、達成AABBを明るい緑へ変更し、GUIに依存しない緑色ビーコンを3Dシーンへ表示。自動テストとGUI主観評価に成功。
- [x] DONE: 走行画面にもオレンジの開始地点とシアンのクリアAABBを表示し、`Show Start / Goal Markers`で一括切り替え可能にする。共通マーカー生成テストと、視認性・ON/OFFのGUI主観評価に成功。
- [x] DONE: 存在しない登録フォルダや不正なManifestは一覧から除外して拒否件数を表示し、モデル欠落時はマップ開始を中止してエラーを表示する。
- [x] DONE: 「エディタで作成 → 保存 → トップ画面 → 選択 → プレイ」の一連の動作をGUI確認。
- [x] DONE: 開始姿勢・衝突・クリア境界の関連テスト、Debugビルド、GUI確認。

完了条件: 作成したマップをトップ画面から選んで走行でき、保存した開始地点と合意したクリア条件が機能する。

## 未確定事項の追跡

| 項目 | 確定が必要な段階 | 現在の状態 |
| --- | --- | --- |
| 保存タイミング・未保存変更の扱い | 1 / 3 | Save方式と各遷移・アプリ終了の確認処理を実装。GUI結合検証は段階3 |
| glTFの役割識別規約 | 2 | version 1確定。完全一致のVisual / Hit親ノード方式 |
| 座標系・回転順・変換規約 | 2 | version 1確定。左手World座標、glTFのZ反転、roll Z → pitch X → yaw Y |
| サブフォルダ検索範囲 | 2 | Mapフォルダ配下を再帰検索。リンク・ジャンクション等は除外 |
| HitMeshの対応形状と制約 | 4 | 静的Meshを基本案とする |
| マップの登録・一覧化・記憶 | 6 | version 1確定。開いたフォルダの絶対パスを保存し、再起動時に復元 |
| クリア判定とクリア時の動作 | 6 | version 1確定。車体中心がいずれかのAABB境界内へ入ると表示をラッチし、走行は継続 |

## 更新履歴

- 2026-09-07: 会話中の6ステップを進捗管理用に整理。実装・検証済みのデータ層を段階0としてDONEにし、今後の具体化と完了更新の運用を追加。
- 2026-09-07: 段階1のフォルダ操作・ファイル入出力・画面遷移を実装し、関連テスト4件とGUI操作を確認してDONEに更新。編集機能がまだないため、未保存確認のGUI結合検証とアプリ終了時の保護を段階3に明記。

- 2026-09-07: 段階2をモデル一覧（2-A）と役割識別・3D読み込み（2-B）に分割。2-Aを実装し、ビルド・関連テスト5件・GUI確認後にDONEへ更新。

- 2026-09-07: 描画・物理に依存しないglTF/GLB役割検査とInspect Rolesを追加。仮規約を仕様書に明記し、正常・不正階層のテストとGUIを検証。2-B全体は3D読み込みが残るためIN_PROGRESSを維持。

- 2026-09-07: 検査済みモデルをManifestへ追加し、Visual Meshだけをレンダラーへロードするプレビューを追加。ノード番号でのロードにより無名・重複名を扱えるようにした。DebugビルドとTankMapのCTestを確認。位置・回転編集、読み込み失敗時にManifestを変更しない処理、GUI表示確認は残るため2-BをIN_PROGRESSに維持。

- 2026-09-08: GUI確認で表示インスタンス数、テストglTFのバッファと面方向を修正。Triangle.gltfのInspect Roles、Add、初期姿勢表示、配置回転、軸色、Saveを確認。Map Editorウィンドウの固定も解除した。

- 2026-09-08: 配置のDuplicate、Remove Selected、一意ID発行をデータ層へ分離してUIへ接続。IDがクリアエリアと衝突しないこと、複製時のAsset・Transform保持、対象外削除の非破壊性を自動テストで確認。

- 2026-09-08: Duplicate、位置変更、2配置表示、Save、再Openでの復元、Remove Selectedの一連のGUI確認が完了。段階3は未保存変更の遷移・終了保護のみ継続する。
- 2026-09-08: 未保存変更をOpen／Back／ESCに加え、ウィンドウの閉じるボタンとAlt+F4でも保護する終了要求コールバックを追加。Map Editorの初期位置指定を初回だけにして、画面上で移動できるよう修正。DebugビルドとMap関連CTest 3件を確認し、GUI確認待ちとして段階3を継続する。
- 2026-09-08: 未保存状態でAlt+F4を押し、確認ダイアログからCancelするとMap Editorへ戻ることをGUI確認。
- 2026-09-08: 未保存状態でAlt+F4を押し、Save and Continueで終了した後、再起動・再Openで編集内容が復元されることをGUI確認。
- 2026-09-08: glTFの役割・座標・変換規約をversion 1として確定。Tank側からRtPbrSurveyのノードMesh回帰テストを生成し、重複名、祖先変換、quaternion回転、右手系から左手系への変換、既存API互換性を確認して段階2をDONEに更新。
- 2026-09-09: 段階4を開始。HitMesh形状規約version 1とRendering非依存のglTF/GLB三角形抽出を追加。外部BIN fixtureでHitだけの抽出、親子の平行移動・回転、左右座標変換、頂点順、失敗時の出力保持をテストし、TankSandboxのDebugビルドを確認。
- 2026-09-09: Manifest配置をVisualと同じroll-pitch-yaw行列でHitMeshへ適用し、Joltの静的BodyとしてPhysicsWorldへ登録。位置・90度yawの数値検証と、配置した床HitMesh上で動的Boxが静止する統合テストを追加。関連テスト2件とTankSandboxのDebugビルドに成功。
- 2026-09-09: `MapEditorPreview`へVisual/Hitを持つ20m四方のGroundを追加。トップ画面でManifest Mapを列挙し、Visual・Hit・開始位置を戦車走行モードへ渡す経路を実装。既定床なしで戦車がglTF HitMesh上に静止・前進する統合テスト、既存物理テストを含む関連6件、Groundのレンダラーロード、TankSandboxのDebugビルドに成功。GUIでの物理走行確認待ち。
- 2026-09-09: HitMesh三角形からJolt Mesh Shapeを生成するPhysics層を追加。正常Shapeと無効な頂点・index・退化三角形の拒否をテストし、関連CTest 2件とTankSandboxのDebugビルドを確認。
- 2026-09-09: トップ画面から`MapEditorPreview (Manifest)`を開始し、glTF HitMesh上での静止とW/A/D走行をGUI確認。段階4の物理走行と段階6のManifest Map選択・開始をDONEへ更新し、次の確認項目として走行画面のHitMesh表示切り替えに着手。
- 2026-09-09: 物理へ登録した変換済みHitMeshからマゼンタの確認用描画Meshを生成し、走行画面のMap欄から表示を切り替える機能を追加。無効indexでシーンを変更しないことを含む描画データ生成テスト、関連CTest 5件、TankSandboxのDebugビルドに成功。GUI重ね合わせ確認待ち。
- 2026-09-09: 段階5を開始。開始地点の位置・回転、クリアAABBの追加・選択・名前・中心・サイズ・削除を数値編集UIへ追加し、オレンジの開始方向マーカーとシアンのAABBワイヤーをプレビューへ追加。ID操作とマーカー生成を自動テストし、TankSandboxのDebugビルドに成功。
- 2026-09-09: クリア条件version 1を車体中心のAABB進入として確定し、境界を含む判定、最初のエリア名の保持、走行画面への`MAP CLEAR`表示を実装。`MapEditorPreview`の前方へ検証用`Forward Goal`を追加。クリア判定を含む関連CTest 7件とTankSandboxのDebugビルドに成功し、GUI編集・表示・走行確認待ち。
- 2026-09-10: GUI確認により、エディタが更新したソースMapフォルダと走行側が列挙したビルド出力内コピーが一致していないことを特定。エディタ終了時に編集元Manifestを再読込し、同名の古い登録を置換して選択するよう修正。トップ画面には選択中の完全なMapフォルダを表示する。
- 2026-09-10: 走行画面へ開始地点とクリアAABBの確認表示を追加し、Map欄の`Show Start / Goal Markers`で切り替え可能にした。Manifest物理統合テストに原点以外の開始位置検証を追加し、ユーザー編集でモデル数が変わっても有効なマップなら検証できるよう修正。TankSandboxのDebugビルドと関連CTest 7件に成功し、GUI再確認待ち。
- 2026-09-10: Map Editorではカメラスロット設定UIを表示しない一方、マウス入力判定が通常のカメラスロット設定を要求していたためカメラが動かなかった。エディタ進入時にArcballカメラを接続し、エディタ中はUI外のマウス入力を直接許可するよう修正。画面へ左ドラッグ・中ドラッグ・ホイールの操作説明も追加。
- 2026-09-10: カメラ入力修正後のTankSandbox Debugビルド、`TankApp.CameraControl`、`TankRendering.MapVisualLoader`に成功。Map Editorで左ドラッグの回転、中ドラッグの移動、ホイールのズームをGUI確認。
- 2026-09-10: Map Editorへ移動・再表示可能なCheat Sheetウィンドウを追加。カメラ操作、モデル追加、配置・開始地点・クリア領域編集、保存、プレビューマーカー、画面遷移を画面内で確認できるようにした。TankSandboxのDebugビルドとGUI表示を確認。
- 2026-09-10: `playerSpawn.position`は車体中心座標であり、MapEditorPreviewのY=-0.5では床HitMesh内から生成されて落下することを確認。X/Zとyawを維持してY=2.0へ修正し、Player Start編集欄とCheat SheetへYをHitMeshより上に置く説明を追加。DebugビルドとHitMesh・静的Mesh・Manifest戦車物理のCTest 3件に成功し、GUI再確認待ち。
- 2026-09-10: MapEditorPreviewでGoal進入時にMap欄へ`MAP CLEAR: Clear Area`が表示されることをGUI確認。
- 2026-09-10: Map Editorで開いたフォルダの絶対パスを `Config/map_editor_folders.json` へ保存し、再起動時に再登録するversion 1ストアを追加。同名のビルド出力内マップより登録フォルダを優先し、不正・消失フォルダは拒否件数へ反映する。ストアの往復・欠落・破損時の非破壊テスト、TankSandboxのDebugビルド、全CTest 50件に成功。GUI再起動確認待ち。
- 2026-09-10: Goal達成をGUI外でも識別できるよう、達成したAABBをシアンから明るい緑へ変更し、AABB中心から上へ12mの緑色ビーコンを表示する3D演出を追加。複数Goalでは達成したエリアだけを変更し、マーカー表示設定にかかわらずビーコンを表示する。TankSandboxのDebugビルド、関連CTest 4件、GUI主観評価に成功。
- 2026-09-10: HitMeshのマゼンタ表示とVisual床の重なり、オレンジの開始方向・シアンのGoalの視認性、各表示のON/OFFをGUI確認。段階4をDONEに更新し、ランタイムマーカーをDONEに更新。
- 2026-09-10: 編集・保存した開始位置とyawでの走行開始、編集したAABBでのクリア、Start/Goalマーカーの視認性を確認済みのため、段階5をDONEに更新。
- 2026-09-11: Map Editorで登録した編集元フォルダが再起動後もトップ画面へ復元され、完全パスが一致することをGUI確認。作成・保存・選択・プレイ、開始姿勢、HitMesh衝突、Goal判定の一連の確認が揃ったため段階6をDONEに更新。
- 2026-09-11: 未保存確認の`Discard`後にOpen先のフォルダ選択をキャンセルすると、破棄したはずの編集内容がメモリに残る問題を修正。MapFolderへ最終保存状態の復元処理を追加し、Open／Back／ESC／Alt+F4のどの遷移でもDiscard選択時に即時復元する。TankSandboxのDebugビルドとMap編集関連CTest 3件に成功し、GUI確認待ち。
- 2026-09-11: 未保存のClear Area名を変更し、`Open → Discard → フォルダ選択キャンセル`後に元の名前とSaved状態へ復元されることをGUI確認。
- 2026-09-11: 未保存のClear Area名に対し、`ESC → Cancel`で編集状態が維持され、続く`Back to Menu → Discard`でトップへ戻り、再Open時に最終保存状態が復元されることをGUI確認。
- 2026-09-11: 未保存のClear Area名に対し、`Alt+F4 → Discard`で終了し、再起動・再Open後に最終保存状態が復元されることをGUI確認。段階3をDONEに更新し、全段階の完了条件を達成。最終Debugビルド後の全CTest 50件に成功。
