#include "minigame.h"
#include "space_ui.h"
#include <M5Stack.h>

// ─── ミニゲーム集 ──────────────────────────────────────────────────────
// 各ゲームは「指示画面 → 本編ループ → 結果画面」の3段構成。
// 共通のコイン報酬関数は持たず、ゲームごとに難易度に応じた報酬計算をしている。
// 新規ゲームを追加する場合は:
//   1. uint16_t gameXxx() 関数を実装する (戻り値=獲得コイン)
//   2. ファイル末尾の GAMES[] テーブルに { "表示名", gameXxx } を追加
// だけで OK。runGameMenu() 側は自動的にスクロール対応。

// ─── 共通ヘルパー ──────────────────────────────────────────────────────

// 指示画面 (タイトル + 説明 1-2 行) を wait ms 表示する。
static void msgScreen(const char* line1, const char* line2 = nullptr, uint32_t wait = 2000) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);
    M5.Lcd.fillRect(0, 72, 320, 46, SM_HDR);
    spCornerFrame(0, 72, 320, 46);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20, 82);
    M5.Lcd.print(line1);
    if (line2) {
        M5.Lcd.setTextColor(SM_GREY, SM_BG);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(20, 132);
        M5.Lcd.print(line2);
    }
    delay(wait);
}

// 各ゲーム終了後の共通結果表示 (Game Over + 獲得コイン)。
static void showResult(uint16_t earned) {
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);
    M5.Lcd.setTextColor(SM_GREY, SM_BG);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(60, 85);
    M5.Lcd.print("Game Over");
    M5.Lcd.setTextColor(SM_WHITE, SM_BG);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(55, 115);
    M5.Lcd.printf("+%d coins!", earned);
    spCornerFrame(0, 0, 320, 240);
    delay(2000);
}

// ── Game 1: Reaction ────────────────────────────────────────────────────────

static int coinsForReaction(uint32_t ms) {
    if (ms < 300)  return 5;
    if (ms < 500)  return 4;
    if (ms < 800)  return 3;
    if (ms < 1200) return 2;
    if (ms < 2000) return 1;
    return 0;
}

static uint16_t gameReaction() {
    static const int ROUNDS = 3;
    uint16_t total = 0;
    msgScreen("Reaction Game!", "Press B when screen flashes");

    for (int r = 0; r < ROUNDS; r++) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(90, 80);
        M5.Lcd.printf("Round %d/%d", r + 1, ROUNDS);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(100, 115);
        M5.Lcd.print("Get ready...");
        delay(1000 + random(2000));

        M5.Lcd.fillScreen(TFT_GREEN);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
        M5.Lcd.setCursor(100, 95);
        M5.Lcd.print("PRESS B!");

        uint32_t t0 = millis();
        bool pressed = false;
        while (millis() - t0 < 3000) {
            M5.update();
            if (M5.BtnB.wasPressed()) { pressed = true; break; }
            delay(10);
        }
        uint32_t ms = millis() - t0;
        int earned = pressed ? coinsForReaction(ms) : 0;
        total += earned;

        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(50, 80);
        if (pressed) {
            M5.Lcd.printf("%dms", ms);
            M5.Lcd.setCursor(50, 110);
            M5.Lcd.printf("+%d coins!", earned);
        } else {
            M5.Lcd.print("Too slow!");
        }
        delay(1500);
    }
    return total;
}

// ── Game 2: Button Mash ─────────────────────────────────────────────────────

static uint16_t gameButtonMash() {
    msgScreen("Button Mash!", "Mash B as fast as you can! (5s)");
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(110, 100);
    M5.Lcd.print("GO!");

    uint32_t end = millis() + 5000;
    int count = 0;
    while (millis() < end) {
        M5.update();
        if (M5.BtnB.wasPressed()) count++;
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(110, 155);
        uint32_t left = (end - millis()) / 1000 + 1;
        M5.Lcd.printf("%ds  ", (int)left);
        delay(16);
    }

    uint16_t earned = (count >= 40) ? 8 : (count >= 25) ? 5 : (count >= 15) ? 3 : (count >= 8) ? 1 : 0;
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(40, 80);
    M5.Lcd.printf("Presses: %d", count);
    M5.Lcd.setCursor(40, 110);
    M5.Lcd.printf("+%d coins!", earned);
    delay(2000);
    return earned;
}

