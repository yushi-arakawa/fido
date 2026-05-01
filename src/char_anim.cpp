#include "char_anim.h"
#include "pet.h"
#include "sprite_stages.h"
#include "space_ui.h"
#include <M5Stack.h>
#include <math.h>

// ─── アニメーション/描画パラメータ ─────────────────────────────────────
// BOB_*: ふわふわ浮遊するアイドルアニメーション。
//   bobY = sin(millis() * BOB_FREQ) * BOB_AMP  → ±3px の上下動
// SCALE: 64x64 のスプライトを 128x128 に最近傍拡大して表示する倍率。
//        変更する場合、DW/DH/scaledBuf のサイズも自動追従する。
static const float BOB_AMP  = 3.0f;
static const float BOB_FREQ = 0.0015f;
static const int   SCALE    = 2;
static const int   DW       = SPRITE_W * SCALE;
static const int   DH       = SPRITE_H * SCALE;

// 拡大済みスプライト (128x128 RGB565 = 32KB) を DRAM に常駐。
// pushImage() が連続したバッファを必要とするので展開しておく必要がある。
// 元の 64x64 ソースバッファ (8KB) は廃止: SPRITES[stage] (フラッシュ常駐) を
// 直接読み出して拡大する事で DRAM 8KB を節約している。
static uint16_t scaledBuf[DW * DH];

// 状態キャッシュ。-1/-999 は「未初期化」のセンチネル値。
static int currentStage = -1;     // 現在ロード中のスプライトインデックス
static int lastBobY     = -999;   // 前フレームの bobY (差分検出用)
static int lastCX       = -1;     // 前フレームのキャラ中心 X
static int lastCY       = -1;     // 前フレームのキャラ中心 Y

// 64x64 → 128x128 の最近傍拡大。フラッシュから1ピクセル読んで4ピクセル書く。
// ステージ遷移時に1回だけ実行される (毎フレームではない)。
static void loadStage(int stage) {
    const uint16_t* src = SPRITES[stage];
    for (int y = 0; y < SPRITE_H; y++) {
        for (int x = 0; x < SPRITE_W; x++) {
            uint16_t px = src[y * SPRITE_W + x];
            int dx = x * SCALE, dy = y * SCALE;
            scaledBuf[ dy      * DW + dx    ] = px;
            scaledBuf[ dy      * DW + dx + 1] = px;
            scaledBuf[(dy + 1) * DW + dx    ] = px;
            scaledBuf[(dy + 1) * DW + dx + 1] = px;
        }
    }
    currentStage = stage;
}

// (cx, cy) がキャラの中心になるように左上座標へ変換して描画。
// 第6引数の透過色 0x0000 (黒) により、スプライトの黒領域は背景が透けて見える。
// → これによりキャラ周りの星空が消えずに見える仕組み。
static void drawChar(int cx, int cy, int bobY) {
    M5.Lcd.pushImage(cx - DW / 2, cy - DH / 2 + bobY,
                     DW, DH, scaledBuf, (uint16_t)0x0000);
}

// 直前フレームの 128x128 領域を背景 (グラデ + 星) で塗り戻す。
// fillRect で黒一色にすると星空が消えてしまうので使わない。
static void eraseChar(int cx, int cy, int bobY) {
    spDrawBackgroundRect(cx - DW / 2, cy - DH / 2 + bobY, DW, DH);
}

// 全画面再描画後に呼ぶ。次の charAnimUpdate() で必ず描画されるよう
// lastBobY をセンチネル値に戻して差分検出を初期化する。
void charAnimRedraw() { lastBobY = -999; }

