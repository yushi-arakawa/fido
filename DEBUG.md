# fido デバッグマニュアル

M5Stack Basic 用ペットゲーム `fido` の開発・動作確認用マニュアル。
ハードウェアやファイル構成の全体像は [CLAUDE.md](CLAUDE.md) を参照。

---

## 1. ビルド & 書き込み

PlatformIO CLI (`pio`) を使う。`pio` が PATH に無い場合は venv 内の実体を直接叩く。

| 操作 | コマンド |
|------|----------|
| ビルドのみ | `pio run` |
| ビルド + 書き込み | `pio run -t upload` |
| シリアルモニタ | `pio device monitor` (115200 baud) |
| ビルド + 書き込み + モニタ | `pio run -t upload -t monitor` |
| クリーンビルド | `pio run -t clean` |

```powershell
# pio が PATH に無いとき (Windows)
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
```

書き込み後のサイズチェック出力で `Flash` / `RAM` 使用率を確認する
(目安: Flash ~57%, RAM ~29%)。

---

## 2. シリアルログの読み方

ファームウェアは要所で `Serial.printf()` を出力する。`pio device monitor` で観察する。

### `[MODE] -> Xxx`
C ボタン (または遠隔 `c`) で画面が切り替わるたびに出力。

```
[MODE] -> Act
```

### `[TICK] ...`
30 秒ごとの `pet.tick()` 実行時に出力。**ログ上の名前と内部変数名が異なる**点に注意。

```
[TICK] age:12 energy:64 morale:71 shield:100 bond:9
```

| ログ表記 | 実際のフィールド | 意味 |
|----------|-----------------|------|
| `age`    | `pet.age`       | 年齢 (tick毎に +1, max 255) |
| `energy` | `pet.hunger`    | 空腹度 0-100 (100=満腹) |
| `morale` | `pet.happiness` | 幸福度 0-100 |
| `shield` | `pet.health`    | 健康度 0-100 |
| `bond`   | `inv.bond`      | 絆 0-1000 |

tick の挙動: `age++`, `hunger -= 2`, `happiness -= 1`、`hunger < 20` なら `health -= 1`。

---

## 3. PC からの遠隔操作デバッグ (fido_remote.html)

実機のボタンを押さずに PC から A/B/C を操作できる。連続テストや録画に便利。

1. M5Stack を USB 接続したまま `fido_remote.html` を Chrome / Edge で開く
2. **[接続]** → ポップアップで M5Stack の COM ポートを選択
3. 画面の A/B/C ボタンをクリック、またはキーボードの **A / B / C** キーで操作
4. 下部のシリアルログで `[MODE]` / `[TICK]` を確認

シリアルモニタから直接 `a` / `b` / `c` を打っても同じ動作になる
(詳細は [CLAUDE.md](CLAUDE.md) の「PC遠隔操作」セクション)。

> ⚠ `fido_remote.html` と `pio device monitor` は **同じ COM ポートを同時に開けない**。
> 片方を閉じてからもう片方を使うこと。

---

## 4. 画面別 動作確認チェックリスト

| 画面 | 確認項目 |
|------|----------|
| **Main** | キャラのボブアニメ / 背景の星がちらつかない / A=NASAガチャ / B=Talk (Egg期は無効) |
| **Act**  | A でアクション選択がループ / B で Feed・Play・Game・Shop が起動 |
| **Back** | ステータス数値が tick で即時更新 / Space カーゴ表示 / A=Config |
| **Config** | 音量 ON/OFF 切替 / 電源OFF / データ削除 (確認ダイアログ) |
| **遷移** | C で Main→Act→Back→Main と循環、戻り後に画面が崩れない |

---

## 5. 成長ステージを素早く確認する

`age` は 1 tick (30秒) で +1。Elder (age≥80) まで通常 **40分**かかる。
デバッグ中にステージ遷移を確認したいときは一時的に次のいずれかを行う:

| ステージ | age | 目安 |
|----------|-----|------|
| Egg | 0-3 | 0-2分 |
| Child | 4-39 | 2-19分 |
| Teen | 40-59 | 20-29分 |
| Young | 60-79 | 30-39分 |
| Elder | 80+ | 40分~ |

- **方法A**: `src/main.cpp` の `TICK_MS` (既定 30000) を一時的に小さくする (例 1000)。
- **方法B**: `setup()` の `pet` 初期化で `age` を直接書き換える
  (例: `{"Fido", 80, 80, 100, 60, ...}` で Young から開始)。
- ⚠ どちらも**デバッグ後は必ず元に戻す**。`age` は NVS に保存されるため、
  確認後は Config の「データ削除」で初期化しておくこと。

---

## 6. よくある不具合と対処

| 症状 | 原因 / 対処 |
|------|-------------|
| WiFi に繋がらない | `include/secrets.h` の `WIFI_SSID` / `WIFI_PASS` を確認。2.4GHz 帯のみ対応 |
| `secrets.h` が無くてビルド失敗 | `cp include/secrets.h.example include/secrets.h` してから編集 |
| NASA ガチャが `No signal...` | `DEMO_KEY` のレート制限 (30 req/h, 50 req/day)。時間を空ける |
| 音が鳴らない | Config で音量 ON か確認。NVS キー `vol` が 0 だと無音 |
| WiFi 中に音が鳴り続ける | ブロッキング前に `M5.Speaker.mute()` を呼んでいるか確認 (main.cpp 参照) |
| 背景の星がちらつく | `spDrawStarfield()` をモード切替時以外で呼んでいないか確認 |
| 画面が崩れる (サブ画面から戻った後) | 戻り後に `fullRedraw()` を呼んでいるか確認 |
| 遠隔操作が効かない | COM ポートが他アプリに占有されていないか / 115200 baud か確認 |
| 書き込みが始まらない | ケーブルがデータ転送対応か / `upload_speed` を下げる (921600→115200) |

---

## 7. NVS (セーブデータ) の扱い

namespace `"fido"`。キー: `hunger`, `happy`, `health`, `age`, `coins`, `bond`,
`it0`-`it13`, `vol`, `nc`, `nc0`-`nc7`。

- **リセット**: Config 画面 → 「データ削除」→ 確認ダイアログで実行。
  ステータス・所持品・カーゴ・絆がすべて初期化される。
- データ削除・再起動の前は `M5.Speaker.end()` でスピーカーを止めてからリセットする。
- 書き込み (`pio run -t upload`) では NVS は消えない。完全初期化したいときは
  上記「データ削除」か、`esptool.py erase_flash` を使う。

---

## 8. メモリ / フラッシュ使用量の確認

```
pio run                          # ビルド末尾に RAM / Flash 使用率が出る
```

詳細な内訳は PlatformIO Home > Project Inspect、または:

```
pio run -t size
```

`huge_app` パーティション採用でアプリ領域は 3MB、OTA 領域なし。
スプライト追加などで Flash が逼迫する場合は [CLAUDE.md](CLAUDE.md) の
「既知の制約」を参照。
