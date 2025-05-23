#ifndef __AMDISPLAY_H
#define __AMDISPLAY_H

#include "DisplayBase.h"
#include "MiniAudioFft.h"
#include "RttyDecoder.h"  // RTTY dekóder osztály
#include "SMeter.h"
#include "SevenSegmentFreq.h"
#include "defines.h"  // A kijelző konstansok

/**
 *
 */
class AmDisplay : public DisplayBase {

   private:
    SMeter *pSMeter;
    SevenSegmentFreq *pSevenSegmentFreq;
    MiniAudioFft *pMiniAudioFft;

    // RTTY szövegterület paraméterei
    static constexpr uint16_t RTTY_TEXT_AREA_X_MARGIN = 5;
    // static constexpr uint16_t RTTY_TEXT_AREA_Y_MARGIN_TOP = 5;     // RSSI/SNR alatt
    static constexpr uint16_t RTTY_TEXT_AREA_Y_MARGIN_BOTTOM = 5;  // Gombok felett
    static constexpr int RTTY_MAX_TEXT_LINES = 6;                  // Megjeleníthető sorok száma (betűmérettől függ)
    static constexpr int RTTY_LINE_BUFFER_SIZE = 60;               // Egy sor maximális karakterszáma

    // Dekódolt szöveg tárolása
    String rttyDisplayLines[RTTY_MAX_TEXT_LINES];
    uint8_t rttyCurrentLineIndex = 0;
    String rttyCurrentLineBuffer = "";

    // RTTY / CW módválasztó gomb
    TftButton *pRttyCwModeButton = nullptr;
    enum class DecodeMode { RTTY, MORSE };            // Átnevezve CW-ről MORSE-ra
    DecodeMode currentDecodeMode = DecodeMode::RTTY;  // Kezdetben RTTY

    AudioProcessor *pAudioProcessor;      // Audio processor példány
    RttyDecoder *pRttyDecoder = nullptr;  // RTTY dekóder

    bool isRttyEnabled = true;
    uint16_t rttyTextAreaX, rttyTextAreaY, rttyTextAreaW, rttyTextAreaH;

    // Segédfüggvények
    void drawRttyTextAreaBackground();
    void appendRttyCharacter(char c);
    void updateRttyTextDisplay();
    void toggleRttyCwMode();

    void drawRttyCwModeButton();

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