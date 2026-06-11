#include "fx.h"
#include "space_ui.h"
#include "char_anim.h"
#include <M5Stack.h>

// ─── 効果音ヘルパー ─────────────────────────────────────────────────────
// FX はブロッキング区間で M5.update() を回さないため、tone(freq, dur) の
// 自動停止 (M5.Speaker.update 依存) が効かない。明示的に mute する。
// (char_anim.cpp::startupBeep と同じ理由。詳細はそちらのコメント参照)
static void fxBeep(uint16_t freq, uint16_t onMs) {
    M5.Speaker.tone(freq);
    delay(onMs);
    M5.Speaker.mute();
}

// ─── バックライト調光 ──────────────────────────────────────────────────
// PWM 値はライブラリ既定 80 を挟む 100(昼)/45(夜)。夜は「消灯前の機内灯」
// くらいの薄暗さにして就寝表現と画面ノイズ低減を兼ねる。
static const uint8_t BRIGHT_DAY   = 100;
static const uint8_t BRIGHT_NIGHT = 45;
static uint8_t curBright = BRIGHT_DAY;

uint8_t fxPhaseBrightness(bool night) { return night ? BRIGHT_NIGHT : BRIGHT_DAY; }

void fxInitBrightness(bool night) {
    curBright = fxPhaseBrightness(night);
    M5.Lcd.setBrightness(curBright);
}

// 3/14ms 刻みの線形ランプ。昼夜差 55 なら ~260ms、0→昼 100 なら ~470ms。
void fxRampTo(uint8_t target) {
    while (curBright != target) {
        int next = curBright + (target > curBright ? 3 : -3);
        if ((target > curBright) ? (next > target) : (next < target)) next = target;
        curBright = (uint8_t)next;
        M5.Lcd.setBrightness(curBright);
        delay(14);
    }
}

void fxDayNightRamp(bool night) { fxRampTo(fxPhaseBrightness(night)); }

// ─── 瞬く星 (Main 画面の固定8座標) ─────────────────────────────────────
// 座標は全て「安全地帯」: キャラ描画ボックス (x96..224, y10..150)、惑星
// (x12..40, y16..32)、三日月 (x290..310, y10..30)、デブリ3座標 (display.cpp
// JUNK_POS ±アイコンサイズ)、メッセージBOX (y>=155) のどれとも重ならない。
// 明滅は 6 段階の輝度テーブルを周期 ~350ms で巡回し、ピーク時のみ十字の
// きらめきを足す。消去は spDrawBackgroundRect(3x3) なので背景の星/星雲も
// 正しく復元される。
struct TwinkleStar { int16_t x, y; };
static const TwinkleStar TW[] = {
    { 22,  52}, { 70,  30}, { 60,  92}, { 30, 132},
    {244,  40}, {286,  76}, {302, 118}, {236, 142},
};
static const int TW_N = sizeof(TW) / sizeof(TW[0]);
static int8_t twPhase[TW_N];  // 前回描画した輝度段階 (-1 = 未描画)

static const uint16_t TW_LEVELS[6] = {
    SM_DIM, SM_GREY, SM_LIGHT, SM_WHITE, SM_LIGHT, SM_GREY
};
static const int TW_PEAK = 3; // TW_LEVELS[3] = SM_WHITE がピーク

static void twinkleUpdate() {
    for (int i = 0; i < TW_N; i++) {
        // 星ごとに 1.7 秒ずつ位相をずらして同期明滅を避ける
        int8_t ph = (int8_t)(((millis() + (unsigned long)i * 1700) / 350) % 6);
        if (ph == twPhase[i]) continue;
        int x = TW[i].x, y = TW[i].y;
        spDrawBackgroundRect(x - 1, y - 1, 3, 3); // 前回の十字ごと消す
        M5.Lcd.drawPixel(x, y, TW_LEVELS[ph]);
        if (ph == TW_PEAK) { // ピークで十字のきらめき
            M5.Lcd.drawPixel(x - 1, y, SM_GREY); M5.Lcd.drawPixel(x + 1, y, SM_GREY);
            M5.Lcd.drawPixel(x, y - 1, SM_GREY); M5.Lcd.drawPixel(x, y + 1, SM_GREY);
        }
        twPhase[i] = ph;
    }
}

