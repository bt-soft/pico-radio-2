#ifndef __AMDISPLAY_H
#define __AMDISPLAY_H

#include "DisplayBase.h"
#include "SMeter.h"
#include "SevenSegmentFreq.h"

/**
 *
 */
class AmDisplay : public DisplayBase {

   private:
    SMeter *pSMeter;
    SevenSegmentFreq *pSevenSegmentFreq;

   protected:
    /**
     * Rotary encoder esemény lekezelése
     */
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;

    /**
     * Touch (nem képrnyő button) esemény lekezelése
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;

    /**
     * Képernyő menügomb esemény feldolgozása
     */
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) override;

    /**
     * Esemény nélküli display loop
     */
    void displayLoop() override;

   public:
    /**
     * Konstruktor
     */
    AmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);

    /**
     * Destruktor
     */
    ~AmDisplay();

    /**
     * Képernyő kirajzolása
     * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
     */
    void drawScreen() override;

    /**
     * Aktuális képernyő típusának lekérdezése
     */
    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::am; };
};

#endif  //__AMDISPLAY_H