#include <M5Stack.h>
#include "remote.h"
#include "pet.h"

// 遠隔 (シリアル) 経由の押下フラグ。remoteSync() が毎回クリア → 再構築するため
// 寿命は 1 イテレーションのみ。詳細は remote.h のコメントを参照。
static bool pendA = false, pendB = false, pendC = false;

// ─── デバッグコマンドの行バッファ ───────────────────────────────────────
// a/b/c 以外の文字は改行が来るまでここに溜め、1 行 = 1 コマンドとして解釈する。
// 解釈結果は dbgField/dbgValue に保留され、remoteDebugApply() で消費される。
static const uint8_t DBG_LINE_MAX = 8;
static char    lineBuf[DBG_LINE_MAX];
static uint8_t lineLen  = 0;
static char    dbgField = 0;  // 適用待ちフィールド ('h'/'p'/'s'/'g')、0=保留なし
static int     dbgValue = 0;  // 適用待ち数値 (クランプ前)

// 完成した 1 行を解釈し、有効なデバッグコマンドなら dbgField/dbgValue に積む。
// 形式は「フィールド文字 1 つ + 数字 1 桁以上」。それ以外の行は黙って捨てる。
static void parseDebugLine() {
    if (lineLen < 2) return;  // フィールド文字 + 数字で最低 2 文字必要
    char field = lineBuf[0];
    if (field != 'h' && field != 'p' && field != 's' && field != 'g') return;
    int  val = 0;
    for (uint8_t i = 1; i < lineLen; i++) {
        char ch = lineBuf[i];
        if (ch < '0' || ch > '9') return;     // 数字以外が混じる行は無効
        val = val * 10 + (ch - '0');
        if (val > 9999) val = 9999;           // 桁あふれ防止 (適用時に再クランプ)
    }
    dbgField = field;
    dbgValue = val;
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

bool remoteDebugApply(Pet& pet) {
    if (dbgField == 0) return false;       // 保留なし
    char field = dbgField;
    int  val   = dbgValue;
    dbgField = 0;                          // 消費 (二重適用を防ぐ)

    int clamped;
    switch (field) {
        case 'h': clamped = constrain(val, 0, 100); pet.hunger    = clamped; break;
        case 'p': clamped = constrain(val, 0, 100); pet.happiness = clamped; break;
        case 's': clamped = constrain(val, 0, 100); pet.health    = clamped; break;
        case 'g': clamped = constrain(val, 0, 255); pet.age       = clamped; break;
        default:  return false;
    }
    pet.mood = pet.calcMood();             // ステータス変更を mood に反映
    Serial.printf("[DEBUG] %c set to %d\n", field, clamped);
    return true;
}
