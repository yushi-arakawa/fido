#include "nasa_gacha.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

#include "secrets.h"

// Extract "title" value from NASA APOD JSON without a full JSON parser.
static String extractTitle(const String& json) {
    int idx = json.indexOf("\"title\":");
    if (idx < 0) return "";
    int q = json.indexOf('"', idx + 8); // opening quote of value
    if (q < 0) return "";
    int end = json.indexOf('"', q + 1); // closing quote
    if (end < 0) return "";
    return json.substring(q + 1, end);
}

String runNasaGacha(NasaCargo& cargo) {
    if (cargo.count >= NASA_CARGO_MAX) return "Cargo full!";

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 10000) delay(200);

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return "";
    }

    // Random date in [2000-01-01, 2025-12-28] for variety
    randomSeed(esp_random());
    char dateStr[12];
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
        2000 + (int)random(26), 1 + (int)random(12), 1 + (int)random(28));

    WiFiClientSecure client;
    client.setInsecure(); // skip cert validation — it's a game, not banking
    HTTPClient http;
    http.useHTTP10(true);
    String url = "https://api.nasa.gov/planetary/apod?api_key=DEMO_KEY&date=";
    url += dateStr;
    http.begin(client, url);

    String title = "";
    if (http.GET() == HTTP_CODE_OK) {
        String body = http.getString();
        String fullTitle = extractTitle(body);
        if (fullTitle.length() > 0) {
            // Store truncated version (NVS / Back-screen space limit)
            String stored = fullTitle.length() > 23 ? fullTitle.substring(0, 23) : fullTitle;
            stored.toCharArray(cargo.items[cargo.count], 24);
            cargo.count++;
            // Return display-friendly version (fits "Discovery: <title>" in message box)
            title = fullTitle.length() > 39 ? fullTitle.substring(0, 36) + "..." : fullTitle;
        }
    }

    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return title;
}

void saveNasaCargo(const NasaCargo& cargo) {
    Preferences p;
    p.begin("fido", false);
    p.putUChar("nc", cargo.count);
    for (int i = 0; i < cargo.count; i++) {
        char key[5];
        snprintf(key, sizeof(key), "nc%d", i);
        p.putString(key, cargo.items[i]);
    }
    p.end();
}

void loadNasaCargo(NasaCargo& cargo) {
    Preferences p;
    p.begin("fido", true);
    cargo.count = p.getUChar("nc", 0);
    if (cargo.count > NASA_CARGO_MAX) cargo.count = NASA_CARGO_MAX;
    for (int i = 0; i < cargo.count; i++) {
        char key[5];
        snprintf(key, sizeof(key), "nc%d", i);
        String s = p.getString(key, "");
        s.toCharArray(cargo.items[i], 24);
    }
    p.end();
}
