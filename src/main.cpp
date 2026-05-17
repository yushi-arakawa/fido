#include <M5Stack.h>
#include "pet.h"
#include "display.h"
#include "claude_client.h"
#include "char_anim.h"
#include "game_data.h"
#include "minigame.h"
#include "shop.h"
#include "status_report.h"
#include "nasa_gacha.h"
#include "remote.h"
#include <Preferences.h>

// ─── キャラ表示位置 ─────────────────────────────────────────────────────
// Main 画面の中央上 (メッセージBOX y=155 の上にちょうど収まる位置)。
// CHAR_CY を変更する場合はキャラの高さ DH=128 と
// メッセージBOX 位置 (display.cpp::MSG_Y=155) を考慮すること。
static const int CHAR_CX = 160;
static const int CHAR_CY = 80;

// ─── ボタン操作音 (M5.Speaker.tone 周波数 Hz) ──────────────────────────
// 機能ごとに音程を分けて触覚的フィードバックを出している。
// 全箇所で同じ周波数を使うため定数化 (旧コードでは 9 箇所に直書き)。
static const uint16_t TONE_A      = 131; // C3 — ナビゲーション/Move
static const uint16_t TONE_B      = 165; // E3 — 決定/Talk/OK
static const uint16_t TONE_C      = 196; // G3 — 画面切替/Close
static const uint16_t TONE_VOL_ON = 220; // A3 — 音量 ON 切替時のプレビュー
static const uint16_t TONE_DUR_MS = 60;  // 既定の鳴動時間

// ─── ゲーム状態 (グローバルだがファイルスコープで局所化) ────────────────
// 永続データ (pet, inv, nasaCargo) は setup() で NVS から読み込まれる。
// {} で全フィールドをゼロ初期化 (Inventory.owned は 14要素全て false に)。
static Pet       pet       = {"Fido", 80, 80, 100, 0, PetMood::Happy};
static Inventory inv       = {};
static NasaCargo nasaCargo = {};
static UIMode    uiMode    = UIMode::Main;
static int       actSel    = 0; // Act 画面で選択中のアクション (0..3)

// ─── tick タイマー ──────────────────────────────────────────────────────
// 30秒ごとに pet.tick() を呼び、ステータスを減衰させて NVS に保存する。
// millis() のオーバーフローは ~49日後だが「差分」で判定しているので問題なし。
static unsigned long lastTick = 0;
static const unsigned long TICK_MS = 30000;

// 画面全体を最初から描き直す。背景・星空・コンテンツ全部を再描画する重い処理。
// モード切り替え直後や、フルスクリーンのサブ画面 (ショップ/ミニゲーム/設定)
// から戻った直後にだけ呼ぶこと。tick による軽量更新では使わない。
//
// charAnimRedraw() はキャラの差分描画キャッシュをリセットして、
// 続く charAnimUpdate() で必ずスプライトを再描画させるためのもの。
static void fullRedraw(const String& msg = "") {
    charAnimRedraw();
    displayInit(uiMode, pet, inv, nasaCargo, actSel);
    if (msg.length() > 0) displayMessage(msg);
}

// ─── アクション実行 (Act 画面で B ボタンを押した時) ─────────────────────
// idx は ACT_LABELS の index と一致 (display.cpp 参照): 0=Feed 1=Play 2=Game 3=Shop
// Feed/Play はその場で完結する軽量アクション (画面遷移なし)。
// Game/Shop はサブ画面に入るので、戻ってきた後に fullRedraw() で UI を復旧する。
static void doAct(int idx) {
    switch (idx) {
        case 0: // Feed: 空腹を大きく回復、幸福もちょっと回復
            pet.hunger    = min(100, (int)pet.hunger    + 30);
            pet.happiness = min(100, (int)pet.happiness + 5);
            pet.mood      = pet.calcMood();
            displayMessage("Yum! Nom nom nom...");
            break;
        case 1: // Play: 幸福を回復するが代わりに空腹が進む
            pet.happiness = min(100, (int)pet.happiness + 20);
            pet.hunger    = max(0,   (int)pet.hunger    - 10);
            pet.mood      = pet.calcMood();
            displayMessage("Wheee! So fun!");
            break;
        case 2: { // Game: ミニゲーム選択画面に入り、稼いだコインを加算
            uint16_t earned = runGameMenu();
            inv.coins += earned;
            saveAll(pet, inv);
            fullRedraw("Game over! +" + String(earned) + " coins");
            break;
        }
        case 3: // Shop: ショップ画面 (中で applyItem→saveAll される)
            runShop(pet, inv);
            saveAll(pet, inv); // 念のため戻り後も保存 (重複コストは無視できる)
            fullRedraw("Thanks for shopping!");
            break;
    }
}

// ─── 初期化 ─────────────────────────────────────────────────────────────
// 起動シーケンス:
//   1. M5.begin() → LCD/Speaker/Buttons/Power 初期化
//   2. NVS から永続データ復元 (pet, inv, nasaCargo, vol)
//   3. ボタン入力なしで時間に依存する乱数を確保
//   4. 起動アニメ (~2秒) を流してから初回フル描画
void setup() {
    M5.begin();
    loadAll(pet, inv);
    loadNasaCargo(nasaCargo);
    lastTick = millis();

    // 音量設定の復元。OFF=0 / ON=1 の2値運用 (詳細は status_report.cpp)。
    {
        Preferences p;
        p.begin("fido", true);
        uint8_t vol = p.getUChar("vol", 1); // 既定: ON
        p.end();
        M5.Speaker.setVolume(vol ? 1 : 0);
    }

    // ESP の真乱数で seed → ミニゲームや NASA ガチャの再現性を排除。
    randomSeed(esp_random());

    charAnimPlayStartup();
    fullRedraw("Hello! I'm " + pet.name + "!");
}

