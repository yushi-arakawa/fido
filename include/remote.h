#pragma once
// ─── PC からのシリアル遠隔操作 ──────────────────────────────────────────
// USB 接続中、PC から USB シリアル (115200 baud) に 'a' / 'b' / 'c' を
// 送ると、物理ボタン A / B / C を 1 回押したのと同じイベントが発火する。
// 大文字 'A'/'B'/'C' も同じ扱い。
//
// 使い方 (入力を取るループ全てで共通):
//   1. M5.update() の直後に remoteSync() を呼ぶ
//   2. M5.BtnX.wasPressed() を btnX() に置き換える
//
// remoteSync() は呼ぶたびに前回分の押下フラグを破棄してから
// シリアルバッファを読み直す。よってフラグの寿命は「次の remoteSync()
// まで」の 1 イテレーションのみ。現在の画面に対応しないコマンドは
// そのイテレーションで自然に捨てられ、古い入力が残らない。
//
// ─── デバッグコマンド (ステータス直接書き換え) ──────────────────────────
// a/b/c 以外に「<フィールド文字><数値><改行>」形式の行を送ると、
// 該当ステータスをその値に設定できる (デバッグ用)。
//   h<n>  hunger    0-100   例: "h50\n"
//   p<n>  happiness 0-100
//   s<n>  health    0-100
//   g<n>  age       0-255
// a/b/c は 1 文字で即時に処理 (改行不要)、デバッグコマンドは改行で確定。
// remoteSync() が行を解析して保留し、remoteDebugApply() が pet に反映する。

struct Pet; // pet.h — remoteDebugApply() の引数。実体は remote.cpp で include。

void remoteSync(); // シリアルを読み、A/B/C 押下フラグとデバッグ行を更新する
bool btnA();       // 物理ボタン A or 遠隔 'a'
bool btnB();       // 物理ボタン B or 遠隔 'b'
bool btnC();       // 物理ボタン C or 遠隔 'c'

// 保留中のデバッグコマンドを pet に適用する。適用したら true を返す
// (呼び出し側で saveAll / 再描画する想定)。保留なしなら false。
bool remoteDebugApply(Pet& pet);