// ── Game 3: Simon Says ──────────────────────────────────────────────────────

static uint16_t gameSimon() {
    msgScreen("Simon Says!", "Repeat the A/B/C sequence");
    static const int MAX_LEN = 5;
    int seq[MAX_LEN];
    for (int i = 0; i < MAX_LEN; i++) seq[i] = random(3);

    const char*    labels[3] = {"A", "B", "C"};
    const uint16_t cols[3]   = {TFT_RED, TFT_GREEN, TFT_BLUE};

    uint16_t earned = 0;
    for (int len = 1; len <= MAX_LEN; len++) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Lcd.setCursor(60, 50);
        M5.Lcd.printf("Round %d/%d", len, MAX_LEN);
        delay(600);
        for (int i = 0; i < len; i++) {
            M5.Lcd.fillScreen(cols[seq[i]]);
            M5.Lcd.setTextSize(4);
            M5.Lcd.setTextColor(TFT_WHITE, cols[seq[i]]);
            M5.Lcd.setCursor(130, 90);
            M5.Lcd.print(labels[seq[i]]);
            delay(500);
            M5.Lcd.fillScreen(TFT_BLACK);
            delay(300);
        }

        bool ok = true;
        for (int i = 0; i < len && ok; i++) {
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Lcd.setCursor(70, 100);
            M5.Lcd.printf("Input %d/%d", i + 1, len);
            M5.Lcd.setTextSize(1);
            M5.Lcd.setCursor(30, 130);
            M5.Lcd.print("[A]     [B]     [C]");

            uint32_t t0 = millis();
            int got = -1;
            while (millis() - t0 < 4000 && got < 0) {
                M5.update();
                if (M5.BtnA.wasPressed()) got = 0;
                if (M5.BtnB.wasPressed()) got = 1;
                if (M5.BtnC.wasPressed()) got = 2;
                delay(10);
            }
            if (got != seq[i]) ok = false;
        }

        if (ok) {
            earned++;
            M5.Lcd.fillScreen(TFT_GREEN);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
            M5.Lcd.setCursor(110, 100);
            M5.Lcd.print("Correct!");
            delay(800);
        } else {
            M5.Lcd.fillScreen(TFT_RED);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
            M5.Lcd.setCursor(110, 100);
            M5.Lcd.print("Wrong!");
            delay(1200);
            break;
        }
    }
    return earned;
}

// ── Game 4: Rhythm Tap ──────────────────────────────────────────────────────

static uint16_t gameRhythm() {
    msgScreen("Rhythm Tap!", "Press B on the beat!");
    static const int    BEATS    = 5;
    static const uint32_t INTERVAL = 800;
    static const uint32_t WINDOW   = 250;

    int hits = 0;
    uint32_t nextBeat = millis() + 1000;

    for (int b = 0; b < BEATS; b++) {
        while (millis() < nextBeat - 50) {
            uint32_t left = nextBeat - millis();
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Lcd.setCursor(90, 80);
            M5.Lcd.printf("Beat %d/%d", b + 1, BEATS);
            M5.Lcd.setTextSize(1);
            M5.Lcd.setCursor(110, 115);
            M5.Lcd.printf("in %dms  ", (int)left);
            delay(16);
        }

        M5.Lcd.fillScreen(TFT_YELLOW);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_BLACK, TFT_YELLOW);
        M5.Lcd.setCursor(120, 95);
        M5.Lcd.print("TAP!");

        uint32_t flashStart = millis();
        bool hit = false;
        while (millis() - flashStart < WINDOW) {
            M5.update();
            if (M5.BtnB.wasPressed()) { hit = true; break; }
            delay(10);
        }
        if (hit) hits++;
        nextBeat += INTERVAL;
    }

    uint16_t earned = (uint16_t)((hits * 8) / BEATS);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(50, 80);
    M5.Lcd.printf("Hits: %d/%d", hits, BEATS);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.printf("+%d coins!", earned);
    delay(2000);
    return earned;
}

// ── Game 5: Lucky Spin ──────────────────────────────────────────────────────