// ─── メインループ ───────────────────────────────────────────────────────
// 構成:
//   - C: 画面循環 (常時受け付け)
//   - A/B: 現在の uiMode によって意味が変わる (画面ごとのハンドラへ分岐)
//   - tick: 30秒ごとのステータス減衰 + 自動保存
//   - char anim: Main 画面だけでスプライトのアイドル動作を更新
// 末尾の delay(16) で約 60fps に制限し ESP 負荷を抑える。
void loop() {
    M5.update();
    remoteSync(); // PC からのシリアル遠隔操作 ('a'/'b'/'c') を取り込む

    // ── PC からのデバッグコマンド (ステータス直接書き換え) ──────────────
    // "h50\n" 等を受けたら pet に反映 → 保存 → 全画面再描画。age 変更時は
    // fullRedraw() 内の charAnimRedraw() でスプライトステージも更新される。
    if (remoteDebugApply(pet)) {
        saveAll(pet, inv);
        fullRedraw();
    }

    // ── C: 画面循環 Main → Act → Back → Main ────────────────────────────
    if (btnC()) {
        M5.Speaker.tone(TONE_C, TONE_DUR_MS);
        if      (uiMode == UIMode::Main) uiMode = UIMode::Act;
        else if (uiMode == UIMode::Act)  uiMode = UIMode::Back;
        else                             uiMode = UIMode::Main;
        const char* modeNames[] = {"Main", "Act", "Back"};
        Serial.printf("[MODE] -> %s\n", modeNames[(int)uiMode]);
        fullRedraw();
    }

    // ── Main 画面 ───────────────────────────────────────────────────────
    if (uiMode == UIMode::Main) {
        // A: NASA APOD ガチャ。WiFi 接続のため数秒ブロックする。
        if (btnA()) {
            M5.Speaker.tone(TONE_A, TONE_DUR_MS);
            displayMessage("Scanning space...");
            // tone() の鳴動は M5.update() 内のポーリングに依存しているため、
            // この後の WiFi ブロッキング中はスピーカーが鳴り続けてしまう。
            // 明示的に mute() で止める。
            M5.Speaker.mute();
            String item = runNasaGacha(nasaCargo);
            if (item == "Cargo full!") {
                displayMessage("Space cargo full! (8/8)");
            } else if (item.length() > 0) {
                saveNasaCargo(nasaCargo);
                displayMessage("Discovery: " + item);
            } else {
                displayMessage("No signal...");
            }
        }

        // B: Talk (Egg ステージはまだ喋れない仕様)
        if (btnB()) {
            if (stageForAge(pet.age) == STAGE_EGG) {
                displayMessage("...(still an egg)");
            } else {
                M5.Speaker.tone(TONE_B, TONE_DUR_MS);
                displayMessage("...");
                displayMessage(askClaude(pet, "How are you feeling?"));
            }
        }
    }

    // ── Act 画面 ────────────────────────────────────────────────────────
    if (uiMode == UIMode::Act) {
        if (btnA()) {
            M5.Speaker.tone(TONE_A, TONE_DUR_MS);
            actSel = (actSel + 1) % 4;
            displayActContent(actSel);
            displayMenuBar(uiMode, actSel);
        }
        if (btnB()) {
            M5.Speaker.tone(TONE_B, TONE_DUR_MS);
            doAct(actSel);
        }
    }

    // ── Back 画面 ───────────────────────────────────────────────────────
    if (uiMode == UIMode::Back) {
        // A: 設定画面を開く (showSettings 内でブロッキング)
        if (btnA()) {
            M5.Speaker.tone(TONE_A, TONE_DUR_MS);
            showSettings(pet, inv);
            fullRedraw();
        }
        // B: 予約 (将来用)
    }

    // ── 30秒 tick: 減衰 + 絆増加 + 保存 ─────────────────────────────────
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick();
        if (inv.bond < 1000) inv.bond++; // 絆は単調増加 (上限 1000 で 5★)
        saveAll(pet, inv);
        Serial.printf("[TICK] age:%d energy:%d morale:%d shield:%d bond:%d\n",
            pet.age, pet.hunger, pet.happiness, pet.health, inv.bond);
        // Back 画面では数値が直接見えるので即時再描画。
        // Main/Act ではキャラアニメや選択UIの方が前面なので更新不要。
        if (uiMode == UIMode::Back) {
            displayBackContent(pet, inv, nasaCargo);
            displayMenuBar(uiMode, actSel);
        }
    }

    // ── キャラのアイドルアニメーション (Main 画面のみ) ──────────────────
    if (uiMode == UIMode::Main) {
        charAnimUpdate(pet.age, CHAR_CX, CHAR_CY);
    }

    delay(16); // ~60fps 上限。バッテリー消費とCPU負荷の妥協点。
}
