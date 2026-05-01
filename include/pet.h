#pragma once
#include <Arduino.h>

// ペットの感情。calcMood() で hunger/happiness/health から算出される派生値。
// UIはこの値を使って文字列・アイコン・アドバイザー文を切り替える。
enum class PetMood { Happy, Neutral, Hungry, Sleepy, Sick };

// 永続化される本体ステータス。NVS namespace "fido" に保存される。
// ※ 値域や減衰率を変える場合は pet.cpp::tick() と
//   display.cpp::advisorMsg()/stageName() の閾値も合わせて更新すること。
struct Pet {
    String  name;       // 表示名 (現状 "Fido" 固定)
    uint8_t hunger;     // 0-100 空腹度 (100=満腹)。tick で -2/30s
    uint8_t happiness;  // 0-100 幸福度。tick で -1/30s
    uint8_t health;     // 0-100 健康度。hunger<20 のとき -1/30s
    uint8_t age;        // 経過 tick 数 (1tick=30s)。255 でカンスト
    PetMood mood;       // 上記から算出される派生値

    // 1tick (30秒) 進める。main.cpp の lastTick タイマーから呼ばれる。
    void tick();

    // hunger/happiness/health から mood を決定する純粋関数。
    PetMood calcMood() const;
};

// 成長ステージ。display と char_anim で参照する単一の真実 (single source of truth)。
// ※ char_anim/sprite_stages のスプライト枚数 (STAGE_COUNT=5) と一致させること。
enum PetStage {
    STAGE_EGG   = 0,  // age 0-3   (~2分)
    STAGE_CHILD = 1,  // age 4-39  (~2-19分)
    STAGE_TEEN  = 2,  // age 40-59 (~20-29分)
    STAGE_YOUNG = 3,  // age 60-79 (~30-39分)
    STAGE_ELDER = 4,  // age 80+   (~40分以降)
};

// age から成長ステージを返す。inline なので呼び出しコストはゼロ。
// display.cpp の stageName() と char_anim.cpp の sprite 切替が
// この関数を経由することで、しきい値のズレ事故を防いでいる。
inline PetStage stageForAge(uint8_t age) {
    if (age < 4)  return STAGE_EGG;
    if (age < 40) return STAGE_CHILD;
    if (age < 60) return STAGE_TEEN;
    if (age < 80) return STAGE_YOUNG;
    return STAGE_ELDER;
}
