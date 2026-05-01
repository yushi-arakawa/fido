#pragma once
#include <M5Stack.h>

// ─── モノクロ・スペースパレット ────────────────────────────────────────
// "宇宙感" を出すために色相を捨てて純粋なグレイスケールで統一している。
// 値は RGB565 (R:5, G:6, B:5)。色を増やしたくなった場合もこのテーブル経由で
// 使うことで全画面のトーンが揃うので、生 RGB565 を直書きしないこと。
#define SM_BG       ((uint16_t)0x0000)  // 黒                    (背景)
#define SM_HDR      ((uint16_t)0x0841)  // 3%   グレー           (ヘッダ帯)
#define SM_SEL      ((uint16_t)0x2945)  // 16%  グレー           (選択ハイライト)
#define SM_DIV      ((uint16_t)0x2104)  // 12%  グレー           (区切り線)
#define SM_DIM      ((uint16_t)0x4208)  // 25%  グレー           (薄い文字 / 未充填バー)
#define SM_GREY     ((uint16_t)0x7BEF)  // 50%  グレー           (本文)
#define SM_LIGHT    ((uint16_t)0xBDF7)  // 75%  グレー           (通常文字)
#define SM_WHITE    ((uint16_t)0xFFFF)  // 白                    (タイトル/強調)
#define SM_BORDER   ((uint16_t)0x4208)  // 枠線色 (実体は SM_DIM と同じ)

// ─── 空のグラデーション背景 ────────────────────────────────────────────
// 上から下へ徐々に明るくなる5層構造。fillRect は内部で SPI でブロック転送
// されるので drawPixel ループより遥かに速い。
// 全画面切替時にだけ呼ぶこと (毎フレーム呼ぶとチラつく)。
// ※ spDrawBackgroundRect の row しきい値 (80/130/180/210) と数値を揃えること。
inline void spDrawBackground() {
    M5.Lcd.fillRect(0,   0, 320,  80, 0x0000);
    M5.Lcd.fillRect(0,  80, 320,  50, 0x0021);
    M5.Lcd.fillRect(0, 130, 320,  50, 0x0062);
    M5.Lcd.fillRect(0, 180, 320,  30, 0x0884);
    M5.Lcd.fillRect(0, 210, 320,  30, 0x08C5);
}

// ─── 星空 (低密度・モノクロ・決定論) ───────────────────────────────────
// 座標 (px,py) からハッシュで明るさを決める疑似乱数描画。
// 同じ座標は常に同じ星になるので、後で部分再描画しても継ぎ目が出ない。
// 数値の意味:
//   - 0x1FF マスクで 1/512 → 実際は < 3 で 3/512 ≈ 0.6% 密度
//   - フルスクリーン (320x240=76800) で約 460 個の星
// drawPixel ループは遅いので、静的画面では一度しか呼ばないこと。
inline void spDrawStarfield(int x, int y, int w, int h) {
    for (int px = x; px < x + w; px++) {
        for (int py = y; py < y + h; py++) {
            // Knuth マルチプリカティブハッシュで再現性のある乱数を生成
            uint32_t v = ((uint32_t)px * 2654435761u) ^ ((uint32_t)py * 2246822519u);
            if ((v & 0x1FF) < 3) {
                uint8_t br = (v >> 9) & 0xFF;
                uint16_t c = br < 80  ? SM_DIM   :
                             br < 160 ? SM_GREY  :
                             br < 230 ? SM_LIGHT : SM_WHITE;
                M5.Lcd.drawPixel(px, py, c);
            }
        }
    }
}

// ─── コーナーアクセント枠 ──────────────────────────────────────────────
// 4隅だけ強調されたシャープな枠。S=6px が L字のアクセント長。
// 全周は薄い SM_DIV、4隅だけ濃い col で描く事で「テック感」を出している。
inline void spCornerFrame(int x, int y, int w, int h, uint16_t col = SM_BORDER) {
    const int S = 6;
    M5.Lcd.drawRect(x, y, w, h, SM_DIV);
    M5.Lcd.drawFastHLine(x,     y,     S, col); M5.Lcd.drawFastVLine(x,     y,     S, col);
    M5.Lcd.drawFastHLine(x+w-S, y,     S, col); M5.Lcd.drawFastVLine(x+w-1, y,     S, col);
    M5.Lcd.drawFastHLine(x,     y+h-1, S, col); M5.Lcd.drawFastVLine(x,     y+h-S, S, col);
    M5.Lcd.drawFastHLine(x+w-S, y+h-1, S, col); M5.Lcd.drawFastVLine(x+w-1, y+h-S, S, col);
}

// ─── 部分背景の再描画 (キャラ消去用) ───────────────────────────────────
// (x,y,w,h) の領域だけ背景グラデ + 星空を再生成する。
// fillRect で黒塗りだけだと星空が消えてしまうため、char_anim でキャラ位置の
// 上下移動をクリアする時にこちらを使う。
// row しきい値は spDrawBackground と同期 (80/130/180/210)。
inline void spDrawBackgroundRect(int x, int y, int w, int h) {
    for (int row = y; row < y + h; row++) {
        uint16_t c = row <  80 ? (uint16_t)0x0000 :
                     row < 130 ? (uint16_t)0x0021 :
                     row < 180 ? (uint16_t)0x0062 :
                     row < 210 ? (uint16_t)0x0884 : (uint16_t)0x08C5;
        M5.Lcd.drawFastHLine(x, row, w, c);
    }
    spDrawStarfield(x, y, w, h);
}

// ─── ステータスバー (0-100%) ───────────────────────────────────────────
// val (0..100) を bw 幅にマッピング。filled 部分の右端にだけ
// SM_LIGHT のハイライトラインを引いてエッジを柔らかく見せる小技。
inline void spBar(int x, int y, int bw, int bh, uint8_t val) {
    int filled = map(val, 0, 100, 0, bw);
    M5.Lcd.fillRect(x, y, bw, bh, SM_DIV); // 背景 (未充填)
    if (filled > 0) {
        M5.Lcd.fillRect(x, y, filled, bh, SM_WHITE);
        if (filled > 1)
            M5.Lcd.drawFastVLine(x + filled - 1, y, bh, SM_LIGHT); // ソフトエッジ
    }
}
