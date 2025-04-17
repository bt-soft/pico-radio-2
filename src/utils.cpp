#include "utils.h"

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
    const __FlashStringHelper *txt = F("TFT touch calibration required\n");
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 - 60);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.println(txt);

    tft.setTextSize(1);
    txt = F("Touch the corners at the indicated places!\n");
    tft.setCursor((tft.width() - tft.textWidth(txt)) / 2, tft.height() / 2 + 20);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println(txt);

    // TFT_eSPI 'bóti' kalibráció indítása (Az IntelliSense fals hibát jelez, de a kód működik)
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
 * Beeper pin init
 */
void beepInit() {
    pinMode(PIN_BEEPER, OUTPUT);
    digitalWrite(PIN_BEEPER, LOW);
}

/**
 *  Pitty hangjelzés
 */
void beepTick() {
    tone(PIN_BEEPER, 800);
    delay(10);
    noTone(PIN_BEEPER);
}

/**
 * Hiba jelzés
 */
void beepError() {
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    tone(PIN_BEEPER, 500);
    delay(100);
    noTone(PIN_BEEPER);
}

}  // namespace Utils
