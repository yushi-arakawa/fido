#include "status_report.h"
#include "space_ui.h"
#include "remote.h"
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
// モーダル風の中央寄せ枠。A=確定 (659Hz E5 を鳴らす), C=キャンセル (523Hz C5)。
// 周波数は main.cpp の TONE_A/TONE_B/TONE_C と揃える (起動音と同じ高域)。
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
        M5.update(); remoteSync();
        if (btnA()) { M5.Speaker.tone(659, 80); return true; }  // E5 — 確定
        if (btnC()) { M5.Speaker.tone(523, 60); return false; } // C5 — キャンセル
        delay(20);
    }
}

// ─── 追悼画面 (ペット死亡 → 転生) ──────────────────────────────────────
// health==0 が CRIT_LIMIT tick 続いて「離脱」した時に main から呼ばれる。
// 今生の記録と歴代ベストを見せ、ボタン押下で次世代を迎える (転生は呼び出し
// 側が実施)。下降する別れの和音 (G4→E4→C4) を1度だけ鳴らす。
void showMemorial(const Pet& pet, const Inventory& inv) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(46, 36); M5.Lcd.print("Returned to");
    M5.Lcd.setCursor(92, 60); M5.Lcd.print("the stars");

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(60, 98);
    M5.Lcd.printf("%s lived to Day %d", pet.name.c_str(), pet.age);

    int stars = min(5, (int)(inv.bond / 200));
    M5.Lcd.setCursor(60, 112);
    M5.Lcd.print("Final bond: ");
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    for (int i = 0; i < 5; i++) M5.Lcd.print(i < stars ? "*" : "-");

    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(60, 132);
    M5.Lcd.printf("Best: Day %d   Bond %d", inv.bestAge, inv.bestBond);
    M5.Lcd.setCursor(60, 144);
    M5.Lcd.printf("Generation #%d", inv.rebirths);

    M5.Lcd.drawFastHLine(30, 172, 260, SM_DIV);
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    M5.Lcd.setCursor(48, 188); M5.Lcd.print("Coins & cargo carry over.");
    M5.Lcd.setCursor(48, 202); M5.Lcd.print("Press any button to go on");

    spCornerFrame(0, 0, 320, 240);

    // 別れの下降和音 (G4 E4 C4)。startupBeep 相当を直書き (mute で確実に切る)。
    const uint16_t fnotes[] = {392, 330, 262};
    for (int i = 0; i < 3; i++) {
        M5.Speaker.tone(fnotes[i], 240);
        delay(260);
        M5.Speaker.mute();
    }

    // いずれかのボタンで次世代へ。
    while (true) {
        M5.update(); remoteSync();
        if (btnA() || btnB() || btnC()) { M5.Speaker.tone(523, 80); break; }
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
        M5.update(); remoteSync();

        if (btnA()) {
            M5.Speaker.tone(523, 60); // C5 — Move
            sel = (sel + 1) % 3;
            drawSettings(sel, vol);
        }

        if (btnB()) {
            if (sel == 0) {
                // 音量トグル: 即座に反映 (再起動なし)。
                // ON にした時だけ A5 を鳴らして「鳴ること」を確認できるようにする。
                vol = 1 - vol;
                saveVolume(vol);
                if (vol) M5.Speaker.tone(880, 150); // A5 preview
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

        if (btnC()) { M5.Speaker.tone(784, 60); break; } // G5 — Close

        delay(20);
    }
}
