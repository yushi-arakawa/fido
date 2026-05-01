#include "pet.h"

// hunger/happiness/health から mood を決める優先順位ロジック。
// 上から順にチェックするので、Sick が最優先 → Hungry → Happy → Sleepy → 中立。
// しきい値 (30/20/70/30) を変えると advisorMsg() のメッセージとも整合させること。
PetMood Pet::calcMood() const {
    if (health < 30)    return PetMood::Sick;    // 体力危機
    if (hunger < 20)    return PetMood::Hungry;  // 空腹危機
    if (happiness > 70) return PetMood::Happy;   // 幸福ゾーン
    if (happiness < 30) return PetMood::Sleepy;  // 元気不足
    return PetMood::Neutral;
}

// 30秒ごとの自動減衰。main.cpp の loop() から TICK_MS 経過ごとに呼ばれる。
// 値の変化はその場で saveAll() により NVS に書き戻されることに注意 (game_data.cpp)。
void Pet::tick() {
    // age は 8bit カンスト (255)。stageForAge() は 80+ を Elder として扱うので
    // カンスト後はずっと Elder のまま動作し続ける。
    if (age < 255) age++;

    // 飽和減算 (アンダーフロー防止)
    if (hunger    > 0)  hunger    -= 2;
    if (happiness > 0)  happiness -= 1;

    // 空腹が深刻なら体力も削れる仕様。連鎖的に Sick mood へ移行する仕掛け。
    if (hunger < 20)    health    = max(0, (int)health - 1);

    mood = calcMood();
}
