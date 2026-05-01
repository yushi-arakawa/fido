#pragma once
#include <Arduino.h>
#include "pet.h"

// ペットの返答を取得する。
// 現状の実装は内蔵のセリフ集からランダムに1つ返すスタブ
// (mood が Hungry/Sick の時はそれぞれ専用の文章プールへ切り替わる)。
//
// 元々は Anthropic Claude API を呼ぶ予定だったが、応答遅延と
// API キー管理のコストを避けるためオフライン化した。
// 将来 Claude API を呼びたい場合は claude_client.cpp の中身を差し替える
// (ヘッダ I/F は変えずに済む設計にしてある)。
//
// userMessage は現状未使用。Claude API 化したときの入力として残してある。
// 戻り値: 失敗時は空文字 (現実装では失敗しない)。
String askClaude(const Pet& pet, const String& userMessage);