static uint16_t gameLuckySpin() {
    msgScreen("Lucky Spin!", "Press B to stop the wheel!");

    while (true) {
        int num = (millis() / 80) % 10;
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(6);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setCursor(130, 80);
        M5.Lcd.print(num);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(70, 180);
        M5.Lcd.print("Press B to STOP");
        M5.update();
        if (M5.BtnB.wasPressed()) {
            uint16_t earned = (num == 7) ? 10 : (num % 2 == 0) ? 3 : 1;
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setTextSize(3);
            M5.Lcd.setTextColor((num == 7) ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
            M5.Lcd.setCursor(120, 70);
            M5.Lcd.print(num);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
            M5.Lcd.setCursor(60, 120);
            if (num == 7) M5.Lcd.print("JACKPOT!!");
            else          M5.Lcd.printf("+%d coins!", earned);
            delay(2000);
            return earned;
        }
        delay(50);
    }
}

// ── Game 6: Whack-a-Mole ───────────────────────────────────────────────────

static uint16_t gameWhack() {
    msgScreen("Whack-a-Mole!", "A=Left  B=Center  C=Right");
    static const int ROUNDS = 5;
    static const int POS_X[3] = {30, 130, 230};
    static const char* BTN_LABEL[3] = {"[A]", "[B]", "[C]"};
    int hits = 0;

    for (int r = 0; r < ROUNDS; r++) {
        int pos = random(3);
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(80, 50);
        M5.Lcd.printf("Round %d/%d", r + 1, ROUNDS);
        M5.Lcd.fillCircle(POS_X[pos] + 30, 120, 30, TFT_GREEN);
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
        M5.Lcd.setCursor(POS_X[pos] + 12, 113);
        M5.Lcd.print("O_O");
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(POS_X[pos] + 8, 160);
        M5.Lcd.print(BTN_LABEL[pos]);

        uint32_t t0 = millis();
        int got = -1;
        while (millis() - t0 < 1500 && got < 0) {
            M5.update();
            if (M5.BtnA.wasPressed()) got = 0;
            if (M5.BtnB.wasPressed()) got = 1;
            if (M5.BtnC.wasPressed()) got = 2;
            delay(10);
        }

        bool hit = (got == pos);
        if (hit) hits++;
        M5.Lcd.fillScreen(hit ? TFT_GREEN : TFT_RED);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(hit ? TFT_BLACK : TFT_WHITE, hit ? TFT_GREEN : TFT_RED);
        M5.Lcd.setCursor(100, 100);
        M5.Lcd.print(hit ? "HIT!" : "MISS!");
        delay(600);
    }

    uint16_t earned = hits * 2;
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(50, 80);
    M5.Lcd.printf("Hits: %d/%d", hits, ROUNDS);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.printf("+%d coins!", earned);
    delay(2000);
    return earned;
}

// ── Game 7: Precision Stop ──────────────────────────────────────────────────

static uint16_t gamePrecision() {
    msgScreen("Prec. Stop!", "Stop the bar in the green zone! (B)");
    static const int BAR_W       = 200;
    static const int GREEN_START = 80;
    static const int GREEN_END   = 120;
    static const int TRACK_X     = 60;
    static const int TRACK_Y     = 110;
    static const int TRACK_H     = 20;

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(40, 80);
    M5.Lcd.print("Stop in the GREEN zone!");
    M5.Lcd.fillRect(TRACK_X, TRACK_Y, BAR_W, TRACK_H, TFT_DARKGREY);
    M5.Lcd.fillRect(TRACK_X + GREEN_START, TRACK_Y, GREEN_END - GREEN_START, TRACK_H, TFT_GREEN);

    uint32_t start = millis();
    int prevPos = -1;

    while (true) {
        int pos = (int)((millis() - start) / 8) % (BAR_W * 2);
        if (pos >= BAR_W) pos = BAR_W * 2 - pos;

        if (pos != prevPos) {
            if (prevPos >= 0) {
                uint16_t col = (prevPos >= GREEN_START && prevPos < GREEN_END) ? TFT_GREEN : TFT_DARKGREY;
                M5.Lcd.fillRect(TRACK_X + prevPos, TRACK_Y, 6, TRACK_H, col);
            }
            M5.Lcd.fillRect(TRACK_X + pos, TRACK_Y, 6, TRACK_H, TFT_WHITE);
            prevPos = pos;
        }

        M5.update();
        if (M5.BtnB.wasPressed()) {
            bool inZone = (prevPos >= GREEN_START && prevPos < GREEN_END);
            int  mid    = GREEN_START + (GREEN_END - GREEN_START) / 2;
            uint16_t earned = inZone ? 7 : (abs(prevPos - mid) < 30 ? 2 : 0);
            M5.Lcd.fillScreen(TFT_BLACK);
            M5.Lcd.setTextSize(2);
            M5.Lcd.setTextColor(inZone ? TFT_GREEN : TFT_RED, TFT_BLACK);
            M5.Lcd.setCursor(60, 80);
            M5.Lcd.print(inZone ? "Perfect!" : "Off target!");
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            M5.Lcd.setCursor(60, 115);
            M5.Lcd.printf("+%d coins!", earned);
            delay(2000);
            return earned;
        }
        delay(10);
    }
}

// ── Game 8: Quick Math ──────────────────────────────────────────────────────

static uint16_t gameMath() {
    msgScreen("Quick Math!", "Pick the correct answer: A / B / C");
    static const int QS = 3;
    int correct = 0;

    for (int q = 0; q < QS; q++) {
        int a  = random(1, 10);
        int b  = random(1, 10);
        int op = random(2);
        int ans = op ? (a - b) : (a + b);

        int opts[3];
        opts[0] = ans;
        opts[1] = ans + (random(1, 5) * (random(2) ? 1 : -1));
        opts[2] = ans + (random(6, 10) * (random(2) ? 1 : -1));
        for (int s = 0; s < 2; s++) {
            int r = random(3);
            int tmp = opts[s]; opts[s] = opts[r]; opts[r] = tmp;
        }
        int correctOpt = 0;
        for (int i = 0; i < 3; i++) if (opts[i] == ans) { correctOpt = i; break; }

        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(50, 60);
        if (op) M5.Lcd.printf("%d - %d = ?", a, b);
        else    M5.Lcd.printf("%d + %d = ?", a, b);
        M5.Lcd.setCursor(20,  120); M5.Lcd.printf("A:%3d", opts[0]);
        M5.Lcd.setCursor(110, 120); M5.Lcd.printf("B:%3d", opts[1]);
        M5.Lcd.setCursor(205, 120); M5.Lcd.printf("C:%3d", opts[2]);

        uint32_t t0 = millis();
        int got = -1;
        while (millis() - t0 < 5000 && got < 0) {
            M5.update();
            if (M5.BtnA.wasPressed()) got = 0;
            if (M5.BtnB.wasPressed()) got = 1;
            if (M5.BtnC.wasPressed()) got = 2;
            delay(10);
        }

        bool ok = (got == correctOpt);
        if (ok) correct++;
        M5.Lcd.fillScreen(ok ? TFT_GREEN : TFT_RED);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(ok ? TFT_BLACK : TFT_WHITE, ok ? TFT_GREEN : TFT_RED);
        M5.Lcd.setCursor(90, 100);
        M5.Lcd.print(ok ? "Correct!" : "Wrong!");
        delay(800);
    }

    uint16_t earned = correct * 2;
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(50, 80);
    M5.Lcd.printf("Score: %d/%d", correct, QS);
    M5.Lcd.setCursor(50, 110);
    M5.Lcd.printf("+%d coins!", earned);
    delay(2000);
    return earned;
}

// ── Game 9: Hot & Cold ──────────────────────────────────────────────────────

static uint16_t gameHotCold() {
    msgScreen("Hot & Cold!", "Guess 1-9. A=Up C=Down B=Guess");
    int secret = random(1, 10);
    int guess  = 5;
    int tries  = 0;
    static const int MAX_TRIES = 5;

    while (tries < MAX_TRIES) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(3);
        M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK);
        M5.Lcd.setCursor(140, 70);
        M5.Lcd.print(guess);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.setCursor(30, 140);
        M5.Lcd.printf("Tries left: %d", MAX_TRIES - tries);
        M5.Lcd.setCursor(30, 160);
        M5.Lcd.print("[A]+  [B]Guess  [C]-");

        bool acted = false;
        while (!acted) {
            M5.update();
            if (M5.BtnA.wasPressed()) { guess = min(9, guess + 1); acted = true; }
            if (M5.BtnC.wasPressed()) { guess = max(1, guess - 1); acted = true; }
            if (M5.BtnB.wasPressed()) {
                tries++;
                int diff = abs(guess - secret);
                if (diff == 0) {
                    uint16_t earned = max(1, 6 - tries) * 2;
                    M5.Lcd.fillScreen(TFT_GREEN);
                    M5.Lcd.setTextSize(2);
                    M5.Lcd.setTextColor(TFT_BLACK, TFT_GREEN);
                    M5.Lcd.setCursor(60, 90);
                    M5.Lcd.printf("Correct! +%d coins", earned);
                    delay(2000);
                    return earned;
                }
                const char* hint = (diff <= 1) ? "Burning HOT!" : (diff <= 3) ? "Warm" : "Cold...";
                M5.Lcd.fillScreen(TFT_BLACK);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setTextColor(diff <= 1 ? TFT_RED : diff <= 3 ? TFT_YELLOW : TFT_CYAN, TFT_BLACK);
                M5.Lcd.setCursor(60, 100);
                M5.Lcd.print(hint);
                delay(1000);
                acted = true;
            }
            delay(16);
        }
    }

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(40, 80);
    M5.Lcd.printf("It was %d!", secret);
    M5.Lcd.setCursor(40, 110);
    M5.Lcd.print("No coins...");
    delay(2500);
    return 0;
}

