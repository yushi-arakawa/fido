#include "status_report.h"
#include "space_ui.h"
#include <M5Stack.h>
#include <Preferences.h>

// ─── 音量ヘルパー ──────────────────────────────────────────────────────
// 音量は ON/OFF の 2値運用。NVS に 0/1 で保存する (キー "vol")。
// M5.Speaker.setVolume(0) は完全ミュート、setVolume(1) でデフォルト音量。
// 値域を増やしたい場合 (段階的調整など) は main.cpp の起動時ロードと
// drawSettings の表示文字も合わせて修正すること。

static uint8_t loadVolume() {
    Preferences p;
    p.begin("fido", true);
    uint8_t v = p.getUChar("vol", 1); // 初回起動時の既定: ON
    p.end();
    return v ? 1 : 0;
}

static void saveVolume(uint8_t v) {
    Preferences p;
    p.begin("fido", false);
    p.putUChar("vol", v);
    p.end();
    M5.Speaker.setVolume(v ? 1 : 0); // 即時反映 (再起動不要)
}

// ─── 確認ダイアログ ────────────────────────────────────────────────────
// モーダル風の中央寄せ枠。A=確定 (165Hz E3 を鳴らす), C=キャンセル (131Hz C3)。
// ダイアログ表示中は他の処理は走らない (ブロッキング)。
bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    M5.Lcd.fillRect(30, 55, 260, 130, SM_HDR);
    spCornerFrame(30, 55, 260, 130);

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(44, 74);  M5.Lcd.print(line1);
    if (line2 && line2[0]) {
        M5.Lcd.setCursor(44, 90); M5.Lcd.print(line2);
    }
    M5.Lcd.drawFastHLine(30, 148, 260, SM_DIV);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setCursor(44,  158); M5.Lcd.print(confirmLabel);
    M5.Lcd.setTextColor(SM_DIM, SM_HDR);
    M5.Lcd.setCursor(210, 158); M5.Lcd.print("[C] Cancel");

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) { M5.Speaker.tone(165, 80); return true; }
        if (M5.BtnC.wasPressed()) { M5.Speaker.tone(131, 60); return false; }
        delay(20);
    }
}

// ─── 設定画面 ──────────────────────────────────────────────────────────
// 項目を追加する場合は items[] と showSettings の sel ループ (`% 3`)、
// および B 押下時の `if (sel == ?)` 分岐を全部更新すること。
static void drawSettings(int sel, uint8_t vol) {
    // Header
    M5.Lcd.fillRect(0, 0, 320, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(8, 4);
    M5.Lcd.print("Config");

    // Items
    struct { const char* label; } items[3] = {
        {"Sound Volume"},
        {"Power Off"},
        {"Clear All Data"},
    };

    for (int i = 0; i < 3; i++) {
        int y = 30 + i * 40;
        bool active = (i == sel);
        uint16_t bg = active ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(0, y, 320, 38, bg);
        if (active) M5.Lcd.fillRect(0, y, 3, 38, SM_WHITE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(active ? SM_WHITE : SM_GREY, bg);
        M5.Lcd.setCursor(12, y + 6);
        M5.Lcd.printf("%s %s", active ? ">" : " ", items[i].label);

        if (i == 0) {
            M5.Lcd.setTextColor(SM_DIM, bg);
            M5.Lcd.setCursor(12, y + 20);
            M5.Lcd.print(vol ? "ON " : "OFF");
        }
    }

    spCornerFrame(0, 0, 320, 160);

    // Footer
    M5.Lcd.fillRect(0, 218, 320, 22, SM_BG);
    M5.Lcd.drawFastHLine(0, 218, 320, SM_DIV);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  225); M5.Lcd.print("[A] Move");
    M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, 225); M5.Lcd.print("[B] Select");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, 225); M5.Lcd.print("[C] Close");
}

// 設定画面ループ。Back 画面の A ボタンから入る。
// pet/inv は将来用 (現在のロジックでは使わないので警告抑止のためだけに参照)。
void showSettings(Pet& pet, Inventory& inv) {
    (void)pet; (void)inv; // unused (将来項目追加時用)

    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    int     sel = 0;
    uint8_t vol = loadVolume();
    drawSettings(sel, vol);

    while (true) {
        M5.update();

        if (M5.BtnA.wasPressed()) {
            M5.Speaker.tone(131, 60); // C3 — Move
            sel = (sel + 1) % 3;
            drawSettings(sel, vol);
        }

        if (M5.BtnB.wasPressed()) {
            if (sel == 0) {
                // 音量トグル: 即座に反映 (再起動なし)。
                // ON にした時だけ A3 を鳴らして「鳴ること」を確認できるようにする。
                vol = 1 - vol;
                saveVolume(vol);
                if (vol) M5.Speaker.tone(220, 150); // A3 preview
                drawSettings(sel, vol);
            } else if (sel == 1) {
                // 電源 OFF: 確認後 M5.Power.powerOFF() で完全停止。
                if (showConfirmDialog("Power off?", "", "[A] Power Off")) {
                    M5.Power.powerOFF();
                }
                // キャンセル時はダイアログで上書きされた背景を再生成して戻る
                spDrawBackground();
                spDrawStarfield(0, 0, 320, 240);
                drawSettings(sel, vol);
            } else if (sel == 2) {
                // データ全削除: NVS namespace ごと clear() してから再起動。
                // M5.Speaker.end() を先に呼ばないと、リセット中に tone が
                // 鳴り続けて電源シーケンスを乱すケースがあるので注意。
                if (showConfirmDialog("Clear all data?",
                                      "This cannot be undone.",
                                      "[A] Confirm")) {
                    M5.Speaker.end();
                    Preferences p;
                    p.begin("fido", false);
                    p.clear();
                    p.end();
                    ESP.restart();
                }
                spDrawBackground();
                spDrawStarfield(0, 0, 320, 240);
                drawSettings(sel, vol);
            }
        }

        if (M5.BtnC.wasPressed()) { M5.Speaker.tone(196, 60); break; } // G3 — Close

        delay(20);
    }
}
