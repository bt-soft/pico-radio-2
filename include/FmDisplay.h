#ifndef __FMDISPLAY_H
#define __FMDISPLAY_H

#include "DisplayBase.h"
#include "Rds.h"
#include "SMeter.h"
#include "SevenSegmentFreq.h"

/**
 *
 */
class FmDisplay : public DisplayBase {
   private:
    // bool ledState = false;
    // int volume = 5;
    // float temperature = 22.5;
    // int antCapValue = 0;
    //
    Rds *pRds;
    SMeter *pSMeter;
    SevenSegmentFreq *pSevenSegmentFreq;

    /**
     * Mono/Stereó felirat megjelenítése
     */
    void showMonoStereo(bool stereo);

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
    FmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);
    ~FmDisplay();

    /**
     * Képernyő kirajzolása
     * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
     */
    void drawScreen() override;

    /**
     * Aktuális képernyő típusának lekérdezése
     */
    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::fm; };

    // /**
    //  *
    //  */
    // void ledStateChanged(double newValue) {
    //     ledState = static_cast<bool>(newValue);
    //     digitalWrite(LED_BUILTIN, ledState);
    // }
};

#endif  //__FMDISPLAY_H