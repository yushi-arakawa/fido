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
#include "world.h"
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
// 音域は起動音 (char_anim.cpp の C5 E5 G5 C6 アルペジオ) に合わせた高域。
// 旧版は 2 オクターブ下の C3/E3/G3 で、小型スピーカーでは低くにごって
// 起動音より弱く聞こえていた。起動音と同じ音を鳴らして雰囲気を統一する。
static const uint16_t TONE_A      = 523; // C5 — ナビゲーション/Move
static const uint16_t TONE_B      = 659; // E5 — 決定/Talk/OK
static const uint16_t TONE_C      = 784; // G5 — 画面切替/Close
static const uint16_t TONE_VOL_ON = 880; // A5 — 音量 ON 切替時のプレビュー
static const uint16_t TONE_EVENT  = 988; // B5 — 宇宙イベント発生のチャイム
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

// ─── 昼夜 / 生死まわりの状態 ────────────────────────────────────────────
// lastNight: 前フレームの昼夜状態。切替を検出して1度だけ fullRedraw する。
// CRIT_LIMIT: health==0 (inv.critStreak) がこの tick 数連続すると「離脱」(死)。
//   health は hunger<20 時のみ -1/tick で削れる (満タンから0まで ~50分) ので、
//   0 到達後さらに 6tick=3分 の猶予を与えてから離脱とする。
static bool lastNight = false;
static const uint8_t CRIT_LIMIT = 6;

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
            if (worldIsNight()) {
                // 夜は就寝中。無理に遊ばせると寝起きで機嫌を損ねる (士気-5)。
                pet.happiness = max(0, (int)pet.happiness - 5);
                pet.mood      = pet.calcMood();
                displayMessage("Zzz... too sleepy to play.");
            } else {
                pet.happiness = min(100, (int)pet.happiness + 20);
                pet.hunger    = max(0,   (int)pet.hunger    - 10);
                pet.mood      = pet.calcMood();
                displayMessage("Wheee! So fun!");
            }
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

