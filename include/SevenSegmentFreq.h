#ifndef __SEVENSEGMENTFREQ_H
#define __SEVENSEGMENTFREQ_H

#include <TFT_eSPI.h>

#include "Band.h"
#include "Config.h"
#include "rtVars.h"

#define FREQ_7SEGMENT_HEIGHT 38  // Magassága

// Színstruktúra
struct SegmentColors {
    uint16_t active;
    uint16_t inactive;
    uint16_t indicator;
};

/**
 *
 */
class SevenSegmentFreq {

   private:
    TFT_eSPI& tft;
    TFT_eSprite spr;
    Band& band;

    uint16_t freqDispX, freqDispY;
    bool screenSaverActive;
    bool simpleMode = false;  // Egyszerű mód, csak a frekvenciát mutatja

    /**
     * @brief Kirajzolja a frekvenciát a megadott formátumban.
     *
     * @param freq A megjelenítendő frekvencia.
     * @param mask A nem aktív szegmensek maszkja.
     * @param colors A szegmensek színei.
     * @param unit A mértékegység.
     */
    void drawFrequency(const String& freq, const char* mask, const SegmentColors& colors, const char* unit = nullptr);

    /**
     * @brief Kiválasztja a megfelelő szegmens színeket az aktuális mód alapján.
     * @return A kiválasztott SegmentColors struktúra.
     */
    const SegmentColors& getSegmentColors() const;

    /**
     * @brief SSB/CW frekvencia kijelzése (BFO-val vagy anélkül).
     * @param currentFrequency Az aktuális frekvencia (kHz).
     * @param colors A használandó színek.
     */
    void displaySsbCwFrequency(uint16_t currentFrequency, const SegmentColors& colors);

    /**
     * @brief FM/AM/LW/MW frekvencia kijelzése.
     * @param currentFrequency Az aktuális frekvencia (10kHz FM, kHz egyébként).
     * @param colors A használandó színek.
     */
    void displayFmAmFrequency(uint16_t currentFrequency, const SegmentColors& colors);

    /**
     * @brief Letörli a frekvencia kijelző területét.
     */
    void clearDisplayArea();

    /**
     * @brief Kiszámítja a frekvencia kijelzés jobb szélének referencia X pozícióját a fő képernyőn.
     * @return A kiszámított X pozíció.
     */
    uint32_t calcFreqSpriteXPosition() const;

    /**
     * @brief Kirajzolja a frekvencia lépésének jelzésére az aláhúzást.
     *
     * @param colors A színek.
     */
    void drawStepUnderline(const SegmentColors& colors);

    /**
     * @brief Ellenőrzi, hogy a touch események kezelése szükséges-e.
     */
    inline bool isDisableHandleTouch() {
        // Ha van BFO, vagy Képernyővédő vagy egyszerű üzemmódban vagyunk, akkor nincs aláhúzás a touch-hoz
        return screenSaverActive or simpleMode or rtv::bfoOn;
    }

   public:
    /**
     *
     */
    SevenSegmentFreq(TFT_eSPI& tft, uint16_t freqDispX, uint16_t freqDispY, Band& band, bool screenSaverActive = false, bool simpleMode = false)
        : tft(tft), freqDispX(freqDispX), freqDispY(freqDispY), band(band), screenSaverActive(screenSaverActive), simpleMode(simpleMode), spr(&tft) {}

    /**
     *
     */
    void freqDispl(uint16_t freq);

    /**
     * Pozíció beállítása (pl.: a ScreenSaver számára)
     */
    inline void setPositions(uint16_t freqDispX, uint16_t freqDispY) {
        this->freqDispX = freqDispX;
        this->freqDispY = freqDispY;
    }

    /**
     * @brief Kezeli az érintési eseményeket a frekvencia kijelzőn.
     *
     * @param touchX Az érintés X koordinátája.
     * @param touchY Az érintés Y koordinátája.
     * @return true, ha az eseményt kezeltük, false, ha nem.
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty);

    // --- Egység kiírás helper ---
    // Hozzáadjuk a deklarációt a headerhez is!
    // SevenSegmentFreq.h-ban: public: void drawUnitLabel(uint16_t x, uint16_t y, const char* unit, uint16_t color, uint16_t bg);
    void drawUnitLabel(uint16_t x, uint16_t y, const char* unit, uint16_t color, uint16_t bg);
};

#endif  //__SEVENSEGMENTFREQ_H