// ── Game 10: Survivor ──────────────────────────────────────────────────────

static uint16_t gameSurvivor() {
    msgScreen("Survivor!", "Press B to dodge! Avoid A and C!");
    uint32_t start = millis();
    uint32_t nextEvent = millis() + random(1000, 3000);
    bool danger = false;
    uint32_t dangerEnd = 0;
    bool alive = true;

    while (alive && millis() - start < 15000) {
        uint32_t now = millis();
        if (!danger && now >= nextEvent) {
            danger    = true;
            dangerEnd = now + 600;
            nextEvent = now + random(1500, 3500);
        }
        if (danger && now >= dangerEnd) danger = false;

        M5.Lcd.fillScreen(danger ? TFT_RED : TFT_BLACK);
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_WHITE, danger ? TFT_RED : TFT_BLACK);
        M5.Lcd.setCursor(60, 60);
        M5.Lcd.print(danger ? "DODGE! Press B!" : "Stay calm...");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(90, 110);
        M5.Lcd.printf("Time: %ds / 15s", (int)((now - start) / 1000));

        M5.update();
        if (!danger && (M5.BtnA.wasPressed() || M5.BtnC.wasPressed())) alive = false;
        delay(50);
    }

    uint32_t survived = (millis() - start) / 1000;
    uint16_t earned   = alive ? 8 : (survived >= 8 ? 4 : survived >= 4 ? 2 : 0);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(40, 80);
    M5.Lcd.printf("Survived: %ds", (int)survived);
    M5.Lcd.setCursor(40, 110);
    M5.Lcd.printf("+%d coins!", earned);
    delay(2000);
    return earned;
}