// ─── ペット「離脱」(死) → 転生 ──────────────────────────────────────────
// inv.critStreak が CRIT_LIMIT に達した時に loop() の tick 処理から呼ばれる。
//   1. 歴代記録 (bestAge/bestBond/rebirths) を今生の値で更新
//   2. 追悼画面をブロッキング表示 (ボタン待ち)
//   3. 転生: ステータスと絆を初期化。コイン・アイテム・記録は継承
//   4. charAnimReset() で Elder→Egg の逆進化演出を抑止し、Main を再描画
static void handleDeath() {
    if (pet.age  > inv.bestAge)  inv.bestAge  = pet.age;
    if (inv.bond > inv.bestBond) inv.bestBond = inv.bond;
    if (inv.rebirths < 65535)    inv.rebirths++;
    saveAll(pet, inv);
    Serial.printf("[DEATH] Day%d bond%d -> generation #%d\n",
                  pet.age, inv.bond, inv.rebirths);

    showMemorial(pet, inv); // ボタンが押されるまでブロック

    // 転生。name は固定、ステータスは初期値、絆と危機カウンタはリセット。
    // コイン (inv.coins) と所有アイテム (inv.owned) はそのまま引き継ぐ。
    pet            = {"Fido", 80, 80, 100, 0, PetMood::Happy};
    inv.bond       = 0;
    inv.critStreak = 0;
    saveAll(pet, inv);

    uiMode    = UIMode::Main;
    lastNight = worldIsNight();   // 直後の偽の昼夜切替を防ぐ
    charAnimReset();              // 進化演出なしで Egg を静かにロード
    fullRedraw("A new arrival! Welcome back, Fido!");
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

    // ── PC からのデバッグコマンド (値の直接書き換え) ────────────────────
    // "h50"/"m999"/"k800" 等を受けたら pet/inv に反映 → 保存 → 全画面再描画。
    // age 変更時は fullRedraw() 内の charAnimRedraw() でスプライトも更新される。
    if (remoteDebugApply(pet, inv)) {
        saveAll(pet, inv);
        fullRedraw();
    }

    // ── PC からのアクションコマンド (新機能の即検証用) ───────────────────
    // 昼夜固定 / 自動復帰 / 宇宙イベント即発生 / 即離脱(死)。
    // 昼夜オーバーライドは worldIsNight() を即変えるので、直後の昼夜切替検出が
    // 再描画とメッセージ ("Zzz.."/"Good morning") を出してくれる。
    switch (remotePopAction()) {
        case RemoteAction::ForceNight:
            worldSetOverride(DayNightOverride::ForceNight);
            Serial.println("[DEBUG] daynight override -> night");
            break;
        case RemoteAction::ForceDay:
            worldSetOverride(DayNightOverride::ForceDay);
            Serial.println("[DEBUG] daynight override -> day");
            break;
        case RemoteAction::AutoCycle:
            worldSetOverride(DayNightOverride::Auto);
            Serial.println("[DEBUG] daynight override -> auto");
            break;
        case RemoteAction::SpaceEvent: {
            String m;
            worldForceEvent(pet, inv, m);
            saveAll(pet, inv);
            Serial.printf("[EVENT] (forced) %s\n", m.c_str());
            M5.Speaker.tone(TONE_EVENT, 120);
            if (uiMode == UIMode::Back) {
                displayBackContent(pet, inv, nasaCargo);
                displayMenuBar(uiMode, actSel);
            } else {
                displayMessage(m); // Main/Act のメッセージ BOX に表示
            }
            break;
        }
        case RemoteAction::Die:
            Serial.println("[DEBUG] forced death");
            handleDeath(); // 内部で追悼 → 転生 → fullRedraw まで行う
            break;
        case RemoteAction::None:
        default:
            break;
    }

    // ── 昼夜サイクル: 切替を検出したら1度だけ全画面再描画 ─────────────────
    // night は以降の tick 減衰・キャラ就寝表現でも使う (毎フレーム評価)。
    // 切替時のメッセージは Back 画面では出さない (ステータス領域を潰すため)。
    bool night = worldIsNight();
    if (night != lastNight) {
        lastNight = night;
        const char* tmsg = night ? "Zzz... Fido drifts to sleep." : "Good morning, Fido!";
        Serial.printf("[DAYNIGHT] -> %s\n", night ? "night" : "day");
        fullRedraw(uiMode == UIMode::Back ? "" : tmsg);
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

        // B: Talk (Egg ステージはまだ喋れない / 夜は寝ている)
        if (btnB()) {
            if (stageForAge(pet.age) == STAGE_EGG) {
                displayMessage("...(still an egg)");
            } else if (night) {
                // 夜は就寝中。Talk はそっと様子を見るだけ (Play と違い士気減少なし)。
                displayMessage("Zzz... (Fido is fast asleep)");
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

    // ── 30秒 tick: 減衰 + 絆増加 + 宇宙イベント + 生死判定 + 保存 ────────
    if (millis() - lastTick >= TICK_MS) {
        lastTick = millis();
        pet.tick(night);                 // 夜は燃料消費半減・mood=Sleepy
        if (inv.bond < 1000) inv.bond++; // 絆は単調増加 (上限 1000 で 5★)

        // health==0 の連続 tick を数える (NVS 永続)。CRIT_LIMIT で離脱。
        if (pet.health == 0) { if (inv.critStreak < 255) inv.critStreak++; }
        else                   inv.critStreak = 0;

        // ランダム宇宙イベント (卵期は発生しない)。pet/inv を直接書き換える。
        String evMsg;
        bool ev = worldRollEvent(pet, inv, evMsg);

        saveAll(pet, inv);
        Serial.printf("[TICK] age:%d energy:%d morale:%d shield:%d bond:%d crit:%d%s\n",
            pet.age, pet.hunger, pet.happiness, pet.health, inv.bond,
            inv.critStreak, night ? " (night)" : "");

        if (inv.critStreak >= CRIT_LIMIT) {
            handleDeath();   // 追悼 → 転生 → fullRedraw (Main)
        } else {
            if (ev) {
                Serial.printf("[EVENT] %s\n", evMsg.c_str());
                M5.Speaker.tone(TONE_EVENT, 120);
            }
            // Back 画面では数値が直接見えるので即時再描画。
            // Main/Act ではイベント時のみメッセージ BOX に通知を出す。
            if (uiMode == UIMode::Back) {
                displayBackContent(pet, inv, nasaCargo);
                displayMenuBar(uiMode, actSel);
            } else if (ev) {
                displayMessage(evMsg);
            }
        }
    }

    // ── キャラのアイドルアニメーション (Main 画面のみ) ──────────────────
    // 夜は就寝表現 (穏やかな寝息 + zZ)。
    if (uiMode == UIMode::Main) {
        charAnimUpdate(pet.age, CHAR_CX, CHAR_CY, night);
    }

    delay(16); // ~60fps 上限。バッテリー消費とCPU負荷の妥協点。
}