// ─── 流れ星 (非ブロッキングのステートマシン) ───────────────────────────
// 出発域 x228..267, y36..69 から (+5,+2)/step で右下へ 12 step 流れる。
// この経路はキャラ (x<=224)・月 (y<=30)・右デブリ (y>=122) のいずれにも
// 触れない。頭 2x2 白 + 尾 2px の彗星型。28ms/step で全体 ~340ms。
static const int SHOOT_STEPS = 12;
static const int SHOOT_DX = 5, SHOOT_DY = 2;
static int8_t        shootFrame  = -1; // -1 = 待機中
static int16_t       shootX0, shootY0;
static unsigned long shootStepAt = 0;  // 次 step の時刻
static unsigned long shootNextAt = 0;  // 次の出現予定時刻

static void shootScheduleNext(bool night) {
    // 夜は流星が出やすい (8-28秒)。昼は控えめ (15-45秒)。
    shootNextAt = millis() + (night ? 8000 + random(20000)
                                    : 15000 + random(30000));
}

// step f での頭座標
static inline int shootPX(int f) { return shootX0 + f * SHOOT_DX; }
static inline int shootPY(int f) { return shootY0 + f * SHOOT_DY; }

static void shootEnd() {
    // 残っている頭と尾 (直近3 step 分) を背景で塗り戻す
    for (int f = shootFrame - 3; f <= shootFrame - 1; f++)
        if (f >= 0) spDrawBackgroundRect(shootPX(f), shootPY(f), 2, 2);
    shootFrame = -1;
}

static void shootUpdate(bool night) {
    unsigned long now = millis();
    if (shootFrame < 0) { // 待機中: 出現時刻が来たら開始
        if (now < shootNextAt) return;
        shootX0 = 228 + (int16_t)random(40);
        shootY0 =  36 + (int16_t)random(34);
        shootFrame  = 0;
        shootStepAt = now;
        shootScheduleNext(night);
        return;
    }
    if (now < shootStepAt) return;
    shootStepAt = now + 28;

    int f = shootFrame;
    if (f >= SHOOT_STEPS || shootPX(f) > 312) { shootEnd(); return; }

    if (f >= 1) { // 直前の頭を尾 (明) に格下げ
        spDrawBackgroundRect(shootPX(f - 1), shootPY(f - 1), 2, 2);
        M5.Lcd.drawPixel(shootPX(f - 1), shootPY(f - 1), SM_LIGHT);
    }
    if (f >= 2) M5.Lcd.drawPixel(shootPX(f - 2), shootPY(f - 2), SM_DIM); // 尾 (暗)
    if (f >= 3) spDrawBackgroundRect(shootPX(f - 3), shootPY(f - 3), 2, 2); // 最古を消す
    M5.Lcd.fillRect(shootPX(f), shootPY(f), 2, 2, SM_WHITE); // 新しい頭
    shootFrame++;
}

void fxAmbientReset() {
    for (int i = 0; i < TW_N; i++) twPhase[i] = -1; // 次 update で必ず再描画
    shootFrame = -1;                                // 飛行中の流れ星は破棄
    shootNextAt = millis() + 6000 + random(12000);  // 画面に入って少ししたら1発目
}

void fxAmbientUpdate(bool night) {
    twinkleUpdate();
    shootUpdate(night);
}

// ─── 宇宙イベントのミニアニメ ──────────────────────────────────────────
// すべて y0..152 (メッセージBOX の上) で完結するブロッキング演出。
// キャラ領域への侵食は許容し、終了時に fxEvent() が charAnimRedraw() で
// スプライトを次フレーム再描画させる。背景は各 FX が自前で復元する。

// Meteor: 4本の流星痕が左上から右下へ次々と走る (描いて → 一括で消す)
static void fxMeteorShower() {
    for (int s = 0; s < 4; s++) {
        int x = 10 + (int)random(60);
        int y =  6 + (int)random(50);
        M5.Speaker.tone(1568 - s * 160); // 下降ブリップ (鳴らしっぱなし→後で mute)
        int f = 0;
        for (; f < 14; f++) {
            int hx = x + f * 9, hy = y + f * 3;
            if (hx > 308 || hy > 148) break;
            M5.Lcd.fillRect(hx, hy, 3, 2, (f & 3) ? SM_LIGHT : SM_WHITE);
            delay(10);
        }
        M5.Speaker.mute();
        spDrawBackgroundRect(x, y, min(f * 9 + 3, 320 - x), min(f * 3 + 2, 152 - y));
        delay(40);
    }
}

