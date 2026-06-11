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

// 就寝中の "z z Z" を頭上 (描画ボックス右上の内側) に描く。drawChar の直後に
// 呼ぶ前提。背景透過 (setTextColor 単色) でスプライト上に乗せ、ボックス内
// (cy-64..) に収めることで次フレームの eraseChar に巻き込まれて消える。
static void drawSleepZ(int cx, int cy, int bobY) {
    int bx = cx + 26;             // 右寄り (ボックス右端 cx+64 の内側)
    int by = cy - 56 + bobY;      // 頭上付近 (ボックス上端 cy-64 の内側)
    M5.Lcd.setTextColor(SM_WHITE); // 単色 = 背景透過
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(bx, by);          M5.Lcd.print("Z");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(bx + 16, by + 6); M5.Lcd.print("z");
}

// 全画面再描画後に呼ぶ。次の charAnimUpdate() で必ず描画されるよう
// lastBobY をセンチネル値に戻して差分検出を初期化する。
void charAnimRedraw() { lastBobY = -999; }

// 転生時に呼ぶ完全リセット。currentStage も未初期化に戻すことで、次回更新の
// 「stage != currentStage」分岐が currentStage<0 となり進化演出をスキップする。
void charAnimReset() {
    currentStage = -1;
    lastBobY     = -999;
    lastCX = lastCY = -1;
}

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

// ─── 起動音ヘルパー ──────────────────────────────────────────────────
// 1音を「鳴らす→止める→間を置く」できれいに発音する。
// charAnimPlayStartup() はブロッキングで M5.update() を回さないため、
// tone(freq,dur) の自動停止 (SPEAKER::update 依存) が効かない。よって
// 明示的に mute() して音を切る。これをしないと最後の音が鳴りっぱなしになる。
static void startupBeep(uint16_t freq, uint16_t onMs, uint16_t gapMs) {
    M5.Speaker.tone(freq);
    delay(onMs);
    M5.Speaker.mute();
    if (gapMs) delay(gapMs);
}

// ─── ワープ演出 (起動シーケンスの導入) ─────────────────────────────────
// 中心 (160,120) から星が放射状に加速して流れる ~0.5 秒のハイパースペース風
// アニメーション。フレーム f での線分を「2フレーム前の線を黒で描き直して
// 消す → 新frame を描く」差分方式で更新する。終了後に残る最後の2フレーム分
// は直後の spDrawBackground() が上書きして消すので後始末は不要。
static void warpLine(int f, int i, uint16_t col) {
    const int cx = 160, cy = 120;
    float a  = i * (TWO_PI / 14.0f) + (i % 3) * 0.15f;
    float r0 = 6.0f + f * 8.0f + (i % 4) * 5.0f;
    float r1 = r0 + 6.0f + f * 1.5f;
    M5.Lcd.drawLine(cx + (int)(r0 * cosf(a)), cy + (int)(r0 * sinf(a)),
                    cx + (int)(r1 * cosf(a)), cy + (int)(r1 * sinf(a)), col);
}

static void playWarpIntro() {
    M5.Lcd.fillScreen(SM_BG);
    for (int f = 0; f < 18; f++) {
        if (f >= 2)
            for (int i = 0; i < 14; i++) warpLine(f - 2, i, SM_BG);
        for (int i = 0; i < 14; i++)
            warpLine(f, i, f > 13 ? SM_WHITE : (f & 1) ? SM_LIGHT : SM_GREY);
        M5.Speaker.tone(200 + f * 70); // 上昇ワープ音 (200→1390Hz スイープ)
        delay(28);
    }
    M5.Speaker.mute();
}

// ─── 起動アニメーション ───────────────────────────────────────────────
// setup() から1回だけ呼ばれるブロッキング演出 (~2.5秒)。
// ワープ突入 → 星空展開 → "FIDO" の各文字を C メジャーの上昇アルペジオに
// 合わせて表示 → サブタイトルのタイプライター表示。
// 終了後は呼び出し元の fullRedraw() が画面を上書きするので明示的なクリアは不要。
void charAnimPlayStartup() {
    playWarpIntro();   // ハイパースペースから通常空間へ
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    // "FIDO" を1文字ずつ、C メジャーの上昇アルペジオ C5 E5 G5 C6 に同期して表示。
    // 主音 C(オクターブ上) で終える解決進行にして「鳴り切った」感を出す
    // (旧版は導音 B4 終わりで宙ぶらりんだった)。小型スピーカーが鳴らしやすい
    // 中〜高域を使う (低い C4 はほぼ聞こえずにごる)。
    const uint16_t lnotes[] = {523, 659, 784, 1047}; // C5 E5 G5 C6
    const char letters[] = "FIDO";
    M5.Lcd.setTextSize(6);
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    const int TX = (320 - 4 * 36) / 2;  // 88 — center 4 chars of width 36
    const int TY = 82;
    for (int i = 0; i < 4; i++) {
        M5.Lcd.setCursor(TX + i * 36, TY);
        M5.Lcd.print(letters[i]);
        startupBeep(lnotes[i], 140, 55); // 約195ms間隔で文字送りと同期
    }

    // Subtitle (タイプライター) + 締めのきらめき。
    // E6→C6 と上から主音へ落として終止感を出す。
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(96, TY + 52); // "your space companion" 20文字を中央寄せ
    const char* sub = "your space companion";
    for (const char* p = sub; *p; ++p) {
        M5.Lcd.print(*p);
        delay(22);
    }
    startupBeep(1319, 90,  30); // E6 — 軽いきらめき
    startupBeep(1047, 280, 0);  // C6 — 主音で着地して終止

    M5.Speaker.mute(); // 念のため: ゲーム本編へ音を残さない
    delay(550);
    // fullRedraw() will overwrite the screen; no explicit clear needed
}

// 毎フレーム main.cpp の loop() から呼ばれる (Main 画面のみ)。
// 1. 表示位置が変わっていれば旧位置の背景を復元
// 2. ステージが変わっていれば進化エフェクト + スプライト再ロード
// 3. bobY (浮遊オフセット) が前フレームと違えば消去→再描画
// pushImage() は中で SPI で送るので、変わらない場合はスキップして帯域を節約。
void charAnimUpdate(uint8_t age, int cx, int cy, bool sleeping) {
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
    //     就寝中は振幅・周波数を落として穏やかな寝息のような動きにする。
    float amp  = sleeping ? 1.5f    : BOB_AMP;
    float freq = sleeping ? 0.0008f : BOB_FREQ;
    int bobY = (int)(sinf(millis() * freq) * amp);
    if (bobY != lastBobY) {
        if (lastBobY != -999) eraseChar(cx, cy, lastBobY);
        drawChar(cx, cy, bobY);
        if (sleeping) drawSleepZ(cx, cy, bobY); // ボックス内に描くので次回 erase で消える
        lastBobY = bobY;
    }
}
