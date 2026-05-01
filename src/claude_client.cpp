#include "claude_client.h"
#include <Arduino.h>

// オフライン版の "ask Claude"。mood に応じて返答プールを切り替えて
// ランダムに1セリフ返すだけのスタブ (claude_client.h のコメント参照)。
//
// セリフを増やす時はここに追加するだけで良い。配列サイズは sizeof で
// 自動計算しているので個数指定不要。
String askClaude(const Pet& pet, const String& /*userMessage*/) {
    // 通常時のセリフプール (Happy / Neutral / Sleepy のとき使う)
    static const char* lines[] = {
        "Woof! I'm feeling great today!",
        "Can I have a snack? Pleeeease?",
        "Let's play! Let's play! Let's play!",
        "I love you so much!",
        "Zzz... just a little nap...",
        "My tummy is making funny noises...",
        "You're the best human ever!",
        "I learned a new trick. Watch this!",
        "Everything is wonderful when you're here.",
        "Is it dinner time yet?",
    };
    // 空腹状態 (hunger < 20) 専用プール
    static const char* hungryLines[] = {
        "So... hungry... need food now...",
        "Feed me and I'll be your best friend!",
        "My bowl is empty. This is a crisis.",
    };
    // 不調状態 (health < 30) 専用プール
    static const char* sickLines[] = {
        "I don't feel so good...",
        "Maybe just rest for a while...",
        "A little under the weather today.",
    };

    // mood に基づいてプールを選択。default は通常プール。
    const char** pool  = lines;
    size_t      poolSize = sizeof(lines) / sizeof(lines[0]);

    if (pet.mood == PetMood::Hungry) {
        pool     = hungryLines;
        poolSize = sizeof(hungryLines) / sizeof(hungryLines[0]);
    } else if (pet.mood == PetMood::Sick) {
        pool     = sickLines;
        poolSize = sizeof(sickLines) / sizeof(sickLines[0]);
    }

    return pool[random(poolSize)];
}