// Supply: 補給クレートがパラシュートでキャラの頭上へ降下
static void fxSupplyDrone() {
    const int x = 152;          // クレート左端 (中心 160 に合わせる)
    int prevY = -1;
    for (int yy = 8; yy <= 60; yy += 4) {
        if (prevY >= 0) spDrawBackgroundRect(x - 8, prevY - 10, 32, 26);
        // キャノピー (三角) + 吊り紐 + クレート
        M5.Lcd.fillTriangle(x - 6, yy - 2, x + 22, yy - 2, x + 8, yy - 10, SM_GREY);
        M5.Lcd.drawLine(x - 6, yy - 2, x + 4,  yy + 4, SM_DIM);
        M5.Lcd.drawLine(x + 22, yy - 2, x + 12, yy + 4, SM_DIM);
        M5.Lcd.fillRect(x + 2, yy + 4, 12, 10, SM_LIGHT);
        M5.Lcd.drawRect(x + 2, yy + 4, 12, 10, SM_DIM);
        if ((yy & 7) == 0) fxBeep(659, 12); else delay(12); // ゆれる搬送音
        delay(26);
        prevY = yy;
    }
    delay(160);
    spDrawBackgroundRect(x - 8, prevY - 10, 32, 26);
}

// CosmicRay: 明滅する垂直のシャワーが次々と画面を走る
static void fxCosmicRays() {
    for (int s = 0; s < 8; s++) {
        int x = 12 + (int)random(296);
        uint16_t c = (s & 1) ? SM_LIGHT : SM_WHITE;
        for (int yy = 4; yy < 150; yy += 6)
            M5.Lcd.drawFastVLine(x, yy, 3, c);
        fxBeep(1175 + s * 60, 18);
        delay(24);
        spDrawBackgroundRect(x, 0, 1, 152);
    }
}

// Flare: 白帯のスイープ2回 + 全面フラッシュ (警告の2音)
static void fxSolarFlare() {
    for (int s = 0; s < 2; s++) {
        for (int yy = 0; yy < 150; yy += 12) {
            M5.Lcd.fillRect(0, yy, 320, 5, SM_WHITE);
            delay(12);
            spDrawBackgroundRect(0, yy, 320, 5);
        }
    }
    M5.Lcd.fillRect(0, 0, 320, 152, SM_WHITE);
    fxBeep(988, 70);
    fxBeep(740, 110);
    spDrawBackgroundRect(0, 0, 320, 152);
}

// Wormhole: キャラ中心 (160,80) から同心円が広がる時空の歪み
static void fxWormhole() {
    for (int r = 12; r <= 66; r += 6) {
        bool odd = (r / 6) & 1;
        M5.Lcd.drawCircle(160, 80, r, odd ? SM_LIGHT : SM_GREY);
        fxBeep(odd ? 1047 : 784, 22);
        delay(20);
    }
    delay(120);
    spDrawBackgroundRect(90, 10, 141, 141); // r=66 の円を覆う矩形を一括復元
}

void fxEvent(WorldEventType t) {
    switch (t) {
        case WorldEventType::Meteor:    fxMeteorShower(); break;
        case WorldEventType::Supply:    fxSupplyDrone();  break;
        case WorldEventType::CosmicRay: fxCosmicRays();   break;
        case WorldEventType::Flare:     fxSolarFlare();   break;
        case WorldEventType::Wormhole:  fxWormhole();     break;
        case WorldEventType::None:      return;
    }
    // FX がキャラ領域を塗り戻している可能性があるため、次フレームで
    // スプライトを必ず再描画させる (背景は復元済みなので erase は走らない)。
    charAnimRedraw();
    // 瞬く星も巻き込まれた可能性があるので次 update で再描画させる
    fxAmbientReset();
}
