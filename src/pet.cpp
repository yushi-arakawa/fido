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

// デブリ生成確率 (%/tick)。昼は 16tick (8分) あるので 20% なら平均 ~3個/日。
// JUNK_MAX で打ち止めなので、1日掃除しないとほぼ確実に満杯になる感覚。
static const int JUNK_CHANCE = 20;

// 30秒ごとの自動減衰。main.cpp の loop() から TICK_MS 経過ごとに呼ばれる。
// 値の変化はその場で saveAll() により NVS に書き戻されることに注意 (game_data.cpp)。
void Pet::tick(bool night) {
    // age は 8bit カンスト (255)。stageForAge() は 80+ を Elder として扱うので
    // カンスト後はずっと Elder のまま動作し続ける。
    if (age < 255) age++;

    // 燃料消費。夜 (就寝中) は代謝が落ちる想定で半減 (-1)、昼は -2。
    // hungerDrain 未満なら 0 にクランプ (アンダーフロー = 255化を防ぐ。
    // アイテムで hunger が奇数になると旧 `if(>0) -=2` は 1→255 とバグり得た)。
    uint8_t hungerDrain = night ? 1 : 2;
    hunger = (hunger > hungerDrain) ? (hunger - hungerDrain) : 0;

    // 士気は昼のみ自然減衰。夜はぐっすり眠っているので減らさない。
    if (!night && happiness > 0) happiness -= 1;

    // 空腹が深刻なら体力も削れる仕様。連鎖的に Sick mood へ移行する仕掛け。
    if (hunger < 20)    health    = max(0, (int)health - 1);

    // デブリ放置ペナルティ: 1個につき士気 -1/tick を上乗せ、2個以上で体力も -1。
    // 夜は就寝中で気にならない想定 (士気据え置きの昼夜設計に合わせる) なので免除。
    if (!night && junk > 0) {
        happiness = max(0, (int)happiness - junk);
        if (junk >= 2) health = max(0, (int)health - 1);
    }

    // デブリ生成: 昼かつ卵以外で JUNK_CHANCE%/tick。生成は減衰の後に行うので、
    // 落ちたばかりのデブリが同 tick でペナルティを与えることはない。
    // 増えた事実は呼び出し側 (main.cpp) が tick 前後の junk 比較で検知して通知する。
    if (!night && stageForAge(age) != STAGE_EGG && junk < JUNK_MAX &&
        (int)random(100) < JUNK_CHANCE) {
        junk++;
    }

    mood = calcMood();

    // 夜かつ危機的でない (Happy/Neutral) なら「眠っている」表現に上書き。
    // Hungry/Sick は夜でも優先して見せる (放置に気付けるように)。
    if (night && (mood == PetMood::Happy || mood == PetMood::Neutral))
        mood = PetMood::Sleepy;
}
