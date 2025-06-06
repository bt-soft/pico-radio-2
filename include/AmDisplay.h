#ifndef __AMDISPLAY_H
#define __AMDISPLAY_H

#include "CwDecoder.h"  // CW dekóder osztály
#include "DisplayBase.h"
#include "MiniAudioFft.h"
#include "RadioButton.h"  // Új RadioButton include
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
    static constexpr uint8_t DECODER_TEXT_AREA_X_START = 5;
    static constexpr uint8_t DECODER_TEXT_AREA_Y_MARGIN_TOP = 5;        // S-Meter alatt
    static constexpr uint8_t DECODER_TEXT_AREA_Y_MARGIN_BOTTOM = 5;     // Horizontális gombok felett
    static constexpr uint8_t RTTY_MAX_TEXT_LINES = 8;                   // Megjeleníthető sorok száma (betűmérettől függ)
    static constexpr uint8_t RTTY_LINE_BUFFER_SIZE = 55;                // Egy sor maximális karakterszáma
    static constexpr uint8_t DECODER_LINE_GAP = 2;                      // Sorköz a dekódolt szöveg sorai között (pixel)
    static constexpr uint8_t DECODER_MODE_BTN_W = SCRN_BTN_W / 2 + 10;  // Kicsit szélesebb mini gombok
    static constexpr uint8_t DECODER_MODE_BTN_H = SCRN_BTN_H / 2;
    static constexpr uint8_t DECODER_MODE_BTN_GAP_X = 5;  // Rés a szövegterület és a gombok között
    static constexpr uint8_t DECODER_MODE_BTN_GAP_Y = 3;  // Függőleges rés a módváltó gombok között    // Dekódolt szöveg tárolása (CW és RTTY)
    String decodedTextDisplayLines[RTTY_MAX_TEXT_LINES];
    uint8_t decodedTextCurrentLineIndex = 0;
    String decodedTextCurrentLineBuffer = "";
    uint16_t decoderCharHeight_ = 0;  // Karakter magasság a dekóder szöveghez

    // Dekódolási módválasztó rádiógomb csoport
    RadioButtonGroup decoderModeGroup;
    enum class DecodeMode { OFF, RTTY, MORSE };
    DecodeMode currentDecodeMode = DecodeMode::OFF;  // Kezdetben kikapcsolva
    uint8_t decoderModeStartId_ = 0;                 // Dekóder gombok kezdő ID-ja
    void setDecodeModeBasedOnButtonId(uint8_t buttonId);

    uint16_t decodedTextAreaX, decodedTextAreaY, decodedTextAreaW, decodedTextAreaH;
    uint16_t decodeModeButtonsX;  // A módváltó gombok oszlopának X pozíciója    // Segédfüggvények
    void drawDecodedTextAreaBackground();
    void appendDecodedCharacter(char c);
    void redrawCurrentInputLine();  // Csak az aktuális sort rajzolja újra
    void updateDecodedTextDisplay();
    void clearDecodedTextBufferOnly();        // Csak a puffert törli
    void clearDecodedTextBufferAndDisplay();  // Törli a puffert és frissíti a kijelzőt
    void drawDecodeModeButtons();
    void setDecodeMode(DecodeMode newMode);
    void decodeCwAndRttyText();

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