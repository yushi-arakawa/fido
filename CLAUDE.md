# fido — Tamagotchi-style pet game for M5Stack Basic

## ハードウェア

- **ボード**: M5Stack Basic (ESP32 240MHz, 320×240 TFT LCD, ボタン A/B/C)
- **フラッシュ**: 4MB / パーティション: `huge_app` (アプリ領域 3MB, 使用中 ~57.1%)
- **RAM**: 320KB (使用中 ~31.1%)
- **プラットフォーム**: PlatformIO / Arduino framework, M5Stack @ 0.4.6
- **ビルド**: `pio run -t upload`

---

## 画面構成（C ボタンで循環）

```
Main → Act → Back → Main ...
```

| 画面 | 内容 | A | B | C |
|------|------|---|---|---|
| **Main** | 宇宙背景 + キャラクター(128×128) + メッセージBOX | Gacha (NASA API) | Talk (Egg期/夜は無効) | → Act |
| **Act** | 左: アクションリスト / 右: Fido生態tips | Move | OK | → Back |
| **Back** | ステータス全表示（ステータス + カード統合） | Config | --- | → Main |
| **Config** | 音量(ON/OFF) / 電源OFF / データ削除 | Move | Select | Close |

---

## ファイル構成

### include/

| ファイル | 役割 |
|----------|------|
| `pet.h` | `Pet` 構造体, `PetMood` enum |
| `game_data.h` | `Inventory`, `ItemDef`, `ITEM_COUNT=14`, save/load宣言 |
| `display.h` | `UIMode` enum (Main/Act/Back), 表示API |
| `char_anim.h` | キャラクターアニメーションAPI |
| `space_ui.h` | モノクロパレット定数, `spDrawBackground/Starfield/Bar/Frame` インライン関数 |
| `sprite_stages.h` | 5ステージ分の64×64 RGB565スプライト配列 (フラッシュに格納) |
| `minigame.h` | `runGameMenu()` 宣言 |
| `shop.h` | `runShop()` 宣言 |
| `status_report.h` | `showSettings()`, `showConfirmDialog()`, `showMemorial()` 宣言 |
| `claude_client.h` | `askClaude()` 宣言 |
| `remote.h` | PC遠隔操作 API (`remoteSync()`, `btnA/B/C()`) |
| `world.h` | 昼夜サイクル (`worldIsNight()`) + ランダム宇宙イベント (`worldRollEvent()`) |
| `nasa_gacha.h` | `NasaCargo` 構造体 (`NASA_CARGO_MAX=8`), `runNasaGacha/saveNasaCargo/loadNasaCargo` 宣言 |
| `secrets.h` | WiFi認証情報 (`WIFI_SSID`, `WIFI_PASS`) — **gitignore済み、コミット不可** |
| `secrets.h.example` | `secrets.h` のテンプレート (コミット可) |

### src/

| ファイル | 役割 |
|----------|------|
| `main.cpp` | ゲームループ, ボタン処理, tick管理 |
| `display.cpp` | `displayInit/ActContent/BackContent/Message/MenuBar` |
| `char_anim.cpp` | ボブアニメーション, 64×64→128×128 2xスケール, 宇宙背景復元 |
| `pet.cpp` | `tick()` (30秒毎), `calcMood()` |
| `game_data.cpp` | アイテム定義(14個), NVS save/load |
| `minigame.cpp` | ミニゲーム11種 + 選択メニュー |
| `shop.cpp` | スクロール可能ショップ(14アイテム) |
| `status_report.cpp` | Config画面, 確認ダイアログ, 追悼画面 |
| `claude_client.cpp` | Wi-Fi + Anthropic API HTTP通信 |
| `remote.cpp` | USBシリアル遠隔操作 (PC→M5Stack ボタン入力) |
| `world.cpp` | 昼夜サイクル (millis 加速) + ランダム宇宙イベント抽選 |
| `nasa_gacha.cpp` | WiFi接続 → NASA APOD API (HTTPS) → タイトル抽出 → NVS保存 |

---

## ペットのデータ構造

