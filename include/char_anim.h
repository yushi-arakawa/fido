#pragma once
#include <Arduino.h>

// ペットのスプライトを (cx, cy) を中心に描画/アイドル動作させる。
// Main 画面の loop() 内で毎フレーム呼ぶ。Act/Back では呼ばないこと
// (キャラが他の UI を上書きしてしまうため)。
//
// 内部に差分検出キャッシュがあり、bobY が変わらないフレームはスキップする。
// ステージ進化を検出すると進化エフェクト (ブロッキング、~2秒) を再生する。
//
// sleeping=true (夜サイクル) のときは浮遊を穏やかにして頭上に "z z z" を出す。
// z はスプライトの 128x128 描画ボックス内に描くので、bob 差分更新時の
// eraseChar で一緒に消える (ゴーストしない)。
void charAnimUpdate(uint8_t age, int cx, int cy, bool sleeping = false);

// 次回 charAnimUpdate() で必ず描画させるためのキャッシュリセット。
// fullRedraw() の直前に呼ぶ (背景クリアでスプライトが消えてもキャッシュは
// 「描画済み」のままになるので強制更新が要るため)。
void charAnimRedraw();

// スプライト状態を完全リセットする (転生時専用)。currentStage を未初期化に
// 戻すので、次回 charAnimUpdate() は進化エフェクトなしで静かに再ロードする。
// (Elder→Egg の「逆進化演出」を防ぐため。charAnimRedraw() は currentStage を
//  保持するので進化検出を壊さないが、こちらは意図的に捨てる。)
void charAnimReset();

// 起動ロゴ (FIDO のアルペジオ表示)。setup() で1回だけ呼ぶ。
// ブロッキングで ~2秒。終了後に呼び出し元が fullRedraw() で上書きする想定。
void charAnimPlayStartup();
