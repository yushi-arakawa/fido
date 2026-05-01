#include "shop.h"
#include "space_ui.h"
#include <M5Stack.h>

// 一度に画面に表示するアイテム行数。ITEM_COUNT > VISIBLE で
// スクロール表示になる (sel を中心に表示窓が動く)。
static const int VISIBLE = 5;

// ヘッダ + アイテム一覧 + フッタを描画するが、背景には触れない。
// runShop の入口で背景を1回だけ描き、navigation 中はこの関数で
// コンテンツだけ書き換えてチラつきを防いでいる。
static void drawShopContent(int sel, const Inventory& inv) {
    // Header
    M5.Lcd.fillRect(0, 0, 320, 28, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 6);
    M5.Lcd.print("Shop");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(220, 11);
    M5.Lcd.printf("$%d", inv.coins);
    M5.Lcd.drawFastHLine(0, 28, 320, SM_BORDER);

    // sel を中心にスクロールウィンドウを決定。両端ではクランプ。
    // 例: ITEM_COUNT=14, VISIBLE=5, sel=0  → start=0
    //     ITEM_COUNT=14, VISIBLE=5, sel=13 → start=9
    int start = max(0, min(sel - VISIBLE / 2, ITEM_COUNT - VISIBLE));

    for (int i = 0; i < VISIBLE && (start + i) < ITEM_COUNT; i++) {
        int idx      = start + i;
        int y        = 32 + i * 38;
        bool selected = (idx == sel);
        bool owned    = inv.owned[idx];

        uint16_t rowBg = selected ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(2, y, 316, 35, rowBg);
        M5.Lcd.drawRect(2, y, 316, 35, selected ? SM_BORDER : SM_DIV);
        if (selected) {
            M5.Lcd.fillRect(2, y, 2, 35, SM_WHITE); // left accent
        }

        // Name
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(
            owned ? SM_DIM : (selected ? SM_WHITE : SM_LIGHT),
            rowBg);
        M5.Lcd.setCursor(10, y + 3);
        M5.Lcd.print(ITEM_DEFS[idx].name);

        // Description
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(selected ? SM_LIGHT : SM_GREY, rowBg);
        M5.Lcd.setCursor(10, y + 24);
        M5.Lcd.print(ITEM_DEFS[idx].desc);

        // Price / owned
        M5.Lcd.setCursor(240, y + 11);
        if (owned) {
            M5.Lcd.setTextColor(SM_DIM, rowBg);
            M5.Lcd.print("Owned");
        } else {
            M5.Lcd.setTextColor(selected ? SM_WHITE : SM_LIGHT, rowBg);
            M5.Lcd.printf("$%d", ITEM_DEFS[idx].cost);
        }
    }

    // Scroll indicator
    if (ITEM_COUNT > VISIBLE) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(SM_DIM, SM_BG);
        M5.Lcd.setCursor(294, 122);
        M5.Lcd.printf("%d/%d", sel + 1, ITEM_COUNT);
    }

    // Footer
    M5.Lcd.fillRect(0, 225, 320, 15, SM_HDR);
    M5.Lcd.drawFastHLine(0, 225, 320, SM_BORDER);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY,  SM_HDR); M5.Lcd.setCursor(10,  229); M5.Lcd.print("[A] Move");
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR); M5.Lcd.setCursor(115, 229); M5.Lcd.print("[B] Buy");
    M5.Lcd.setTextColor(SM_GREY,  SM_HDR); M5.Lcd.setCursor(225, 229); M5.Lcd.print("[C] Back");
}

// ショップ画面のメインループ。
// A=次のアイテムへ、B=購入 (コイン足りる && 未所有なら)、C=戻る。
// 購入は不可逆 (返金/売却なし)。
void runShop(Pet& pet, Inventory& inv) {
    int  sel     = 0;
    bool running = true;

    // 入口で1回だけ背景を描き、以降は drawShopContent() で内容だけ更新。
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);
    spCornerFrame(0, 0, 320, 240);
    drawShopContent(sel, inv);

    while (running) {
        M5.update();

        if (M5.BtnA.wasPressed()) {
            sel = (sel + 1) % ITEM_COUNT;
            drawShopContent(sel, inv);
        }

        if (M5.BtnB.wasPressed()) {
            // 所持済み or コイン不足はサイレントで無視 (UI 側で価格/Owned 表示済み)
            if (!inv.owned[sel] && inv.coins >= ITEM_DEFS[sel].cost) {
                inv.coins -= ITEM_DEFS[sel].cost;
                inv.owned[sel] = true;
                applyItem(pet, sel);  // ステータス即時反映
                saveAll(pet, inv);    // 永続化 (ここで失敗しても致命的ではない)
                drawShopContent(sel, inv);

                // "Acquired!" フラッシュ表示 (~800ms)。次のフレームで通常表示に戻る。
                M5.Lcd.fillRect(90, 100, 140, 28, SM_SEL);
                M5.Lcd.drawRect(90, 100, 140, 28, SM_BORDER);
                M5.Lcd.setTextColor(SM_WHITE, SM_SEL);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setCursor(104, 108);
                M5.Lcd.print("Acquired!");
                delay(800);
                drawShopContent(sel, inv);
            }
        }

        if (M5.BtnC.wasPressed()) running = false;
        delay(16);
    }
}
