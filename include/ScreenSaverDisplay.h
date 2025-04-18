#ifndef __SCREENSAVERDISPLAY_H
#define __SCREENSAVERDISPLAY_H

#include <Arduino.h>

#include "DisplayBase.h"
#include "SevenSegmentFreq.h"

class ScreenSaverDisplay : public DisplayBase {

   private:
    uint16_t saverX;
    uint16_t saverY;
    uint16_t currentFrequency;  // Az aktuális frekvencia

    uint16_t posSaver = 0;
    uint8_t saverLineColors[63];  // A vonal színeinek tömbje

    SevenSegmentFreq *pSevenSegmentFreq;

   protected:
    /**
     * Esemény nélküli display loop
     * Ezt a metódust a ScreenSaver animációjára használjuk
     * Nem kell figyelni a touch eseményt, azt már a főprogram figyeli és leállítja/törli a ScreenSaver-t
     */
    void displayLoop() override;

   public:
    /**
     * Konstruktor
     */
    ScreenSaverDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);

    /**
     * Destruktor
     */
    ~ScreenSaverDisplay();

    /**
     * Képernyő kirajzolása
     */
    void drawScreen() override;

    /**
     * Aktuális képernyő típusának lekérdezése
     */
    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::screenSaver; };
};

#endif  //__SCREENSAVERDISPLAY_H
