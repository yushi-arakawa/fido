#include <M5Stack.h>
#include "remote.h"
#include "pet.h"
#include "game_data.h"

// 遠隔 (シリアル) 経由の押下フラグ。remoteSync() が毎回クリア → 再構築するため
// 寿命は 1 イテレーションのみ。詳細は remote.h のコメントを参照。
static bool pendA = false, pendB = false, pendC = false;

// ─── デバッグコマンドの行バッファ ───────────────────────────────────────
// a/b/c 以外の文字は改行が来るまでここに溜め、1 行 = 1 コマンドとして解釈する。
// 解釈結果は dbgField/dbgValue (値設定) と pendAction (アクション) に保留され、
// それぞれ remoteDebugApply() / remotePopAction() で消費される。
static const uint8_t DBG_LINE_MAX = 12;
static char    lineBuf[DBG_LINE_MAX];
static uint8_t lineLen  = 0;
static char    dbgField = 0;  // 適用待ちフィールド ('h'/'p'/'s'/'g'/'m'/'k')、0=保留なし
static int     dbgValue = 0;  // 適用待ち数値 (クランプ前)
static RemoteAction pendAction = RemoteAction::None; // 適用待ちアクション

// lineBuf[0..lineLen) が文字列 w と完全一致するか。
static bool lineEquals(const char* w) {
    uint8_t n = 0;
    while (w[n]) n++;
    if (n != lineLen) return false;
    for (uint8_t i = 0; i < n; i++) if (lineBuf[i] != w[i]) return false;
    return true;
}

// lineBuf[from..lineLen) が 1 文字以上の数字列かどうか。
static bool lineDigitsFrom(uint8_t from) {
    if (lineLen <= from) return false;
    for (uint8_t i = from; i < lineLen; i++)
        if (lineBuf[i] < '0' || lineBuf[i] > '9') return false;
    return true;
}

// 完成した 1 行を解釈する。
//   (1) 値設定: フィールド文字 (h/p/s/g/m/k) + 数字列 → dbgField/dbgValue
//   (2) アクション: 既知の語句 → pendAction
// どちらにも当てはまらない行は黙って捨てる。
static void parseDebugLine() {
    if (lineLen == 0) return;

    // (1) 値設定コマンド
    char field = lineBuf[0];
    bool isField = (field == 'h' || field == 'p' || field == 's' ||
                    field == 'g' || field == 'm' || field == 'k');
    if (isField && lineDigitsFrom(1)) {
        int val = 0;
        for (uint8_t i = 1; i < lineLen; i++) {
            val = val * 10 + (lineBuf[i] - '0');
            if (val > 99999) val = 99999;     // 桁あふれ防止 (適用時に再クランプ)
        }
        dbgField = field;
        dbgValue = val;
        return;
    }

    // (2) アクションコマンド (語句)
    if      (lineEquals("night")) pendAction = RemoteAction::ForceNight;
    else if (lineEquals("sun"))   pendAction = RemoteAction::ForceDay;
    else if (lineEquals("free"))  pendAction = RemoteAction::AutoCycle;
    else if (lineEquals("event")) pendAction = RemoteAction::SpaceEvent;
    else if (lineEquals("die"))   pendAction = RemoteAction::Die;
}

void remoteSync() {
    pendA = pendB = pendC = false; // 前イテレーションの未消費分を破棄
    while (Serial.available() > 0) {
        char ch = Serial.read();
        switch (ch) {
            // a/b/c は 1 文字で即時にボタン押下。書きかけのデバッグ行は破棄する。
            case 'a': case 'A': pendA = true; lineLen = 0; break;
            case 'b': case 'B': pendB = true; lineLen = 0; break;
            case 'c': case 'C': pendC = true; lineLen = 0; break;
            // 改行でデバッグ行を確定 ('\n'/'\r' どちらでも)。
            case '\n': case '\r': parseDebugLine(); lineLen = 0; break;
            // それ以外はデバッグ行として蓄積 (溢れたら捨てる)。
            default: if (lineLen < DBG_LINE_MAX) lineBuf[lineLen++] = ch; break;
        }
    }
}

bool btnA() { return M5.BtnA.wasPressed() || pendA; }
bool btnB() { return M5.BtnB.wasPressed() || pendB; }
bool btnC() { return M5.BtnC.wasPressed() || pendC; }

bool remoteDebugApply(Pet& pet, Inventory& inv) {
    if (dbgField == 0) return false;       // 保留なし
    char field = dbgField;
    int  val   = dbgValue;
    dbgField = 0;                          // 消費 (二重適用を防ぐ)

    int clamped;
    switch (field) {
        case 'h': clamped = constrain(val, 0, 100);  pet.hunger    = clamped; break;
        case 'p': clamped = constrain(val, 0, 100);  pet.happiness = clamped; break;
        case 's': clamped = constrain(val, 0, 100);  pet.health    = clamped; break;
        case 'g': clamped = constrain(val, 0, 255);  pet.age       = clamped; break;
        case 'm': clamped = constrain(val, 0, 9999); inv.coins     = clamped; break;
        case 'k': clamped = constrain(val, 0, 1000); inv.bond      = clamped; break;
        default:  return false;
    }
    pet.mood = pet.calcMood();             // ステータス変更を mood に反映
    Serial.printf("[DEBUG] %c set to %d\n", field, clamped);
    return true;
}

RemoteAction remotePopAction() {
    RemoteAction a = pendAction;
    pendAction = RemoteAction::None;       // 消費 (二重適用を防ぐ)
    return a;
}
