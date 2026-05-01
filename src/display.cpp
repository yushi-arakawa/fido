#include "display.h"
#include "space_ui.h"
#include "nasa_gacha.h"
#include <M5Stack.h>

// ─── 画面レイアウト定数 ──────────────────────────────────────────────────
// 320x240 の TFT を以下の領域に分割している:
//   y=  0..154   : メイン領域 (Act ではアクション一覧 / Back ではステータス全体)
//   y=155..217   : メッセージ BOX (Main/Act 共通)
//   y=218..239   : ボタンガイド帯
// 数値を変える場合は char_anim の CHAR_CY=80 や displayBackContent の
// y 座標群とも整合させること。
static const int MSG_Y  = 155;  // メッセージ BOX の上端
static const int MSG_H  = 63;   // メッセージ BOX の高さ (155+63=218 = MENU_Y と一致)
static const int MENU_Y = 218;  // ボタンガイド帯の上端
static const int MENU_H = 22;   // ボタンガイド帯の高さ
static const int PDIV   = 165;  // Act 画面の左/右パネル境界 X
static const int SX     = 8;    // 各画面のテキスト基準 X

// ─── Act 画面右パネル "Fido tips" ──────────────────────────────────────
// 2行に分かれた tips を 10秒ごとに巡回表示する (displayActContent の
// `(millis() / 10000) % 10` で index を決定)。
// 増やす場合は L1/L2 を必ず同数で揃え、ループ側の `% 10` も合わせて更新する。
static const char* TIPS_L1[10] = {
    "Feeds on stray photons",
    "Stretches 3 light-years",
    "Purring generates",
    "Sheds dark matter",
    "Sleeps by folding into",
    "Sees across 17",
    "Fave toy: collapsed",
    "Its meows travel",
    "Hairballs contain",
    "Tail wags slightly",
};
static const char* TIPS_L2[10] = {
    "near event horizons.",
    "when yawning.",
    "micro-singularities.",
    "instead of fur.",
    "a 2D manifold.",
    "dimensions at once.",
    "neutron star (small).",
    "faster than light.",
    "compressed spacetime.",
    "warp local gravity.",
};

// ── Helpers ───────────────────────────────────────────────────────────────

// ステージ表示名。閾値は pet.h::stageForAge() に集約してあるので、
// この関数は単なる enum→文字列のマッピングだけ。
// (旧実装は age<20=Egg と書かれていてスプライト側の age<4=Egg と矛盾していた)
static const char* stageName(uint8_t age) {
    switch (stageForAge(age)) {
        case STAGE_EGG:   return "Egg";
        case STAGE_CHILD: return "Child";
        case STAGE_TEEN:  return "Teen";
        case STAGE_YOUNG: return "Young";
        case STAGE_ELDER: return "Elder";
    }
    return "?";
}

// PetMood 列挙値 → 表示文字列 (絵文字風 ASCII suffix 付き)。
// Back 画面のステータス表示と Advisor 行で使う。
static const char* moodLabel(PetMood m) {
    switch (m) {
        case PetMood::Happy:   return "Happy  :D";
        case PetMood::Hungry:  return "Hungry :(";
        case PetMood::Sleepy:  return "Sleepy zz";
        case PetMood::Sick:    return "Sick   xx";
        default:               return "Stable  .";
    }
}

// アドバイザーのメッセージ。stat の閾値で優先順位を決めて1つ返す。
// pet.cpp::calcMood() のしきい値と概念的に重複しているので、片方だけ
// 編集すると整合が崩れる点に注意 (例: hunger<20 を変えるなら両方触る)。
static const char* advisorMsg(const Pet& pet) {
    if (pet.health    < 30) return "Hull breach! Help me!";
    if (pet.hunger    < 20) return "Fuel critical. Feed me!";
    if (pet.hunger    < 40) return "Running low on fuel...";
    if (pet.happiness < 25) return "Crew morale very low.";
    if (pet.happiness < 50) return "Need recreation time.";
    if (pet.health    < 50) return "Shields at half.";
    if (pet.hunger > 80 && pet.happiness > 80 && pet.health > 80)
        return "All systems nominal!";
    if (pet.happiness > 70) return "Happy and exploring!";
    return "Systems stable.";
}

// ─── Act 画面のコンテンツ部分 (背景には触れない) ───────────────────────
// 左パネル (アクション一覧) と右パネル (Fido tips) を分割描画。
// 行ハイライトは selected のみ SM_SEL 色 + 左 3px のアクセントバー。
// アクション項目を追加する時は ACT_LABELS と main.cpp::doAct の case を
// 同じインデックスで更新すること。配列要素数 4 はループの `% 4` にも依存。
static const char* ACT_LABELS[] = { "Feed", "Play", "Game", "Shop" };

