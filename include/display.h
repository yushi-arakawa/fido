#pragma once
#include "pet.h"
#include "game_data.h"
#include "nasa_gacha.h"

// メインの3画面モード。C ボタンで Main → Act → Back → Main と循環する。
enum class UIMode { Main, Act, Back };

// ─── 描画 API のレベル分けの方針 ────────────────────────────────────────
// "noise-safe" (背景に触れない) と "full-redraw" (背景込み) の2系統に分けて、
// ナビゲーション時のチラつきや星空の再生成を最小化している。
//   - 画面切替や全画面サブ画面から戻った時 → displayInit() を1回
//   - 画面内の選択移動・tick 更新 → displayActContent / displayBackContent
//   - 1行メッセージだけ更新したい時 → displayMessage()
//   - ボタン帯の文字だけ → displayMenuBar()

// 背景 (グラデ + 星空) も含めて全部描き直す。重いので画面遷移時のみ呼ぶ。
void displayInit(UIMode mode, const Pet& pet, const Inventory& inv, const NasaCargo& nasa, int sel);

// 背景には触れずコンテンツ部分だけ更新する軽量版。
// ナビゲーション中はこちらを使うと星空の再描画が省けてチラつかない。
void displayActContent(int sel);
void displayBackContent(const Pet& pet, const Inventory& inv, const NasaCargo& nasa);

// メッセージ BOX (画面下) だけを更新。Main / Act 画面で使う。
// Back 画面はメッセージ BOX が無い (ステータスで全領域を使う) ので呼ばないこと。
void displayMessage(const String& msg);

// ボタンガイド帯 (最下段) だけを再描画。アクション選択変更時などに呼ぶ。
void displayMenuBar(UIMode mode, int sel);
