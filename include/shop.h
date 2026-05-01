#pragma once
#include "pet.h"
#include "game_data.h"

// フルスクリーンのショップ画面。C で戻るまでブロックする。
// 購入時に pet (アイテム効果適用) と inv (coins 減算 / owned[] セット) を
// その場で書き換えるので、呼び出し側は処理後の値で UI を再描画すること。
// 内部で saveAll() を呼んで NVS にも反映済み。
void runShop(Pet& pet, Inventory& inv);
