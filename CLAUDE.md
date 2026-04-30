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
| **Main** | 宇宙背景 + キャラクター(128×128) + メッセージBOX | Gacha (NASA API) | Talk (Claude API, Egg期は無効) | → Act |
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
| `status_report.h` | `showSettings()`, `showConfirmDialog()` 宣言 |
| `claude_client.h` | `askClaude()` 宣言 |
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
| `status_report.cpp` | Config画面, 確認ダイアログ |
| `claude_client.cpp` | Wi-Fi + Anthropic API HTTP通信 |
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
    uint16_t coins;
    uint16_t bond;       // 0-1000 (絆レベル)
    bool     owned[14];  // NVSキー安定のため固定サイズ
};
```

### tick() の動作 (30秒毎)
- `age++` (max 255)
- `hunger -= 2`, `happiness -= 1`
- `hunger < 20` なら `health -= 1`
- NVSに保存, Back画面なら即時再描画

### 成長ステージ (ageで決定)
| age | ステージ | 目安 |
|-----|---------|------|
| 0-3 | Egg | 0-2分 |
| 4-39 | Child | 2-19分 |
| 40-59 | Teen | 20-29分 |
| 60-79 | Young | 30-39分 |
| 80+ | Elder | 40分~ |

---

## キャラクターアニメーション (char_anim.cpp)

- スプライト: 64×64 RGB565 (フラッシュ内、`sprite_stages.h`)
- 描画: 2xソフトウェアスケール → **128×128** で pushImage
- ボブ: `sin(millis() * 0.0015f) * 3.0f` px の上下移動
- 消去時: `spDrawBackgroundRect()` で宇宙背景と星を復元 (黒塗りしない)
- Main画面のみ呼び出し: `charAnimUpdate(pet.age, 160, 80)`

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

| ボタン | 周波数 | 長さ | 用途 |
|--------|--------|------|------|
| A | C3 131 Hz | 60 ms | Move / Config |
| B | E3 165 Hz | 60 ms | OK / Talk / 確認 |
| C | G3 196 Hz | 60 ms | 画面切り替え / Close |
| 音量ON時 (B on vol) | A3 220 Hz | 150 ms | ON時のプレビュー |

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

## 永続化 (ESP32 Preferences NVS)

namespace: `"fido"` キー: `hunger`, `happy`, `health`, `age`, `coins`, `bond`, `it0`-`it13`, `vol`, `nc`, `nc0`-`nc7`

---

## 既知の制約

- `huge_app` パーティション使用によりアプリ領域は 3MB。OTA パーティションなし
- NASA APOD `DEMO_KEY` のレート制限: 30 req/hour, 50 req/day
- スプライトを増やす場合は SD カード経由が必要 (`convert_sprites.py` を `SIZE=128` に変更)
- `sprite_data.h` は旧ファイル (未使用、削除可)
