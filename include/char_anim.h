#pragma once
#include <Arduino.h>

// ペットのスプライトを (cx, cy) を中心に描画/アイドル動作させる。
// Main 画面の loop() 内で毎フレーム呼ぶ。Act/Back では呼ばないこと
// (キャラが他の UI を上書きしてしまうため)。
//
// 内部に差分検出キャッシュがあり、bobY が変わらないフレームはスキップする。
// ステージ進化を検出すると進化エフェクト (ブロッキング、~2秒) を再生する。
void charAnimUpdate(uint8_t age, int cx, int cy);

// 次回 charAnimUpdate() で必ず描画させるためのキャッシュリセット。
// fullRedraw() の直前に呼ぶ (背景クリアでスプライトが消えてもキャッシュは
// 「描画済み」のままになるので強制更新が要るため)。
void charAnimRedraw();

// 起動ロゴ (FIDO のアルペジオ表示)。setup() で1回だけ呼ぶ。
// ブロッキングで ~2秒。終了後に呼び出し元が fullRedraw() で上書きする想定。
void charAnimPlayStartup();
