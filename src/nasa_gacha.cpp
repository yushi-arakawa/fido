#include "nasa_gacha.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

#include "secrets.h"

// ─── 設定値 ─────────────────────────────────────────────────────────────
static const uint32_t WIFI_TIMEOUT_MS  = 10000; // WiFi 接続タイムアウト
static const int      STORED_TITLE_MAX = 47;    // NasaCargo.items 1要素分 (+null=48)
static const int      DISPLAY_TITLE_MAX = 39;   // メッセージBOX で "Discovery: <39文字>" がギリ
// NASA APOD は 1995-06-16 が初回。安全側で 2000-2025 の範囲を使う。
static const int DATE_YEAR_BASE  = 2000;
static const int DATE_YEAR_RANGE = 26;          // 2000..2025
static const int DATE_DAY_RANGE  = 28;          // 全月で安全な日 (うるう年問題回避)

// NASA APOD JSON から "title" の値を抜き出す軽量パーサ。
// 真の JSON パースは不要 (タイトルがエスケープ "\"" を含むケースは稀)。
// 例: ..."title":"Crab Nebula in Infrared",... → "Crab Nebula in Infrared"
static String extractTitle(const String& json) {
    int idx = json.indexOf("\"title\":");
    if (idx < 0) return "";
    int q = json.indexOf('"', idx + 8); // 値の開始引用符
    if (q < 0) return "";
    int end = json.indexOf('"', q + 1); // 値の終了引用符
    if (end < 0) return "";
    return json.substring(q + 1, end);
}

String runNasaGacha(NasaCargo& cargo) {
    if (cargo.count >= NASA_CARGO_MAX) return "Cargo full!";

    // ── WiFi 接続 (タイムアウト付き) ────────────────────────────────────
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TIMEOUT_MS) delay(200);

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return "";
    }

    // ── ランダムな日付を生成 (2000-01-01 ~ 2025-12-28) ──────────────────
    // 28日固定なのはうるう年や月末のばらつきを気にせず安全側に倒すため。
    char dateStr[12];
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
        DATE_YEAR_BASE + (int)random(DATE_YEAR_RANGE),
        1 + (int)random(12),
        1 + (int)random(DATE_DAY_RANGE));

    // ── HTTPS リクエスト ────────────────────────────────────────────────
    // setInsecure() で証明書検証をスキップ。エンタープライズ用途なら問題だが
    // 公開 API の照会だけなのでリスクは低いと判断。
    // useHTTP10(true) は HTTP/1.0 強制で chunked-encoding を回避し、
    // getString() がそのままレスポンスボディを取れるようにしている。
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.useHTTP10(true);
    String url = "https://api.nasa.gov/planetary/apod?api_key=DEMO_KEY&date=";
    url += dateStr;
    http.begin(client, url);

    // ── レスポンスのパースと cargo 追加 ─────────────────────────────────
    String title = "";
    if (http.GET() == HTTP_CODE_OK) {
        String body = http.getString();
        String fullTitle = extractTitle(body);
        if (fullTitle.length() > 0) {
            // 保存用 (Back 画面の1行に収まる長さに切り詰め)
            String stored = fullTitle.length() > STORED_TITLE_MAX
                          ? fullTitle.substring(0, STORED_TITLE_MAX) : fullTitle;
            stored.toCharArray(cargo.items[cargo.count], STORED_TITLE_MAX + 1);
            cargo.count++;
            // 表示用 ("Discovery: <タイトル>" の形でメッセージBOX に収まる長さ)
            title = fullTitle.length() > DISPLAY_TITLE_MAX
                  ? fullTitle.substring(0, DISPLAY_TITLE_MAX - 3) + "..." : fullTitle;
        }
    }

    // ── 後始末 (WiFi をオフにして電力節約) ──────────────────────────────
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return title;
}

// NVS 保存。ガチャ成功時にしか呼ばれないので、頻度は気にしなくて良い。
// キー命名: "nc"=件数, "nc0".."nc7"=各タイトル文字列。
void saveNasaCargo(const NasaCargo& cargo) {
    Preferences p;
    p.begin("fido", false);
    p.putUChar("nc", cargo.count);
    for (int i = 0; i < cargo.count; i++) {
        char key[5]; // "nc" + 1桁 + null = 4 だが余裕を持たせて 5
        snprintf(key, sizeof(key), "nc%d", i);
        p.putString(key, cargo.items[i]);
    }
    p.end();
}

// 起動時に1回だけ呼ばれる。クラッシュ時の不正値に備えて
// count を NASA_CARGO_MAX で頭打ちさせている。
void loadNasaCargo(NasaCargo& cargo) {
    Preferences p;
    p.begin("fido", true);
    cargo.count = p.getUChar("nc", 0);
    if (cargo.count > NASA_CARGO_MAX) cargo.count = NASA_CARGO_MAX;
    for (int i = 0; i < cargo.count; i++) {
        char key[5];
        snprintf(key, sizeof(key), "nc%d", i);
        String s = p.getString(key, "");
        s.toCharArray(cargo.items[i], STORED_TITLE_MAX + 1);
    }
    p.end();
}
