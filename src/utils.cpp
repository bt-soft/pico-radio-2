#include "utils.h"

#include "Config.h"  // Szükséges a config objektum eléréséhez
#include "defines.h"

namespace Utils {

/**
 * Várakozás a soros port megnyitására
 * @param tft a TFT kijelző példánya
 */
void debugWaitForSerial(TFT_eSPI &tft) {
#ifdef __DEBUG
    beepError();
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Nyisd meg a soros portot!", 0, 0);
    while (!Serial) {
    }
    tft.fillScreen(TFT_BLACK);
    beepTick();
#endif
}

/**
 * TFT érintőképernyő kalibráció
 * @param tft TFT kijelző példánya
 * @param calData kalibrációs adatok
 */
void tftTouchCalibrate(TFT_eSPI &tft, uint16_t (&calData)[5]) {

    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(2);
    const __FlashStringHelper *txt = F("TFT erintokepernyo kalibracio szukseges\n");
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 - 60);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println(txt);

    tft.setTextSize(1);
    txt = F("Erintsd meg a jelzett helyeken a sarkokat!\n");
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 + 20);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println(txt);

    // TFT_eSPI 'bóti' kalibráció indítása
    tft.calibrateTouch(calData, TFT_YELLOW, TFT_BLACK, 15);

    txt = F("Kalibracio befejezodott!");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.println(txt);

    DEBUG("// Használd ezt a kalibrációs kódot a setup()-ban:\n");
    DEBUG("  uint16_t calData[5] = { ");
    for (uint8_t i = 0; i < 5; i++) {
        DEBUG("%d", calData[i]);
        if (i < 4) {
            DEBUG(", ");
        }
    }
    DEBUG(" };\n");
    DEBUG("  tft.setTouch(calData);\n");
}

/**
 * Hiba megjelenítése a képrnyőn
 */
void displayException(TFT_eSPI &tft, const char *msg) {

    int16_t screenWidth = tft.width();
    int16_t screenHeight = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.drawRect(0, 0, screenWidth, screenHeight, TFT_RED);  // 2px széles piros keret
    tft.drawRect(1, 1, screenWidth - 2, screenHeight - 2, TFT_RED);

    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);  // Középre igazítás
    tft.setTextSize(2);

    tft.drawString("HIBA!", screenWidth / 2, screenHeight / 3);
    tft.setTextSize(1);
    tft.drawString(msg, screenWidth / 2, screenHeight / 2);

    DEBUG(msg);
    // Végtelen ciklusba esünk  és a belső LED villogtatásával jelezzük hogy hiba van
    while (true) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(300);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(300);
    }
}

/**
 *  Pitty hangjelzés
 */
void beepTick() {
    // Csak akkor csipogunk, ha a beeper engedélyezve van
    if (!config.data.beeperEnabled) return;
    tone(PIN_BEEPER, 800);
    delay(10);
    noTone(PIN_BEEPER);
}

/**
 * Hiba jelzés
 */
void beepError() {
    // Csak akkor csipogunk, ha a beeper engedélyezve van
    if (!config.data.beeperEnabled) return;
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    noTone(PIN_BEEPER);
}

/**
 * @brief Ellenőrzi, hogy egy C string egy adott indextől kezdve csak szóközöket tartalmaz-e.
 *
 * @param str A vizsgálandó C string.
 * @param offset Az index, ahonnan a vizsgálatot kezdeni kell.
 * @return true, ha az offsettől kezdve csak szóközök vannak (vagy a string vége van), egyébként false.
 */
bool isRemainingOnlySpaces(const char *str, uint16_t offset) {
    // Ellenőrizzük, hogy a string és az offset érvényes-e
    if (str == nullptr) {
        return true;  // Null pointert tekinthetjük "csak szóköznek" vagy hibának, itt true-t adunk
    }
    size_t len = strlen(str);
    if (offset >= len) {
        return true;  // Ha az offset a stringen túl mutat, akkor nincs hátra karakter, tehát "csak szóköz"
    }

    // Ciklus az offsettől a string végéig
    for (size_t i = offset; i < len; ++i) {
        if (str[i] != ' ') {
            return false;  // Találtunk egy nem szóköz karaktert
        }
    }

    // Ha a ciklus végigfutott, csak szóközök voltak
    return true;
}

/**
 * @brief Összehasonlít két C stringet max n karakterig, de a második string végén lévő szóközöket figyelmen kívül hagyja.
 *
 * @param s1 Az első C string.
 * @param s2 A második C string (ennek a végéről hagyjuk el a szóközöket).
 * @param n A maximálisan összehasonlítandó karakterek száma.
 * @return 0, ha a stringek (a szóközök elhagyásával) megegyeznek n karakterig,
 *         negatív érték, ha s1 kisebb, mint s2, pozitív érték, ha s1 nagyobb, mint s2.
 */
int strncmpIgnoringTrailingSpaces(const char *s1, const char *s2, size_t n) {
    if (n == 0) {
        return 0;  // Nincs mit összehasonlítani
    }

    // s2 effektív hosszának megkeresése (szóközök nélkül a végén)
    size_t len2 = strlen(s2);
    size_t effective_len2 = len2;
    while (effective_len2 > 0 && s2[effective_len2 - 1] == ' ') {
        effective_len2--;
    }

    size_t i = 0;
    while (i < n) {
        bool end1 = (s1[i] == '\0');
        bool end2_effective = (i >= effective_len2);

        if (end1 && end2_effective) return 0;                                    // Mindkettő véget ért (effektíven) n karakteren belül
        if (end1 || end2_effective) return (end1) ? -1 : 1;                      // Csak az egyik ért véget
        if (s1[i] != s2[i]) return (unsigned char)s1[i] - (unsigned char)s2[i];  // Különbség
        i++;
    }
    return 0;  // n karakterig nem volt különbség
}

/**
 * @brief Eltávolítja a C string végéről a szóközöket (in-place).
 *
 * @param str A módosítandó C string.
 */
void trimTrailingSpaces(char *str) {
    if (str == nullptr) return;  // Null pointer ellenőrzés

    int len = strlen(str);
    while (len > 0 && str[len - 1] == ' ') {
        str[len - 1] = '\0';  // Szóköz helyére null terminátor
        len--;                // Hossz csökkentése
    }
}

/**
 * @brief Eltávolítja a C string elejéről a szóközöket (in-place).
 *
 * @param str A módosítandó C string.
 */
void trimLeadingSpaces(char *str) {
    if (str == nullptr) return;

    int i = 0;
    while (str[i] != '\0' && str[i] == ' ') {
        i++;
    }

    if (i > 0) {
        // Karakterek eltolása balra
        int j = 0;
        while (str[i + j] != '\0') {
            str[j] = str[i + j];
            j++;
        }
        str[j] = '\0';  // Új null terminátor
    }
}

/**
 * @brief Eltávolítja a C string elejéről és végéről a szóközöket (in-place).
 * @param str A módosítandó C string.
 */
void trimSpaces(char *str) {
    trimLeadingSpaces(str);
    trimTrailingSpaces(str);
}
}  // namespace Utils