void displayActContent(int sel) {
    Serial.printf("[ACT] selected: %s\n", ACT_LABELS[sel]);
    // Left panel: action list
    M5.Lcd.fillRect(0, 0, PDIV, MSG_Y, SM_BG);
    M5.Lcd.fillRect(0, 0, PDIV, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(SX, 4);
    M5.Lcd.print("Actions");

    for (int i = 0; i < 4; i++) {
        int y = 22 + i * 33;
        bool active = (i == sel);
        uint16_t bg = active ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(0, y, PDIV, 33, bg);
        if (active) M5.Lcd.fillRect(0, y, 3, 33, SM_WHITE);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(active ? SM_WHITE : SM_GREY, bg);
        M5.Lcd.setCursor(SX + 4, y + 8);
        M5.Lcd.printf("%s %s", active ? ">" : " ", ACT_LABELS[i]);
    }
    spCornerFrame(0, 0, PDIV, MSG_Y);

    // Right panel: Fido tips
    int RX = PDIV + 8;
    int RW = 320 - PDIV;
    M5.Lcd.fillRect(PDIV, 0, RW, MSG_Y, SM_BG);
    M5.Lcd.fillRect(PDIV, 0, RW, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(RX, 7);
    M5.Lcd.print("FIDO FACTS");

    uint8_t tip = (millis() / 10000) % 10;
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(RX, 30);
    M5.Lcd.printf("tip %d/10", tip + 1);

    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(RX, 50);
    M5.Lcd.print(TIPS_L1[tip]);
    M5.Lcd.setCursor(RX, 62);
    M5.Lcd.print(TIPS_L2[tip]);

    M5.Lcd.drawFastHLine(PDIV + 4, 80, RW - 8, SM_DIV);

    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(RX, 90);
    M5.Lcd.print("Blackhole Cat Fido");
    M5.Lcd.setCursor(RX, 102);
    M5.Lcd.print("Ecology Database");
    M5.Lcd.setCursor(RX, 114);
    M5.Lcd.print("v2.4.1  classified");

    spCornerFrame(PDIV, 0, RW, MSG_Y);
}

// ─── Back 画面 (ステータス全表示) ──────────────────────────────────────
// 上から: ヘッダ (名前/ステージ/Day) → 絆 + コイン → ステータスバー3本 →
//        Mood + アイテム数 → Cargo (アイテム名) → Advisor → Space (NASA).
// 30秒 tick ごとに main.cpp から再描画される (背景非破壊)。
// 行 y 座標を変える時は MENU_Y=218 を超えないように注意。
void displayBackContent(const Pet& pet, const Inventory& inv, const NasaCargo& nasa) {
    int owned = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (inv.owned[i]) owned++;
    Serial.printf("[BACK] %s [%s] Day%d | Energy:%d Morale:%d Shield:%d | Mood:%s | Bond:%d/1000 $%d Items:%d/%d | %s\n",
        pet.name.c_str(), stageName(pet.age), pet.age,
        pet.hunger, pet.happiness, pet.health,
        moodLabel(pet.mood), inv.bond, inv.coins, owned, ITEM_COUNT,
        advisorMsg(pet));
    M5.Lcd.fillRect(0, 0, 320, MENU_Y, SM_BG);

    // Header
    M5.Lcd.fillRect(0, 0, 320, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(SX, 4);
    M5.Lcd.printf("%-5s [%s] Day%-3d", pet.name.c_str(), stageName(pet.age), pet.age);

    // Bond + coins
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 28);
    M5.Lcd.print("Bond: ");
    int stars = min(5, (int)(inv.bond / 200));
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    for (int i = 0; i < 5; i++) M5.Lcd.print(i < stars ? "*" : "-");
    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.printf(" (%d/1000)", inv.bond);
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(240, 28);
    M5.Lcd.printf("$%d", inv.coins);
    M5.Lcd.drawFastHLine(0, 40, 320, SM_DIV);

    // Stat bars
    struct { const char* lbl; uint8_t val; } stats[3] = {
        {"Energy", pet.hunger},
        {"Morale", pet.happiness},
        {"Shield", pet.health},
    };
    for (int i = 0; i < 3; i++) {
        int y = 46 + i * 22;
        bool crit = stats[i].val < 30;
        M5.Lcd.setTextColor(crit ? SM_WHITE : SM_GREY, SM_BG);
        M5.Lcd.setCursor(SX, y);
        M5.Lcd.printf("%-7s%s", stats[i].lbl, crit ? "!" : " ");
        spBar(70, y, 200, 8, stats[i].val);
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
        M5.Lcd.setCursor(276, y);
        M5.Lcd.printf("%3d%%", stats[i].val);
    }
    M5.Lcd.drawFastHLine(0, 114, 320, SM_DIV);

    // Mood + item count
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(SX, 120);
    M5.Lcd.print(moodLabel(pet.mood));
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(150, 120);
    M5.Lcd.printf("Items: %d/%d", owned, ITEM_COUNT);
    M5.Lcd.drawFastHLine(0, 132, 320, SM_DIV);

    // Cargo
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 138);
    M5.Lcd.print("Cargo: ");
    int cx = SX + 42, cy = 138;
    bool any = false;
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!inv.owned[i]) continue;
        any = true;
        int w = strlen(ITEM_DEFS[i].name) * 6 + 4;
        if (cx + w > 314) { cx = SX + 42; cy += 10; }
        if (cy > 150) break;
        M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
        M5.Lcd.setCursor(cx, cy);
        M5.Lcd.print(ITEM_DEFS[i].name);
        cx += w;
    }
    if (!any) {
        M5.Lcd.setTextColor(SM_DIM, SM_BG);
        M5.Lcd.setCursor(SX + 42, 138);
        M5.Lcd.print("--");
    }
    M5.Lcd.drawFastHLine(0, 160, 320, SM_DIV);

    // Advisor
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 166);
    M5.Lcd.print("Advisor: ");
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.print(advisorMsg(pet));

    // NASA gacha cargo
    M5.Lcd.drawFastHLine(0, 178, 320, SM_DIV);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(SX, 184);
    if (nasa.count == 0) {
        M5.Lcd.print("Space: --");
    } else {
        M5.Lcd.printf("Space:(%u)", nasa.count);
        // Show last 2 titles, one per line (y=194, 204)
        int show = min((int)nasa.count, 2);
        int base = (int)nasa.count - show;
        for (int i = 0; i < show; i++) {
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
            M5.Lcd.setCursor(SX + 4, 194 + i * 10);
            M5.Lcd.print(nasa.items[base + i]);
        }
    }

    spCornerFrame(0, 0, 320, MENU_Y);
}

