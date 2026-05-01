#pragma once
#include <Arduino.h>

// カーゴの収容上限。Back 画面 / NVS キー数に直結するので軽率に増やさない。
// 増やす場合は saveNasaCargo / loadNasaCargo のキー命名 ("nc%d") の桁も確認。
#define NASA_CARGO_MAX 8

// NASA APOD ガチャで集めたタイトルのインベントリ。
// 47 文字制限は Back 画面の幅 (text size 1 で 1行に収まる長さ) に基づく。
// 表示時はさらに 320px 幅で折り返される可能性あり。
struct NasaCargo {
    char    items[NASA_CARGO_MAX][48]; // 各 47 文字 + null 終端
    uint8_t count;                     // 現在の登録件数 (0..NASA_CARGO_MAX)
};

// WiFi 接続 → NASA APOD API へリクエスト → タイトル抽出 → cargo に追加。
// 戻り値:
//   - "" (空文字)         : WiFi 失敗 / API エラー / タイトル取得失敗
//   - "Cargo full!"       : 既に NASA_CARGO_MAX 件埋まっている (新規追加なし)
//   - その他 (短縮タイトル) : 成功 (cargo は更新済み、呼び出し側で saveNasaCargo すること)
//
// 注意: 数秒間ブロッキングする。呼ぶ前に M5.Speaker.mute() で音を止めないと
// tone() の鳴動が伸びる (M5 ライブラリの仕様)。
String runNasaGacha(NasaCargo& cargo);

// NVS namespace "fido" の "nc" + "nc0".."nc7" に保存/復元。
void saveNasaCargo(const NasaCargo& cargo);
void loadNasaCargo(NasaCargo& cargo);
