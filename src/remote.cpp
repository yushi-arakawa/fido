#include <M5Stack.h>
#include "remote.h"

// 遠隔 (シリアル) 経由の押下フラグ。remoteSync() が毎回クリア → 再構築するため
// 寿命は 1 イテレーションのみ。詳細は remote.h のコメントを参照。
static bool pendA = false, pendB = false, pendC = false;

void remoteSync() {
    pendA = pendB = pendC = false; // 前イテレーションの未消費分を破棄
    while (Serial.available() > 0) {
        switch (Serial.read()) {
            case 'a': case 'A': pendA = true; break;
            case 'b': case 'B': pendB = true; break;
            case 'c': case 'C': pendC = true; break;
            default: break; // 改行 ('\n'/'\r')・空白など制御文字は無視
        }
    }
}

bool btnA() { return M5.BtnA.wasPressed() || pendA; }
bool btnB() { return M5.BtnB.wasPressed() || pendB; }
bool btnC() { return M5.BtnC.wasPressed() || pendC; }
