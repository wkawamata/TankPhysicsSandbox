# RtPbrSurvey への依頼要約

## 依頼 001: CMake から使える SceneRenderer ターゲット

- Tank Physics Sandbox は CMake ベースなので、`RtPbrSurvey::SceneRenderer` を CMake host から使える形にしたい。
- 現状の RtPbrSurvey は Visual Studio/MSBuild アプリ project 形状が強く、Tank 側で reusable renderer layer だけをリンクしにくい。
- 希望は、RtPbrSurvey 側に CMake target または CMake host 用の明確な組み込み手順を用意すること。
- `SceneRenderer` などの公開名は backend-neutral のままにする。
- Tank 専用 API や Jolt 依存は RtPbrSurvey 側に入れない。
- Tank 側の最初の用途は、固定 cube/sphere scene と Tank 用 ImGui window を表示する renderer proof。
## Request 002: 外部hostで利用できるSceneRenderer Debug UI

- Tank Physics Sandboxの物理UIと並べて、RtPbrSurveyのrenderer診断UIを表示したい。
- 現在の `App::DrawDebugUi()` は `RtPbrSurveyApp` のprivate stateへ依存するため、Tankから直接呼び出せない。
- `SceneRenderer`だけで利用できる、backend-neutralなdebug UI layerの抽出を希望する。
- 最初の対象はframe情報、ray tracing対応、temporal upscaler状態、背景色、render view、shadow、reflection、tone mappingとする。
- scene選択、app camera、scene close、app固有のsave/resetはRtPbrSurvey app側に残してよい。
- Tank固有APIやphysics概念をRtPbrSurveyへ追加しない。
- upstream CMakeの `RtPbrSurvey::SceneRenderer` と一緒に外部hostから利用できる形を希望する。
