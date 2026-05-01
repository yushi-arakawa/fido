#pragma once
#include <Arduino.h>
#include "pet.h"

// ITEM_COUNT を増やす場合は以下を全部更新する必要がある:
//   - game_data.cpp の ITEM_DEFS 配列の要素数
//   - Inventory.owned[14] の固定サイズ (下記コメント参照)
//   - shop.cpp / display.cpp の Cargo 表示ループ
static const int ITEM_COUNT = 14;

// アイテムの静的定義 (フラッシュ常駐の const)。NVS には保存しない。
struct ItemDef {
    const char* name;        // 表示名 (短い英語、~10文字)
    uint16_t    cost;        // 購入コスト (コイン)
    const char* desc;        // 効果説明 (ショップ画面に表示)
    int8_t      hungerBoost; // 購入時に hunger に加算
    int8_t      happyBoost;  // 購入時に happiness に加算
    int8_t      healthBoost; // 購入時に health に加算
};

extern const ItemDef ITEM_DEFS[ITEM_COUNT];

// プレイヤーが持つ可変データ。NVS namespace "fido" に保存される。
struct Inventory {
    uint16_t coins;       // 所持コイン (ミニゲーム報酬で増加)
    uint16_t bond;        // 絆レベル 0-1000 (200刻みで 5★ 評価)
    // owned[14]: 固定サイズで宣言 (ITEM_COUNT を増やしても旧 NVS が残るよう
    // キー "it0".."it13" を安定させるため)。新アイテム追加時はこの配列の
    // サイズも合わせて広げ、新規キー "it14".."" を追加すること。
    bool     owned[14];
};

// ─── 永続化 API ─────────────────────────────────────────────────────────
// すべて Preferences (NVS) を経由する。namespace は "fido" 固定。
// saveAll は tick ごとに呼ばれるので、NVS の摩耗を意識すること
// (ESP32 の NVS は wear-leveling 内蔵だが、TICK_MS を縮める前に検討)。
void saveAll(const Pet& pet, const Inventory& inv);
void loadAll(Pet& pet, Inventory& inv);

// アイテム購入直後にステータスを反映するヘルパー (shop.cpp が使用)。
void applyItem(Pet& pet, int idx);
