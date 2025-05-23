#ifndef __AMDISPLAY_H
#define __AMDISPLAY_H

#include "DisplayBase.h"
#include "MiniAudioFft.h"
#include "RadioButton.h"  // Új RadioButton include
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

    // Dekóder terület és gombok konstansai
    static constexpr uint16_t DECODER_TEXT_AREA_X_START = 5;
    static constexpr uint16_t DECODER_TEXT_AREA_Y_MARGIN_TOP = 5; // S-Meter alatt
    static constexpr uint16_t DECODER_TEXT_AREA_Y_MARGIN_BOTTOM = 5; // Horizontális gombok felett
    static constexpr int RTTY_MAX_TEXT_LINES = 6;                  // Megjeleníthető sorok száma (betűmérettől függ)
    static constexpr int RTTY_LINE_BUFFER_SIZE = 60;               // Egy sor maximális karakterszáma
    static constexpr uint16_t DECODER_MODE_BTN_W = SCRN_BTN_W / 2 + 10; // Kicsit szélesebb mini gombok
    static constexpr uint16_t DECODER_MODE_BTN_H = SCRN_BTN_H / 2;
    static constexpr uint16_t DECODER_MODE_BTN_GAP_X = 5; // Rés a szövegterület és a gombok között
    static constexpr uint16_t DECODER_MODE_BTN_GAP_Y = 3; // Függőleges rés a módváltó gombok között

    // Dekódolt szöveg tárolása
    String rttyDisplayLines[RTTY_MAX_TEXT_LINES];
    uint8_t rttyCurrentLineIndex = 0;
    String rttyCurrentLineBuffer = "";

    // Dekódolási módválasztó rádiógomb csoport
    RadioButtonGroup decoderModeGroup;
    enum class DecodeMode { OFF, RTTY, MORSE };
    DecodeMode currentDecodeMode = DecodeMode::OFF;  // Kezdetben kikapcsolva

    AudioProcessor *pAudioProcessor;      // Audio processor példány
    RttyDecoder *pRttyDecoder = nullptr;  // RTTY dekóder

    // A szövegterület tényleges koordinátái és méretei (konstruktorban számolva)
    uint16_t rttyTextAreaX, rttyTextAreaY, rttyTextAreaW, rttyTextAreaH;
    uint16_t decodeModeButtonsX; // A módváltó gombok oszlopának X pozíciója

    // Segédfüggvények
    void drawRttyTextAreaBackground();
    void appendRttyCharacter(char c);
    void updateRttyTextDisplay();
    void drawDecodeModeButtons();
    void setDecodeMode(DecodeMode newMode);

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