// ─── 進化エフェクト ────────────────────────────────────────────────────
// ステージが変わった瞬間に1度だけ呼ばれるブロッキング演出。
// 3段構成: ①画面パルス3回 → ②スパーク12フレーム → ③白フラッシュ + 勝利音
static void playEvolveAnim(int cx, int cy) {
    // Phase 1: three full-area pulses with rising tones (C4→E4→G4)
    const uint16_t pnotes[] = {262, 330, 392};
    for (int i = 0; i < 3; i++) {
        M5.Lcd.fillRect(0, 0, 320, 155, SM_WHITE);
        M5.Speaker.tone(pnotes[i], 100);
        delay(130);
        spDrawBackgroundRect(0, 0, 320, 155);
        delay(70);
    }

    // Phase 2: sparkles spiral outward and accumulate
    int pMinX = cx, pMinY = cy, pMaxX = cx, pMaxY = cy;
    for (int frame = 0; frame < 12; frame++) {
        float r  = 30.0f + frame * 9.0f;
        float ao = frame * 12.0f * (PI / 180.0f);
        uint16_t col = (frame & 1) ? SM_WHITE : (uint16_t)0xFFE0;
        for (int p = 0; p < 8; p++) {
            float angle = p * (PI / 4.0f) + ao;
            int px = (int)(cx + r * cosf(angle));
            int py = (int)(cy + r * sinf(angle));
            if (px >= 2 && px < 318 && py >= 2 && py < 153) {
                M5.Lcd.fillRect(px - 1, py - 1, 3, 3, col);
                if (px - 2 < pMinX) pMinX = px - 2;
                if (py - 2 < pMinY) pMinY = py - 2;
                if (px + 3 > pMaxX) pMaxX = px + 3;
                if (py + 3 > pMaxY) pMaxY = py + 3;
            }
        }
        delay(35);
    }
    if (pMaxX > pMinX) spDrawBackgroundRect(pMinX, pMinY, pMaxX - pMinX, pMaxY - pMinY);

    // Phase 3: final full-area flash + victory tone (C5)
    M5.Lcd.fillRect(0, 0, 320, 155, SM_WHITE);
    M5.Speaker.tone(523, 350);
    delay(350);
    spDrawBackgroundRect(0, 0, 320, 155);
}

// ─── 起動アニメーション ───────────────────────────────────────────────
// setup() から1回だけ呼ばれるブロッキング演出 (~2秒)。
// "FIDO" の各文字をアルペジオに合わせて表示するだけ。
// 終了後は呼び出し元の fullRedraw() が画面を上書きするので明示的なクリアは不要。
void charAnimPlayStartup() {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    // "FIDO" letter by letter with ascending arpeggio (C4 E4 G4 B4)
    const uint16_t lnotes[] = {262, 330, 392, 494};
    const char letters[] = "FIDO";
    M5.Lcd.setTextSize(6);
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    const int TX = (320 - 4 * 36) / 2;  // 88 — center 4 chars of width 36
    const int TY = 82;
    for (int i = 0; i < 4; i++) {
        M5.Lcd.setCursor(TX + i * 36, TY);
        M5.Lcd.print(letters[i]);
        M5.Speaker.tone(lnotes[i], 80);
        delay(200);
    }

    // Subtitle
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(78, TY + 52);
    M5.Lcd.print("   your space companion");
    delay(400);

    delay(600);
    // fullRedraw() will overwrite the screen; no explicit clear needed
}

// 毎フレーム main.cpp の loop() から呼ばれる (Main 画面のみ)。
// 1. 表示位置が変わっていれば旧位置の背景を復元
// 2. ステージが変わっていれば進化エフェクト + スプライト再ロード
// 3. bobY (浮遊オフセット) が前フレームと違えば消去→再描画
// pushImage() は中で SPI で送るので、変わらない場合はスキップして帯域を節約。
void charAnimUpdate(uint8_t age, int cx, int cy) {
    int stage = (int)stageForAge(age);

    // (1) キャラの基準座標が動いた場合、旧位置を消す。lastCX==-1 は初回フレーム。
    if ((cx != lastCX || cy != lastCY) && lastBobY != -999 && lastCX != -1) {
        eraseChar(lastCX, lastCY, lastBobY);
        lastBobY = -999;
    }
    lastCX = cx;
    lastCY = cy;

    // (2) ステージ進化検知。currentStage<0 は起動直後 (進化演出を出さない)。
    if (stage != currentStage) {
        if (lastBobY != -999) eraseChar(cx, cy, lastBobY);
        if (currentStage >= 0) playEvolveAnim(cx, cy);
        loadStage(stage);
        lastBobY = -999; // 強制再描画
    }

    // (3) 浮遊アニメーション差分更新。bobY が同じなら何もしない。
    int bobY = (int)(sinf(millis() * BOB_FREQ) * BOB_AMP);
    if (bobY != lastBobY) {
        if (lastBobY != -999) eraseChar(cx, cy, lastBobY);
        drawChar(cx, cy, bobY);
        lastBobY = bobY;
    }
}