// ── Game 11: Lucky Roll ────────────────────────────────────────────────────

static uint16_t gameLuckyRoll() {
    msgScreen("Lucky Roll!", "Press B to roll the dice!");
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(60, 100);
    M5.Lcd.print("Press B to roll!");
    while (true) { M5.update(); if (M5.BtnB.wasPressed()) break; delay(16); }

    for (int i = 0; i < 15; i++) {
        int d1 = random(1, 7), d2 = random(1, 7);
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(4);
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
        M5.Lcd.setCursor(70, 90);
        M5.Lcd.printf("[%d] [%d]", d1, d2);
        delay(60 + i * 10);
    }

    int d1 = random(1, 7), d2 = random(1, 7);
    int sum = d1 + d2;
    bool doubles = (d1 == d2);
    uint16_t earned = doubles ? 8 : (sum > 9 ? 4 : sum > 6 ? 2 : 0);

    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setTextColor(doubles ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(70, 70);
    M5.Lcd.printf("[%d] [%d]", d1, d2);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(60, 130);
    if (doubles) M5.Lcd.print("DOUBLES! Lucky!");
    else         M5.Lcd.printf("Sum=%d +%d coins", sum, earned);
    delay(2500);
    return earned;
}

// ─── ゲーム選択メニュー ────────────────────────────────────────────────

// メニュー1行分の登録情報。fn が NULL の項目は作らないこと。
struct GameEntry {
    const char* name;     // 表示名 (英数 ~10文字以内)
    uint16_t (*fn)();     // ゲーム本体。戻り値が獲得コイン。
};

// 並び順がメニュー上の順番。新規ゲームは末尾追加が安全。
static const GameEntry GAMES[] = {
    {"Reaction",   gameReaction  },
    {"Btn Mash",   gameButtonMash},
    {"Simon Says", gameSimon     },
    {"Rhythm Tap", gameRhythm    },
    {"Lucky Spin", gameLuckySpin },
    {"Whack-Mole", gameWhack     },
    {"Prec.Stop",  gamePrecision },
    {"Quick Math", gameMath      },
    {"Hot&Cold",   gameHotCold   },
    {"Survivor",   gameSurvivor  },
    {"Lucky Roll", gameLuckyRoll },
};
static const int GAME_COUNT = (int)(sizeof(GAMES) / sizeof(GAMES[0]));
static const int VISIBLE    = 5; // 同時表示行数 (shop と同じパターン)

// 背景には触れずコンテンツだけ書き換える軽量再描画 (チラつき抑止)。
static void drawGameMenuContent(int sel) {
    // Header strip
    M5.Lcd.fillRect(0, 0, 320, 28, SM_HDR);
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(80, 6);
    M5.Lcd.print("Mini Games");
    M5.Lcd.drawFastHLine(0, 28, 320, SM_BORDER);

    int start = max(0, min(sel - VISIBLE / 2, GAME_COUNT - VISIBLE));

    for (int i = 0; i < VISIBLE && (start + i) < GAME_COUNT; i++) {
        int idx      = start + i;
        int y        = 32 + i * 36;
        bool selected = (idx == sel);
        uint16_t rowBg = selected ? SM_SEL : SM_BG;
        M5.Lcd.fillRect(2, y, 316, 32, rowBg);
        if (selected) {
            M5.Lcd.fillRect(2, y, 2, 32, SM_WHITE);
        }
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(selected ? SM_WHITE : SM_GREY, rowBg);
        M5.Lcd.setCursor(12, y + 7);
        M5.Lcd.printf("%2d. %s", idx + 1, GAMES[idx].name);
    }

    // Footer strip
    M5.Lcd.fillRect(0, 212, 320, 28, SM_HDR);
    M5.Lcd.drawFastHLine(0, 212, 320, SM_BORDER);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setTextColor(SM_LIGHT, SM_HDR); M5.Lcd.setCursor(10,  220); M5.Lcd.print("[A] Move");
    M5.Lcd.setTextColor(SM_WHITE, SM_HDR); M5.Lcd.setCursor(115, 220); M5.Lcd.print("[B] Play");
    M5.Lcd.setTextColor(SM_GREY,  SM_HDR); M5.Lcd.setCursor(225, 220); M5.Lcd.print("[C] Back");
}

// ミニゲーム選択 → 実行 → 結果表示 → 戻る。
// A=選択移動 / B=決定 (ゲーム実行) / C=戻る (コイン 0 で抜ける)。
// 戻り値は獲得コイン。ゲーム実行後はそのまま呼び出し元へ戻る (Act 画面)。
uint16_t runGameMenu() {
    int sel = 0;

    // 背景は入口で1回だけ。navigation は drawGameMenuContent でコンテンツだけ更新。
    spDrawBackground();
    spDrawStarfield(0, 0, 320, 240);
    spCornerFrame(0, 0, 320, 240);
    drawGameMenuContent(sel);

    while (true) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
            sel = (sel + 1) % GAME_COUNT;
            drawGameMenuContent(sel);
        }
        if (M5.BtnB.wasPressed()) {
            uint16_t earned = GAMES[sel].fn();
            showResult(earned);
            return earned;
        }
        if (M5.BtnC.wasPressed()) return 0;
        delay(16);
    }
}