```cpp
struct Pet {
    String  name;       // "Fido"
    uint8_t hunger;     // 0-100 (100=満腹)
    uint8_t happiness;  // 0-100
    uint8_t health;     // 0-100
    uint8_t age;        // tick毎に+1 (1tick=30秒)
    PetMood mood;       // Happy/Neutral/Hungry/Sleepy/Sick
};

struct Inventory {
    uint16_t coins;      // ミニゲーム報酬。転生でも継承
    uint16_t bond;       // 0-1000 (絆レベル)。転生でリセット
    bool     owned[14];  // NVSキー安定のため固定サイズ。転生でも継承
    uint8_t  critStreak; // health==0 の連続tick。CRIT_LIMIT(6)で離脱(死)
    uint16_t rebirths;   // 看取った世代数。転生で+1 (世代をまたいで残る)
    uint8_t  bestAge;    // 歴代最高の到達Day
    uint16_t bestBond;   // 歴代最高の絆
};
```

### tick(night) の動作 (30秒毎)
- `age++` (max 255)
- `hunger -= (night ? 1 : 2)`（夜は就寝中で半減・0クランプ）
- `happiness -= 1`（**昼のみ**。夜は減らさない）
- `hunger < 20` なら `health -= 1`
- 夜かつ非危機なら `mood = Sleepy` に上書き
- NVSに保存, Back画面なら即時再描画
- `night` は `worldIsNight()` (world.cpp) が決める昼夜サイクル

### 成長ステージ (ageで決定)
| age | ステージ | 目安 |
|-----|---------|------|
| 0-3 | Egg | 0-2分 |
| 4-39 | Child | 2-19分 |
| 40-59 | Teen | 20-29分 |
| 60-79 | Young | 30-39分 |
| 80+ | Elder | 40分~ |

---

## 昼夜サイクル (world.cpp)

RTC 非搭載のため**実時刻ではなく millis ベースの加速サイクル**。`worldIsNight()` が判定する。

- 周期: **昼 8分 + 夜 4分 = 12分で1日**（位相は NVS 非保存 = 電源OFFで昼から再開）
- 夜の挙動: `tick()` の hunger 減衰半減・happiness 据え置き・mood=Sleepy、キャラは就寝表現（寝息 + zZ）、Main 右上に三日月
- 夜の操作制限: **Talk** はそっと見るだけ（"Zzz... fast asleep"）、**Play** は寝起きで士気 −5
- `main.cpp::loop()` で昼夜切替を検出したら1度だけ `fullRedraw()`（Back画面ではメッセージ抑制）

## ランダム宇宙イベント (world.cpp)

`worldRollEvent()` を tick 毎に呼ぶ。**15%** で発生（卵期は対象外）。発生時は B5 988Hz のチャイム + メッセージBOX通知。

| イベント | 効果 | 重み |
|----------|------|------|
| Meteor shower | coins +5〜15 | 30 |
| Supply drone | hunger +25 | 25 |
| Cosmic ray surge | happiness +15 | 20 |
| Solar flare | health −5 (health>60) / −10 | 15 |
| Wormhole | age +3（進化が早まる） | 10 |

## 生死＆転生 (main.cpp::handleDeath / status_report.cpp::showMemorial)

- `health == 0` が **`CRIT_LIMIT`=6 tick (3分)** 連続すると「離脱」（死）。連続数は `inv.critStreak` に**NVS永続**（電源OFFで放置が帳消しにならない）
- 離脱時: 記録更新（`bestAge`/`bestBond`/`rebirths`）→ 追悼画面 `showMemorial()`（ボタン待ち）→ 転生
- **転生の引き継ぎ**: ステータス・絆は初期化、**コイン (`coins`) と所有アイテム (`owned[]`) は継承**、記録3つは世代をまたいで残る
- 転生直後は `charAnimReset()` で進化演出なしに Egg を再ロード

---

## キャラクターアニメーション (char_anim.cpp)

- スプライト: 64×64 RGB565 (フラッシュ内、`sprite_stages.h`)
- 描画: 2xソフトウェアスケール → **128×128** で pushImage
- ボブ: `sin(millis() * 0.0015f) * 3.0f` px の上下移動
- 消去時: `spDrawBackgroundRect()` で宇宙背景と星を復元 (黒塗りしない)
- Main画面のみ呼び出し: `charAnimUpdate(pet.age, 160, 80, night)`
  - `night=true` で就寝表現 (穏やかな寝息 + 頭上に "zZ")。zはボックス内に描くので次回 erase で消える
