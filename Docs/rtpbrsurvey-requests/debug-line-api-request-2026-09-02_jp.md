# RtPbrSurvey DrawLine API リクエスト

## 目的

TankPhysicsSandbox の迫撃攻撃範囲サークル、およびローリング妨害表示を軽量に描画するため、RtPbrSurvey に一時的なデバッグライン描画 API を追加する。

## 最小要件

```cpp
using DebugLineHandle = uint32_t;

DebugLineHandle AddDebugLine();
void UpdateDebugLine(
    DebugLineHandle handle,
    Vec3 start,
    Vec3 end,
    Color color,
    bool visible);
void RemoveDebugLine(DebugLineHandle handle);
```

## 挙動要件

- ワールド座標で描画する。
- 毎フレーム、端点・色・表示状態を更新できる。
- `visible == false` では描画しない。
- 無効なハンドルは安全に無視する。
- デバッグ用途のため、深度テストの有無を設定可能にする。
- 既存の通常メッシュ描画APIには影響を与えない。

## 想定利用

迫撃範囲は32本のラインで円周を構成し、射程変更時には各ラインの端点だけ更新する。発射可能時と待機中で色を変更する。

## 受け入れ条件

- API追加後に既存サンプルがビルドできる。
- ラインの追加・更新・非表示・削除が確認できる。
- TankPhysicsSandbox からリンク可能な公開ヘッダ／ライブラリとして提供される。
