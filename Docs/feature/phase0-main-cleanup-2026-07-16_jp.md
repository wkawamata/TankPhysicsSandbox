# Phase 0 mainブランチ整理

日付: 2026-07-16

ブランチ: `main`

## 完了項目

- `feature/rtpbrsurvey-submodule` の作業が `main` へsquash mergeされたことを確認した。
- `.gitignore` に `.vscode/` と `vcpkg_installed/` を追加した。
- ignore設定の更新をコミットした。
  - `46772a9 Ignore local editor and vcpkg output`
- `main` を `origin/main` へpushした。
- ローカルの `HEAD` と `origin/main` が、どちらも `46772a9` を指していることを確認した。
- 通常の `git status` がクリーンであることを確認した。
- `External/RtPbrSurvey` submoduleの作業ツリーがクリーンであることを確認した。

## 残りのフォローアップ

次の整理対象は、Tank側でsubmoduleを直接変更せずにRtPbrSurveyを利用できるようにすることである。

推奨する次のタスク:

1. ホスト向けrenderer layerを利用できるstatic library targetを、RtPbrSurveyのupstream CMakeに追加するよう依頼する。
2. shader build出力、生成されたDirectX headerの公開、assets、runtime packagingの要件を、upstream targetまたは文書化された統合手順へ含める。
3. Tankの `External/RtPbrSurvey` を、そのupstream commitへ更新する。
4. submoduleがクリーンな状態でTankをビルドできることを確認する。

## 注記

`Docs/opencode` は引き続きOpenCodeの管理領域とする。Codexはここへ書き込まない。
