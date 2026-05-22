#pragma once
#include <Arduino.h>
#include "pet.h"
#include "game_data.h"

// ─── 世界シミュレーション (昼夜サイクル + ランダム宇宙イベント) ──────────
// M5Stack Basic は RTC 非搭載のため、昼夜は実時刻ではなく millis ベースの
// 加速サイクルで表現する (既存の「Day = tick数」という圧縮時間に合わせる)。
// 位相は環境演出なので NVS には保存しない (電源 OFF で昼から再開する)。

// 現在が夜サイクルか。約12分 (昼8分 + 夜4分) で1日が一巡する。
// 夜は Pet::tick() の燃料消費が半減し、キャラが就寝表現になる。
// worldSetOverride() で固定された場合はそちらが優先される。
bool worldIsNight();

// ─── 昼夜の手動オーバーライド (PC 遠隔操作によるデバッグ/検証用) ──────────
// Auto = 通常の millis サイクル。ForceDay/ForceNight で固定。
// remote.cpp の "sun"/"night"/"free" コマンドから main.cpp 経由で呼ばれる。
enum class DayNightOverride { Auto, ForceDay, ForceNight };
void worldSetOverride(DayNightOverride o);

// tick 毎に呼ぶランダム宇宙イベント。WORLD_EVENT_CHANCE% で何かが起こり、
// pet/inv に効果を適用して outMsg に通知文をセットし true を返す。
// 卵 (STAGE_EGG) 期はまだ世界と関わらないので必ず false。
// 効果適用後に pet.mood を再計算するので呼び出し側での再計算は不要。
bool worldRollEvent(Pet& pet, Inventory& inv, String& outMsg);

// 確率ゲートと卵チェックを無視して必ず1つイベントを発生させる (検証用)。
// remote.cpp の "event" コマンドから呼ばれる。常に true。
bool worldForceEvent(Pet& pet, Inventory& inv, String& outMsg);