// ─── 画面下のメッセージ BOX ────────────────────────────────────────────
// Main / Act 画面の y=155..217 を独占する横長エリア。
// 一時的な通知用なので、Back 画面では使わない (上書きするとステータスが消える)。
void displayMessage(const String& msg) {
    Serial.printf("[MSG] %s\n", msg.c_str());
    M5.Lcd.fillRect(1, MSG_Y + 1, 318, MSG_H - 2, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(6, MSG_Y + 8);
    M5.Lcd.print("> ");
    M5.Lcd.println(msg);
    spCornerFrame(0, MSG_Y, 320, MSG_H);
}

// ─── 最下段ボタンガイド ────────────────────────────────────────────────
// 各画面の A/B/C ボタン用ラベルを描画。アクティブな機能 (B 中心) は
// SM_WHITE、無効/未割当は SM_DIM で表現することで誤操作を減らしている。
// sel は将来的に画面ごとに動的なラベルを出す余地として残してある (現状未使用)。
void displayMenuBar(UIMode mode, int /*sel*/) {
    M5.Lcd.fillRect(0, MENU_Y, 320, MENU_H, SM_BG);
    M5.Lcd.drawFastHLine(0, MENU_Y, 320, SM_DIV);
    M5.Lcd.setTextSize(1);

    switch (mode) {
        case UIMode::Main:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Gacha");
            M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] Talk");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Act");
            break;
        case UIMode::Act:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Move");
            M5.Lcd.setTextColor(SM_WHITE, SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] OK");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Back");
            break;
        case UIMode::Back:
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(10,  MENU_Y + 7); M5.Lcd.print("[A] Config");
            M5.Lcd.setTextColor(SM_DIM,   SM_BG); M5.Lcd.setCursor(115, MENU_Y + 7); M5.Lcd.print("[B] ---");
            M5.Lcd.setTextColor(SM_LIGHT, SM_BG); M5.Lcd.setCursor(225, MENU_Y + 7); M5.Lcd.print("[C] Main");
            break;
    }
}

// ─── フル初期化 (背景 + 全コンテンツ) ──────────────────────────────────
// 重い処理 (背景グラデ + 星空 + 各画面のコンテンツ)。
// 画面遷移直後やサブ画面 (ショップ等) から戻った時にだけ呼ぶこと。
// Main 画面ではキャラスプライトはここでは描画されないので、続く
// charAnimUpdate() で描画される (キャラを別レイヤー扱いするための分離)。
void displayInit(UIMode mode, const Pet& pet, const Inventory& inv, const NasaCargo& nasa, int sel) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);

    switch (mode) {
        case UIMode::Main:
            displayMessage("");
            break;
        case UIMode::Act:
            displayActContent(sel);
            displayMessage("");
            break;
        case UIMode::Back:
            displayBackContent(pet, inv, nasa);
            break;
    }
    displayMenuBar(mode, sel);
}
