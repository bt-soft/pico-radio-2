#ifndef SSTVDISPLAY_H
#define SSTVDISPLAY_H

#include <ArduinoFFT.h>

#include "DisplayBase.h"
#include "FftBase.h"  // A meglévő FFT bázis osztály használata

class SstvDisplay : public DisplayBase {
   public:
    SstvDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band);
    ~SstvDisplay();

    void drawScreen() override;
    void displayLoop() override;
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) override;

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::sstv; }

   private:
};

#endif  // SSTVDISPLAY_H