- `charAnimReset()`: 転生時に呼ぶ完全リセット。Elder→Egg の逆進化演出を抑止 (`charAnimRedraw()` は currentStage を保持するので別物)

### スプライト再生成
```
py convert_sprites.py
```
`src/char/*.jpg` (64×64) → `include/sprite_stages.h`

---

## UI設計原則

### ノイズ回避
- `spDrawStarfield()` は**モード切替時の一度だけ**呼ぶ
- ナビゲーション時は `displayActContent()` / `displayBackContent()` のみ（背景に触れない）
- Main画面の左パネルは `fillRect(SM_BG)` で描画（星なし）

### モノクロパレット (space_ui.h)
| 定数 | 値 | 用途 |
|------|----|------|
| `SM_BG` | `0x0000` | 背景(黒) |
| `SM_HDR` | `0x0841` | ヘッダー帯 |
| `SM_SEL` | `0x2945` | 選択行ハイライト |
| `SM_DIV` | `0x2104` | 区切り線 |
| `SM_DIM` | `0x4208` | 薄いテキスト |
| `SM_GREY` | `0x7BEF` | 本文テキスト |
| `SM_LIGHT` | `0xBDF7` | 通常テキスト |
| `SM_WHITE` | `0xFFFF` | タイトル・強調 |

---

## ミニゲーム (11種)

Reaction / Button Mash / Simon Says / Rhythm Tap / Lucky Spin /
Whack-a-Mole / Precision Stop / Quick Math / Hot & Cold / Survivor / Lucky Roll

---

## アイテム (14種)

Apple / Ball / Bandage / Crown / Cookie / Candy / Teddy /
Vitamin / Potion / Steak / Toy Car / P.Hat / Elixir / Star

---

## サウンド (M5.Speaker)

### ボタン効果音 (main.cpp / status_report.cpp)

音域は起動音 (char_anim.cpp の C5 E5 G5 C6 アルペジオ) に合わせた高域で統一。
低い C3 系だと小型スピーカーでにごり、起動音より弱く聞こえるため2オクターブ上げた。

| ボタン | 周波数 | 長さ | 用途 |
|--------|--------|------|------|
| A | C5 523 Hz | 60 ms | Move / Config |
| B | E5 659 Hz | 60 ms | OK / Talk / 確認 |
| C | G5 784 Hz | 60 ms | 画面切り替え / Close |
| 音量ON時 (B on vol) | A5 880 Hz | 150 ms | ON時のプレビュー |

### 音量設定

Config の音量設定は **ON / OFF の二択**。NVS キー `"vol"` に 0 or 1 を保存。

```cpp
M5.Speaker.setVolume(vol ? 1 : 0);
// ON=1, OFF=0, デフォルト ON (1)
```

`main.cpp` (起動時) と `status_report.cpp` (saveVolume) の両方で同じロジックを使う。
データ削除・再起動前は `M5.Speaker.end()` でスピーカーを停止してからリセットする。

---

## NASAガチャ (nasa_gacha.cpp)

Main画面の [A] ボタンで起動。WiFi接続後、NASA APOD API からランダムな日付の天文写真タイトルを取得してカーゴに追加する。

```cpp
struct NasaCargo {
    char    items[NASA_CARGO_MAX][24]; // APODタイトル (最大23文字 + null)
    uint8_t count;                     // 現在の件数 (max 8)
};
```

- **API**: `https://api.nasa.gov/planetary/apod?api_key=DEMO_KEY&date=YYYY-MM-DD`
- **日付範囲**: 2000〜2025年のランダム日付 (`esp_random()` でシード)
- **タイトル抽出**: `"title":` をキーにした軽量文字列検索 (JSON パーサー不使用)
- **表示**: Back画面最下部「Space:」セクション (最大8件、折り返し表示)
- **認証情報**: `include/secrets.h` に `WIFI_SSID` / `WIFI_PASS` を定義 (gitignore済み)

### セットアップ
```
cp include/secrets.h.example include/secrets.h
# secrets.h 内の WIFI_SSID / WIFI_PASS を書き換える
```

---

## PC遠隔操作 (remote.cpp / fido_remote.html)

USB 接続中、PC から USB シリアル (115200 baud) に 1 文字を送ると物理ボタン
A/B/C を 1 回押したのと同じイベントが発火する。

