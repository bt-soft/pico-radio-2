#include "ScreenSaverDisplay.h"

#include <Arduino.h>

#include "PicoSensorUtils.h"

#define SAVER_ANIMATION_STEPS 500          // Az animáció ciklusának hossza
#define SAVER_ANIMATION_LINE_LENGTH 63     // A vonal hossza
#define SAVER_LINE_CENTER 31               // A vonal közepe
#define SAVER_X_OFFSET_1 10                // X eltolás a t < 200 szakaszon
#define SAVER_Y_OFFSET_1 5                 // Y eltolás a t < 200 szakaszon
#define SAVER_X_OFFSET_2 189               // X eltolás a 200 <= t < 250 szakaszon
#define SAVER_Y_OFFSET_2 205               // Y eltolás a 200 <= t < 250 szakaszon
#define SAVER_X_OFFSET_3 439               // X eltolás a 250 <= t < 450 szakaszon
#define SAVER_Y_OFFSET_3 44                // Y eltolás a 250 <= t < 450 szakaszon
#define SAVER_X_OFFSET_4 10                // X eltolás az t >= 450 szakaszon
#define SAVER_Y_OFFSET_4 494               // Y eltolás az t >= 450 szakaszon
#define SAVER_ANIMATION_STEP_JUMP 3        // Ugrás az animációs ciklusban
#define SAVER_NEW_POS_INTERVAL_MSEC 15000  // Új pozíció generálásának intervalluma (15 másodperc)
#define SAVER_COLOR_FACTOR 64              // szín tényező

/**
 * Konstruktor
 */
ScreenSaverDisplay::ScreenSaverDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band) {

    DEBUG("ScreenSaverDisplay::ScreenSaverDisplay()\n");

    // Előre kiszámítjuk a 'c' értékeket a vonalhoz
    for (uint8_t i = 0; i < SAVER_ANIMATION_LINE_LENGTH; i++) {
        saverLineColors[i] = (31 - abs(i - SAVER_LINE_CENTER));
    }

    // Frekvencia kijelzés pédányosítása
    saverX = tft.width() / 2;   // Kezdeti érték a képernyő közepére
    saverY = tft.height() / 2;  // Kezdeti érték a képernyő közepére
    pSevenSegmentFreq = new SevenSegmentFreq(tft, saverX - FREQ_7SEGMENT_HEIGHT, saverY - 20, band, true);

    // Kezdeti keret szélesség lekérdezése
    currentFrequency = band.getCurrentBand().varData.currFreq;
    pSevenSegmentFreq->freqDispl(currentFrequency);
}

/**
 * Destruktor
 */
ScreenSaverDisplay::~ScreenSaverDisplay() {
    delete (pSevenSegmentFreq);
    DEBUG("ScreenSaverDisplay::~ScreenSaverDisplay()\n");
}

/**
 * Képernyő kirajzolása
 * A ScreenSaver rögtön a képernyő törlésével kezd
 */
void ScreenSaverDisplay::drawScreen() { tft.fillScreen(TFT_COLOR_BACKGROUND); }

/**
 * Esemény nélküli display loop - ScreenSaver futtatása
 * Nem kell figyelni a touch eseményt, azt már a főprogram figyeli és leállítja/törli a ScreenSaver-t
 */
void ScreenSaverDisplay::displayLoop() {
    uint16_t t = posSaver;
    posSaver++;
    if (posSaver == SAVER_ANIMATION_STEPS) {
        posSaver = 0;
    }

    for (uint8_t i = 0; i < SAVER_ANIMATION_LINE_LENGTH; i++) {
        uint8_t c = saverLineColors[i];  // Használjuk az előre kiszámított 'c' értékeket

        if (t < 200) {
            tft.drawPixel(saverX - SAVER_X_OFFSET_1 + t, saverY - SAVER_Y_OFFSET_1, (c * SAVER_COLOR_FACTOR) + c);
        } else if (t >= 200 and t < 250) {
            tft.drawPixel(saverX + SAVER_X_OFFSET_2, saverY - SAVER_Y_OFFSET_2 + t, (c * SAVER_COLOR_FACTOR) + c);
        } else if (t >= 250 and t < 450) {
            tft.drawPixel(saverX + SAVER_X_OFFSET_3 - t, saverY + SAVER_Y_OFFSET_3, (c * SAVER_COLOR_FACTOR) + c);
        } else {
            tft.drawPixel(saverX - SAVER_X_OFFSET_4, saverY + SAVER_Y_OFFSET_4 - t, (c * SAVER_COLOR_FACTOR) + c);
        }

        t += SAVER_ANIMATION_STEP_JUMP;
        if (t >= SAVER_ANIMATION_STEPS) {
            t -= SAVER_ANIMATION_STEPS;
        }
    }

    static uint32_t elapsedSaver = 0;
    if ((elapsedSaver + SAVER_NEW_POS_INTERVAL_MSEC) < millis()) {  // 15 másodpercenként
        elapsedSaver = millis();

        tft.fillScreen(TFT_COLOR_BACKGROUND);

        // Véletlen pozíció a frekvenciának
        saverX = random(tft.width() / 2) + 10;
        saverY = random(tft.height() / 2) + 5;

        // Frekvencia kijelzése
        pSevenSegmentFreq->setPositions(saverX - 30, saverY);
        pSevenSegmentFreq->freqDispl(currentFrequency);

        // Az animált keretet hozzá igazítjuk a frekvenciához
        saverX += 35;  // A keret X pozíciója
        saverY += 20;  // A keret Y pozíciója

        if (true) {  // konfigból beállíthatóvá tenni, hogy mutassa-e a feszültséget

            float vSupply = PicoSensorUtils::readVBus();

            uint8_t bat = map(int(vSupply * 100), MIN_BATTERRY_VOLTAGE, MAX_BATTERRY_VOLTAGE, 0, 100);
            // csak 0-100% között lehet
            if (bat < 0) {
                bat = 0;
            } else if (bat > 100) {
                bat = 100;
            }

            // Szín beállítása az alacsony feszültségekhez
            uint32_t colorBatt = TFT_DARKCYAN;
            if (bat < 5) {
                colorBatt = TFT_COLOR(248, 252, 0);
            } else if (bat < 15) {
                colorBatt = TFT_ORANGE;
            }

            tft.drawRect(saverX + 145, saverY, 38, 18, colorBatt);
            tft.drawRect(saverX + 184, saverY + 4, 2, 10, colorBatt);
            tft.setFreeFont();
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM);
            tft.drawString(String(bat) + "%", saverX + 164, saverY + 13);
        }
    }
}
