#include "display.h"
#include "space_ui.h"
#include "nasa_gacha.h"
#include "world.h"
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
static const uint8_t TIP_COUNT = 10;
static const char* TIPS_L1[TIP_COUNT] = {
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
static const char* TIPS_L2[TIP_COUNT] = {
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
static uint8_t lastActTip = 255;

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
    if (pet.junk  >= 2)     return "Debris field! Clean up!";
    if (pet.hunger    < 40) return "Running low on fuel...";
    if (pet.happiness < 25) return "Crew morale very low.";
    if (pet.happiness < 50) return "Need recreation time.";
    if (pet.health    < 50) return "Shields at half.";
    if (pet.hunger > 80 && pet.happiness > 80 && pet.health > 80)
        return "All systems nominal!";
    if (pet.happiness > 70) return "Happy and exploring!";
    return "Systems stable.";
}

// ─── 夜サイクルの三日月インジケータ (Main 画面右上) ────────────────────
// キャラの描画ボックス (中心 160, 横幅128 → x 96..224) の外側 (右上) に置く
// ので、charAnimUpdate のスプライト再描画と干渉しない。
// 三日月は明るい円を背景色の円で削って作る。y<80 の背景は 0x0000 (黒) なので
// SM_BG で削れば継ぎ目が出ない。fullRedraw 時に再描画される (昼は描かない)。
static void drawNightMoon() {
    M5.Lcd.fillCircle(300, 20, 10, SM_LIGHT);
    M5.Lcd.fillCircle(305, 15,  9, SM_BG);
    M5.Lcd.drawPixel(296, 23, SM_GREY);  // クレーターの影
    M5.Lcd.drawPixel(293, 18, SM_GREY);
    M5.Lcd.drawPixel(286, 11, SM_DIM);   // 周囲に淡い光点 (ハロー)
    M5.Lcd.drawPixel(288, 29, SM_DIM);
    M5.Lcd.drawPixel(311, 31, SM_DIM);
}

// ─── 環状惑星 (Main 画面左上の常設デコ) ────────────────────────────────
// 遠景に浮かぶ小さな土星型の惑星。キャラボックス (x>=96) の外側なので
// アニメと干渉しない。環は点列の楕円 (rx=13, ry=4.5) を手描きし、本体の
// 「後ろ側」(上半分かつ本体の幅の内側) を描かないことで奥行きを出す。
static void drawRingedPlanet() {
    const int px = 26, py = 24;
    M5.Lcd.fillCircle(px, py, 7, SM_DIM);
    M5.Lcd.fillCircle(px - 2, py - 2, 3, SM_GREY); // 受光面ハイライト
    for (int a = 0; a < 360; a += 6) {
        float rad = a * 0.0174533f;
        int ex = px + (int)(13.0f * cosf(rad));
        int ey = py + (int)(4.5f * sinf(rad));
        if (ey > py || ex < px - 8 || ex > px + 8)
            M5.Lcd.drawPixel(ex, ey, SM_GREY);
    }
}

// Main 画面の装飾一式 (惑星 / 夜の三日月 / デブリ) をまとめて描く。
// displayInit(Main) のほか、イベント FX が背景を塗り戻した後の復元にも使う。
void displayMainDeco(uint8_t junk, bool night) {
    drawRingedPlanet();
    if (night) drawNightMoon();
    displayJunk(junk);
}

// ─── スペースデブリ (宇宙ゴミ) アイコン (Main 画面のみ) ────────────────
// たまごっちの「うんち」に相当するお世話対象。配置はキャラ描画ボックス
// (中心 160,80 / 128×128 + ボブ±3 → x 96..224, y 13..147) の完全に外側に
// 取ってあるので、charAnimUpdate の erase/再描画と干渉しない。
// 夜の三日月 (300,20 r10) とも重ならない位置にすること。
// 消去は専用処理を持たない: Clean もデバッグ j コマンドも fullRedraw 経由で
// 画面ごと描き直されるため、描く側だけ用意すれば足りる。
static const struct { int x, y; } JUNK_POS[JUNK_MAX] = {
    {  58, 116 },   // 左下 (キャラの足元寄り)
    { 256, 122 },   // 右下
    {  42,  60 },   // 左上 (3個目で「散らかってる」感を出す)
};

// ゴツゴツした破片の塊 (~15×10px) を図形プリミティブで描く。
// スプライトを増やすとフラッシュを食うので手描きで済ませている。
static void drawJunkIcon(int x, int y) {
    M5.Lcd.fillTriangle(x,     y + 8, x + 5,  y,     x + 11, y + 7, SM_GREY);
    M5.Lcd.fillTriangle(x + 2, y + 9, x + 11, y + 7, x + 7,  y + 3, SM_DIM);
    M5.Lcd.drawPixel(x + 4,  y + 2, SM_LIGHT); // ハイライト
    M5.Lcd.drawPixel(x + 14, y + 3, SM_DIM);   // 漂う小破片
    M5.Lcd.drawPixel(x - 3,  y + 6, SM_DIM);
}

void displayJunk(uint8_t junk) {
    for (int i = 0; i < junk && i < JUNK_MAX; i++)
        drawJunkIcon(JUNK_POS[i].x, JUNK_POS[i].y);
}

// ─── Act 画面のコンテンツ部分 (背景には触れない) ───────────────────────
// 左パネル (アクション一覧) と右パネル (Fido tips) を分割描画。
// 行ハイライトは selected のみ SM_SEL 色 + 左 3px のアクセントバー。
// アクション項目を追加する時は ACT_LABELS と main.cpp::doAct の case を
// 同じインデックスで更新すること。配列要素数 5 は main.cpp の `% 5` にも依存。
// 行高 26px × 5行 = 130px で 22..152 に収まる (MSG_Y=155 が下限)。
static const char* ACT_LABELS[] = { "Feed", "Play", "Clean", "Game", "Shop" };

static uint8_t currentTipIndex() {
    return (uint8_t)((millis() / 10000) % TIP_COUNT);
}

static void displayActTipsPanel(uint8_t tip) {
    int RX = PDIV + 8;
    int RW = 320 - PDIV;
    M5.Lcd.fillRect(PDIV, 0, RW, MSG_Y, SM_BG);
    M5.Lcd.fillRect(PDIV, 0, RW, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(RX, 7);
    M5.Lcd.print("FIDO FACTS");

    M5.Lcd.setTextColor(SM_DIM, SM_BG);
    M5.Lcd.setCursor(RX, 30);
    M5.Lcd.printf("tip %d/%d", tip + 1, TIP_COUNT);

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

    // パネル下部の空きにミニ星図 (星座 "Fido Minor") を描いて宇宙観を足す。
    // 点を SM_DIV の細線で結び、星本体は 2x2 + 中心 1px の白で小さく光らせる。
    static const struct { int16_t x, y; } CONST_PTS[6] = {
        {178, 144}, {198, 133}, {220, 139}, {243, 129}, {266, 145}, {291, 134},
    };
    for (int i = 0; i < 5; i++)
        M5.Lcd.drawLine(CONST_PTS[i].x, CONST_PTS[i].y,
                        CONST_PTS[i + 1].x, CONST_PTS[i + 1].y, SM_DIV);
    for (int i = 0; i < 6; i++) {
        M5.Lcd.fillRect(CONST_PTS[i].x - 1, CONST_PTS[i].y - 1, 3, 3, SM_GREY);
        M5.Lcd.drawPixel(CONST_PTS[i].x, CONST_PTS[i].y, SM_WHITE);
    }

    spCornerFrame(PDIV, 0, RW, MSG_Y);
    lastActTip = tip;
}

void displayActContent(int sel) {
    Serial.printf("[ACT] selected: %s\n", ACT_LABELS[sel]);
    // Left panel: action list
    M5.Lcd.fillRect(0, 0, PDIV, MSG_Y, SM_BG);
    M5.Lcd.fillRect(0, 0, PDIV, 22, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(SX, 4);
    M5.Lcd.print("Actions");

    for (int i = 0; i < 5; i++) {
        int y = 22 + i * 26;
        bool active = (i == sel);
        uint16_t bg = active ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(0, y, PDIV, 26, bg);
        if (active) M5.Lcd.fillRect(0, y, 3, 26, SM_WHITE);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(active ? SM_WHITE : SM_GREY, bg);
        M5.Lcd.setCursor(SX + 4, y + 5);
        M5.Lcd.printf("%s %s", active ? ">" : " ", ACT_LABELS[i]);
    }
    spCornerFrame(0, 0, PDIV, MSG_Y);

    displayActTipsPanel(currentTipIndex());
}

void displayActTipsMaybeUpdate() {
    uint8_t tip = currentTipIndex();
    if (tip != lastActTip) displayActTipsPanel(tip);
}

// ─── Back 画面 (ステータス全表示) ──────────────────────────────────────
// 上から: ヘッダ (名前/ステージ/Day) → 絆 + コイン → ステータスバー3本 →
//        Mood + アイテム数 → Cargo (アイテム名) → Advisor → Space (NASA).
// 30秒 tick ごとに main.cpp から再描画される (背景非破壊)。
// 行 y 座標を変える時は MENU_Y=218 を超えないように注意。
void displayBackContent(const Pet& pet, const Inventory& inv, const NasaCargo& nasa) {
    int owned = 0;
    for (int i = 0; i < ITEM_COUNT; i++) if (inv.owned[i]) owned++;
    Serial.printf("[BACK] %s [%s] Day%d | Energy:%d Morale:%d Shield:%d | Mood:%s Junk:%d | Bond:%d/1000 $%d Items:%d/%d | %s\n",
        pet.name.c_str(), stageName(pet.age), pet.age,
        pet.hunger, pet.happiness, pet.health,
        moodLabel(pet.mood), pet.junk, inv.bond, inv.coins, owned, ITEM_COUNT,
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

    // Mood + item count + debris
    M5.Lcd.setTextColor(SM_LIGHT, SM_BG);
    M5.Lcd.setCursor(SX, 120);
    M5.Lcd.print(moodLabel(pet.mood));
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setCursor(150, 120);
    M5.Lcd.printf("Items: %d/%d", owned, ITEM_COUNT);
    // デブリ数。2個以上 (体力が削れ始めるライン) は白で警告表示する。
    M5.Lcd.setTextColor(pet.junk >= 2 ? SM_WHITE : SM_GREY, SM_BG);
    M5.Lcd.setCursor(250, 120);
    M5.Lcd.printf("Debris:%d", pet.junk);
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
// 文字は端末風のタイプライター表示 (1文字ずつ送出)。長文 (Claude の返答等)
// は字送りを速めて体感待ちを抑える。ブロッキングだが最長でも ~0.5 秒。
void displayMessage(const String& msg) {
    Serial.printf("[MSG] %s\n", msg.c_str());
    M5.Lcd.fillRect(1, MSG_Y + 1, 318, MSG_H - 2, SM_HDR);
    spCornerFrame(0, MSG_Y, 320, MSG_H);
    if (msg.length() == 0) return; // 空メッセージは枠だけ描いて終了

    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR);
    M5.Lcd.setCursor(6, MSG_Y + 8);
    M5.Lcd.print("> ");
    uint8_t wait = msg.length() > 60 ? 2 : 6;
    for (unsigned int i = 0; i < msg.length(); i++) {
        M5.Lcd.print(msg[i]); // print の自動折返しは println と同じ挙動
        delay(wait);
    }
    spCornerFrame(0, MSG_Y, 320, MSG_H); // 折返しが左枠に触れた場合の補修
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
            displayMainDeco(pet.junk, worldIsNight()); // 惑星 + 夜の三日月 + デブリ
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