| 受信文字 | 動作 |
|----------|------|
| `a` / `A` | ボタン A |
| `b` / `B` | ボタン B |
| `c` / `C` | ボタン C |
| その他 (改行・空白等) | 無視 (デバッグコマンドは下記) |

### デバッグコマンド (値の直接書き換え)

`<フィールド文字><数値><改行>` 形式の行を送ると、値を直接設定できる。
a/b/c は 1 文字で即時処理、デバッグコマンドは**改行で確定**する。

| コマンド | 対象 | 範囲 | 例 |
|----------|------|------|-----|
| `h<n>` | hunger | 0-100 | `h50` |
| `p<n>` | happiness | 0-100 | `p50` |
| `s<n>` | health | 0-100 | `s100` |
| `g<n>` | age | 0-255 | `g80` |
| `m<n>` | coins (money) | 0-9999 | `m999` |
| `k<n>` | bond (絆/kizuna) | 0-1000 | `k800` |

`remoteSync()` が行バッファに溜めて解析し、`remoteDebugApply(pet, inv)` が
`main.cpp::loop()` 内で反映 → `saveAll()` → `fullRedraw()`。範囲外は自動クランプ、
適用時に `[DEBUG] x set to n` をシリアル出力する。

### アクションコマンド (新機能の即検証)

`<語句><改行>` を送ると、待ち時間なしで新機能を検証できる。`remoteSync()` が
解析して保留し、`remotePopAction()` が `main.cpp::loop()` で取り出して分岐する。

| コマンド | 動作 |
|----------|------|
| `night` | 昼夜を**夜に固定** (worldSetOverride ForceNight) |
| `sun` | 昼夜を**昼に固定** (ForceDay) |
| `free` | **自動サイクルに復帰** (Auto) |
| `event` | ランダム**宇宙イベントを即発生** (確率/卵ゲート無視) |
| `die` | 即「**離脱**(死)」→ 追悼 → 転生 |

⚠ `remoteSync()` は受信文字に **a/b/c が含まれると位置に関係なく**ボタン押下扱いに
するため、アクション語は a/b/c を一切含まない単語にしてある (例: 昼=`day` ではなく
`sun`、auto ではなく `free`)。新しいアクション語を足す時もこの制約を守ること。

### ファームウェア側の仕組み

- `remoteSync()` を **全ての入力ループで `M5.update()` の直後に呼ぶ**。
  呼ぶたびに前イテレーションの押下フラグを破棄 → シリアルを読み直すため、
  古い入力が残らない (フラグ寿命 = 1 イテレーション)。
- `M5.BtnX.wasPressed()` は **`btnX()` に置き換え済み** (= 物理 or 遠隔)。
  main.cpp / minigame.cpp / shop.cpp / status_report.cpp の全画面が対応。
- 新しくボタンを読むループを足す場合も同じ規約 (`remoteSync()` + `btnX()`) に従う。

### PC側の操作盤

- `fido_remote.html` — Web Serial API を使う単一 HTML ファイル。
  Chrome / Edge で開き [接続] → COM ポート選択。A/B/C をクリック、または
  キーボードの A/B/C キーで操作。`[MODE] -> Xxx` ログを拾ってボタンの
  ヒント表示を現在の画面に追従させる。DEBUG セクションで値設定 (hunger/happy/
  health/age/coins/bond)、ACTION セクションで Night/Day/Auto/Event/Die を送れる。
- シリアルモニタ (`pio device monitor`) で `a`/`b`/`c` や `h50`/`m999`/`night`/
  `event`/`die` 等を直接打っても可。

---

## 永続化 (ESP32 Preferences NVS)

namespace: `"fido"` キー: `hunger`, `happy`, `health`, `age`, `coins`, `bond`, `it0`-`it13`, `vol`, `nc`, `nc0`-`nc7`, `crit`, `reb`, `bAge`, `bBond`

---

## 既知の制約

- `huge_app` パーティション使用によりアプリ領域は 3MB。OTA パーティションなし
- NASA APOD `DEMO_KEY` のレート制限: 30 req/hour, 50 req/day
- スプライトを増やす場合は SD カード経由が必要 (`convert_sprites.py` を `SIZE=128` に変更)
