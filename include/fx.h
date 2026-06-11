#pragma once
#include <Arduino.h>
#include "world.h"

// ─── 演出 (プレゼンテーション FX) API ──────────────────────────────────
// ゲームロジックには一切触れない「見せ方」専用モジュール。
//   - バックライト調光: 昼夜で画面全体の明るさを変える (夜は薄暗い)
//   - 環境演出:        Main 画面の瞬く星 + たまに流れる流れ星 (非ブロッキング)
//   - イベント演出:    宇宙イベント発生時の種別別ミニアニメ (ブロッキング ~0.6s)

// ─── バックライト調光 ──────────────────────────────────────────────────
// M5Stack のバックライトは PWM (0-255、ライブラリ既定 80)。
// 昼 100 / 夜 45 を本作の基準とし、遷移は fxRampTo() で滑らかに変える。
uint8_t fxPhaseBrightness(bool night);   // 昼夜サイクルに応じた基準輝度
void    fxInitBrightness(bool night);    // setup() 用: ランプなしで即設定
void    fxDayNightRamp(bool night);      // 昼夜切替時: 基準輝度へ滑らかに遷移
void    fxRampTo(uint8_t target);        // 任意輝度へのランプ (追悼フェード等)

// ─── 環境演出 (Main 画面のみ・非ブロッキング) ──────────────────────────
// fxAmbientUpdate() を Main 画面のフレームループから毎回呼ぶ。
//   - 瞬く星: 固定8座標の星が明滅し、最大光度で十字のきらめきを出す
//   - 流れ星: 右上の安全地帯 (キャラ/月/デブリと非干渉) をたまに流れる
// fxAmbientReset() は fullRedraw 直後に呼んで描画キャッシュを無効化する。
void fxAmbientReset();
void fxAmbientUpdate(bool night);

// ─── 宇宙イベントのミニアニメ (Main 画面のみ・ブロッキング) ─────────────
// 種別ごとに ~0.6 秒の演出 + 効果音を再生する。終了時に背景を復元し
// charAnimRedraw() を呼ぶので、キャラは次フレームで自動復帰する。
// 月/惑星/デブリ等の装飾は消えるため、呼び出し側で displayMainDeco() を
// 呼んで復元すること。
void fxEvent(WorldEventType t);
