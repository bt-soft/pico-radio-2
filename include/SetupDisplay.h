#ifndef __SETUPDISPLAY_H
#define __SETUPDISPLAY_H

#include "DisplayBase.h"

class SetupDisplay : public DisplayBase {

   private:
    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;

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
    SetupDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);

    /**
     * Destruktor
     */
    ~SetupDisplay();

    /**
     * Képernyő kirajzolása
     */
    void drawScreen() override;

    /**
     * Az előző képernyőtípus beállítása
     * A SetupDisplay esetén használjuk, itt adjuk át, hogy hova kell visszatérnie a képrnyő bezárása után
     */
    void setPrevDisplayType(DisplayBase::DisplayType prevDisplay) override { this->prevDisplay = prevDisplay; };

    /**
     * Aktuális képernyő típusának lekérdezése
     */
    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::setup; };
};

#endif  //__SETUPDISPLAY_H
