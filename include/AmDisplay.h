#ifndef __AMDISPLAY_H
#define __AMDISPLAY_H

#include "DisplayBase.h"
#include "MiniAudioFft.h"
#include "SMeter.h"
#include "SevenSegmentFreq.h"
#include "CwDecoder.h" // CW dekóder include

/**
 *
 */
class AmDisplay : public DisplayBase {

   private:
    SMeter *pSMeter;
    SevenSegmentFreq *pSevenSegmentFreq;
    MiniAudioFft *pMiniAudioFft;
    CwDecoder *pCwDecoderInstance; // Pointer a CwDecoder példányra

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
    AmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band, CwDecoder *cwDecoder); // Módosított konstruktor

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