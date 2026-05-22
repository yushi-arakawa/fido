#include "world.h"

// ─── 昼夜サイクル ──────────────────────────────────────────────────────
// millis() を CYCLE_MS で割った余りが DAY_MS 以上なら夜。
// millis() のオーバーフロー (~49日) は剰余演算なので継ぎ目で1サイクル
// だけ乱れるが、ゲーム体験上は無視できる。
static const unsigned long DAY_MS   = 8UL * 60 * 1000; // 昼 8分
static const unsigned long NIGHT_MS = 4UL * 60 * 1000; // 夜 4分
static const unsigned long CYCLE_MS = DAY_MS + NIGHT_MS;

// 手動オーバーライド (デバッグ用)。既定は Auto = 通常サイクル。
static DayNightOverride g_override = DayNightOverride::Auto;

void worldSetOverride(DayNightOverride o) { g_override = o; }

bool worldIsNight() {
    if (g_override == DayNightOverride::ForceNight) return true;
    if (g_override == DayNightOverride::ForceDay)   return false;
    return (millis() % CYCLE_MS) >= DAY_MS;
}

// ─── ランダム宇宙イベント ──────────────────────────────────────────────
// 毎 tick この確率で抽選。低めにして「たまに起きるご褒美/事件」の頻度感に。
// 15% なら平均 ~7tick (約3.5分) に1回。
static const int WORLD_EVENT_CHANCE = 15; // %

// 各イベントの重み (合計100)。前から累積で判定する。
//   Meteor shower  30 : コイン +5..15  (報酬)
//   Supply drone   25 : hunger +25     (補給)
//   Cosmic ray     20 : happiness +15  (高揚)
//   Solar flare    15 : health -5/-10  (被害, shield高いと軽減)
//   Wormhole       10 : age +3         (時間ジャンプ → 進化が早まる)
//
// applyRandomEvent: 重み付き抽選して効果を適用する本体。常に1つ起こして true。
// 確率ゲート/卵チェックは呼び出し側 (worldRollEvent) が持つので、ここには無い。
static bool applyRandomEvent(Pet& pet, Inventory& inv, String& outMsg) {
    int r = (int)random(100);
    if (r < 30) {
        uint16_t c = 5 + (uint16_t)random(11); // 5..15
        inv.coins += c;
        outMsg = "Meteor shower! +" + String(c) + " coins";
    } else if (r < 55) {
        pet.hunger = min(100, (int)pet.hunger + 25);
        outMsg = "Supply drone! +25 fuel";
    } else if (r < 75) {
        pet.happiness = min(100, (int)pet.happiness + 15);
        outMsg = "Cosmic ray surge! +15 morale";
    } else if (r < 90) {
        int dmg = pet.health > 60 ? 5 : 10; // shield が厚いと被害軽減
        pet.health = max(0, (int)pet.health - dmg);
        outMsg = "Solar flare! shields -" + String(dmg);
    } else {
        pet.age = min(255, (int)pet.age + 3);
        outMsg = "Wormhole! time jumps ahead";
    }

    pet.mood = pet.calcMood();
    return true;
}

// 通常版: 卵期は除外し、WORLD_EVENT_CHANCE% を引いた時だけ発生させる。
bool worldRollEvent(Pet& pet, Inventory& inv, String& outMsg) {
    if (stageForAge(pet.age) == STAGE_EGG) return false; // 卵は対象外
    if ((int)random(100) >= WORLD_EVENT_CHANCE) return false;
    return applyRandomEvent(pet, inv, outMsg);
}

// 検証版: 確率/卵ゲートを無視して必ず発生 (PC 遠隔の "event" コマンド用)。
bool worldForceEvent(Pet& pet, Inventory& inv, String& outMsg) {
    return applyRandomEvent(pet, inv, outMsg);
}
