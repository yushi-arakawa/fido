#pragma once
#include "pet.h"
#include "game_data.h"

// 設定画面 (音量切替 / 電源 OFF / データ全削除)。
// C ボタンで閉じるまでブロックする。
// pet/inv を引数で受けているのは将来的に「セーブ強制書き込み」みたいな
// 項目を追加する余地を残しているため (現状は内部では使われていない)。
void showSettings(Pet& pet, Inventory& inv);

// Yes/No 確認ダイアログ。A=実行, C=キャンセル。
// confirmLabel には "[A] Confirm" のように A ボタンの動作を表示する。
// line2 が NULL or 空文字なら 1 行だけ表示。
bool showConfirmDialog(const char* line1, const char* line2, const char* confirmLabel);
