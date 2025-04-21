#include "SevenSegmentFreq.h"

#include "DSEG7_Classic_Mini_Regular_34.h"
#include "rtVars.h"

#define FREQ_7SEGMENT_BFO_WIDTH 110   // BFO kijelzése alatt a kijelző szélessége
#define FREQ_7SEGMENT_SEEK_WIDTH 194  // Seek alatt a kijelző szélessége

namespace SevenSegmentConstants {
constexpr int DigitXStart[] = {141, 171, 200};     // Digit X koordináták kezdőértékei
constexpr int DigitWidth = 30;                     // Egy digit szélessége
constexpr int DigitHeight = FREQ_7SEGMENT_HEIGHT;  // Digit magassága
constexpr int DigitYStart = 20;                    // Digit Y kezdőértéke
constexpr int UnderlineYOffset = 60;               // Aláhúzás Y eltolása
constexpr int UnderlineHeight = 5;                 // Aláhúzás magassága
}  // namespace SevenSegmentConstants

// Színek a különböző módokhoz
const SegmentColors normalColors = {TFT_GOLD, TFT_COLOR(50, 50, 50), TFT_YELLOW};
const SegmentColors screenSaverColors = {TFT_SKYBLUE, TFT_COLOR(50, 50, 50), TFT_SKYBLUE};
const SegmentColors bfoColors = {TFT_ORANGE, TFT_BROWN, TFT_ORANGE};

/**
 * @brief Kirajzolja a frekvenciát a megadott formátumban.
 *
 * @param freq A megjelenítendő frekvencia.
 * @param mask A nem aktív szegmensek maszkja.
 * @param d Az X pozíció eltolása.
 * @param colors A szegmensek színei.
 * @param unit A mértékegység.
 */
void SevenSegmentFreq::drawFrequency(const String& freq, const __FlashStringHelper* mask, int d, const SegmentColors& colors, const __FlashStringHelper* unit) {

    uint16_t spriteWidth = rtv::bfoOn ? FREQ_7SEGMENT_BFO_WIDTH : tft.width() / 2;

    if (rtv::SEEK) {
        spriteWidth = FREQ_7SEGMENT_SEEK_WIDTH;
    }
    spr.createSprite(spriteWidth, FREQ_7SEGMENT_HEIGHT);
    spr.fillScreen(TFT_COLOR_BACKGROUND);
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    spr.setTextDatum(BR_DATUM);

    uint8_t currentBandType = band.getCurrentBandType();
    uint8_t currentDemod = band.getCurrentBand().varData.currMod;

    int x = 222;
    if (rtv::bfoOn) {
        x = 110;
        spr.setTextColor(TFT_BROWN);
        spr.drawString(mask, x, 38);
        spr.setTextColor(TFT_ORANGE);
        spr.drawString(freq, x, 38);
    } else if (currentDemod == FM or currentDemod == AM) {
        x = 190;
    } else if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
        x = 222;
    }
    if (rtv::SEEK) {
        x = 144;
    }

    // Először a maszkot rajzoljuk ki
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.drawString(mask, x, FREQ_7SEGMENT_HEIGHT);
    }

    // Majd utána a frekvenciát
    spr.setTextColor(colors.active);
    spr.drawString(freq, x, FREQ_7SEGMENT_HEIGHT);

    spr.pushSprite(freqDispX + d, freqDispY + 20);
    spr.setFreeFont();
    spr.deleteSprite();

    // Mértékegység kirajzolása
    if (unit != nullptr) {
        tft.setTextDatum(BC_DATUM);
        tft.setFreeFont();
        tft.setTextSize(2);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
        uint16_t xOffset = screenSaverActive ? 205 : 215;
        tft.drawString(unit, freqDispX + xOffset + d, freqDispY + 60);
    }
}

/**
 * @brief Kirajzolja a BFO frekvenciát.
 *
 * @param bfoValue A BFO frekvencia értéke.
 * @param d Az X pozíció eltolása.
 * @param colors A színek.
 */
void SevenSegmentFreq::drawBfo(int bfoValue, int d, const SegmentColors& colors) {

    drawFrequency(String(bfoValue), F("-888"), d, colors);
    tft.setTextSize(2);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(colors.indicator, TFT_BLACK);
    tft.drawString("Hz", freqDispX + 120 + d, freqDispY + 40);
    tft.setTextColor(TFT_BLACK, colors.active);
    tft.fillRect(freqDispX + 156 + d, freqDispY + 21, 42, 20, colors.active);
    tft.drawString("BFO", freqDispX + 160 + d, freqDispY + 40);
    tft.setTextDatum(BR_DATUM);
}

/**
 * @brief Kirajzolja a frekvencia lépésének jelzésére az aláhúzást.
 *
 * @param d Az X pozíció eltolása.
 * @param colors A színek.
 */
void SevenSegmentFreq::drawStepUnderline(int d, const SegmentColors& colors) {

    // Képernyővédő vagy egyszerű üzemmódban nincs aláhúzás a touch-hoz
    if (screenSaverActive or simpleMode) {
        return;
    }

    using namespace SevenSegmentConstants;

    // Töröljük a korábbi aláhúzást
    tft.fillRect(freqDispX + DigitXStart[0] + d, freqDispY + UnderlineYOffset, DigitWidth * 3, UnderlineHeight, TFT_COLOR_BACKGROUND);

    // Rajzoljuk ki az aktuális aláhúzást
    tft.fillRect(freqDispX + DigitXStart[rtv::freqstepnr] + d, freqDispY + UnderlineYOffset, DigitWidth, UnderlineHeight, colors.indicator);
}

/**
 * @brief Kezeli az érintési eseményeket a frekvencia kijelzőn.
 *
 * @param touchX Az érintés X koordinátája.
 * @param touchY Az érintés Y koordinátája.
 * @return true, ha az eseményt kezeltük, false, ha nem.
 */
bool SevenSegmentFreq::handleTouch(bool touched, uint16_t tx, uint16_t ty) {

    // Képernyővédő vagy egyszerű üzemmódban nincs touch a digiteken
    if (screenSaverActive or simpleMode) {
        return false;
    }

    using namespace SevenSegmentConstants;

    // Ellenőrizzük, hogy az érintés a digit teljes területére esett-e
    if (ty >= freqDispY + DigitYStart && ty <= freqDispY + DigitYStart + DigitHeight) {  // Digit teljes magassága

        // Csak 3 digitet ismerünk, amit megérinthet
        for (int i = 0; i <= 2; ++i) {

            // Az i. digitet érintettük meg?
            if (tx >= freqDispX + DigitXStart[i] && tx < freqDispX + DigitXStart[i] + DigitWidth) {

                // Ha ugyanazt a digitet érintettük meg, ami eddig volt aktív, akkor nem csinálunk semmit
                if (rtv::freqstepnr == i) {
                    break;
                }

                // Ha a digit indexe nem 0-2 között van, akkor hiba történt
                if (i > 2) {
                    Utils::beepError();
                    DEBUG("SevenSegmentFreq::handleTouch -> Érvénytelen digit érintés (freqstepnr: %d)\n", rtv::freqstepnr);
                    return false;  // Nem kezeltük az eseményt
                }

                // Ha másik digit-et érintettük meg, akkor beállítjuk a frekvencia lépést
                rtv::freqstepnr = i;  // Frekvencia lépés index beállítása

                // Frekvencia lépés értékének a beállítása (Hz-ben)
                if (rtv::freqstepnr == 0)
                    rtv::freqstep = 1000;
                else if (rtv::freqstepnr == 1)
                    rtv::freqstep = 100;
                else  // (rtv::freqstepnr == 2)
                    rtv::freqstep = 10;
                break;
            }
        }

        // Frissítsük az aláhúzást a kijelzőn
        drawStepUnderline(0, normalColors);

        return true;  // Esemény kezelve
    }

    return false;  // Nem kezeltük az eseményt
}

/**
 * @brief Frekvencia kijelzése a megfelelő formátumban.
 *
 * @param currentFrequency Az aktuális frekvencia (kHz-ben AM/SSB/CW, vagy 10kHz-ben FM esetén).
 */
void SevenSegmentFreq::freqDispl(uint16_t currentFrequency) {

    // Lekérjük az aktuális sáv (band) adatait
    BandTable& currentBand = band.getCurrentBand();
    uint8_t currDemod = currentBand.varData.currMod;
    uint8_t currentBandType = band.getCurrentBandType();

    int d = 0;  // X eltolás, alapértelmezetten 0
    // Megfelelő színek kiválasztása az aktuális mód alapján (normál, BFO, képernyővédő)
    const SegmentColors& colors = rtv::bfoOn ? bfoColors : (screenSaverActive ? screenSaverColors : normalColors);

    // Előző érték törlése a kijelzőről (csak ha nem képernyővédő módban vagyunk)
    if (!screenSaverActive) {
        // A törlendő terület szélessége/magassága függhet a módtól, figyeljünk, hogy a BFO mód vagy más speciális esetek ne okozzanak vizuális hibát
        // pl: Lépést jelző aláhúzást is töröljük, de FM esetén ilyen nincs, belelógna a törlés a STEREO feliratba
        uint32_t clearHeightCorr = currentBandType == FM_BAND_TYPE ? 0 : SevenSegmentConstants::UnderlineHeight;
        tft.fillRect(freqDispX + d, freqDispY + 20, 240, FREQ_7SEGMENT_HEIGHT + 2 + clearHeightCorr, TFT_COLOR_BACKGROUND);
    }

    uint32_t displayFreqHz = 0;  // A megjelenítendő frekvencia Hz-ben

    // Ha nem ScreenSaver módban vagyunk és SSB vagy CW az üzemmód
    if (!screenSaverActive && (currDemod == LSB || currDemod == USB || currDemod == CW)) {

        // Kiszámítjuk a pontos frekvenciát Hz-ben a BFO eltolással
        uint32_t bfoOffset = simpleMode ? 0 : currentBand.varData.lastBFO;  // BFO eltolás
        displayFreqHz = (uint32_t)currentFrequency * 1000 - bfoOffset;

        // // CW módban további 700Hz eltolás (ha aktív a CW shift)
        // if (rtv::CWShift) {
        //     displayFreqHz -= CW_SHIFT_FREQUENCY;
        // }

        // Új formázás: kHz érték és a 100Hz/10Hz rész kiszámítása
        long khz_part = displayFreqHz / 1000;            // Egész kHz rész
        int hz_tens_part = (displayFreqHz % 1000) / 10;  // A 100Hz és 10Hz-es rész (00-99)

        // Formázás: kHz.százHz tízHz
        // A sprintf %ld formátumot használunk a long int (khz_part) és %02d-t a hz_tens_part-hoz (két számjegy, vezető nullával)
        char s[12] = {'\0'};  // String buffer a formázott frekvenciának
        sprintf(s, "%ld.%02d", khz_part, hz_tens_part);

        // BFO kijelzés kezelése (animáció, stb.)
        if (!rtv::bfoOn || rtv::bfoTr) {
            tft.setTextDatum(BR_DATUM);
            tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);

            // A BFO frekvencia kijelzés miatt a frekvencia méretének az animált csökkentése/növelése
            if (rtv::bfoTr) {
                rtv::bfoTr = false;  // Animációs flag törlése

                // Méretváltás animációja (ez a rész opcionális, a régi kódból átvéve)
                for (uint8_t i = 4; i > 1; i--) {

                    tft.setTextSize(rtv::bfoOn ? i : (6 - i));  // Méret beállítása

                    // Régi frekvencia törlése az animációhoz
                    tft.fillRect(freqDispX + d, freqDispY + 20, 240, 48, TFT_BLACK);  // Méretet ellenőrizni!

                    // Új méretű frekvencia kirajzolása
                    constexpr uint16_t X_OFFSET = 230;  // X pozíció eltolás (a digit szélessége + 5 pixel)
                    constexpr uint16_t Y_OFFSET = 62;
                    tft.drawString(String(s), freqDispX + X_OFFSET + d, freqDispY + Y_OFFSET);  // Pozíciót ellenőrizni!
                    delay(100);
                }
            }

            // Ha a BFO nincs bekapcsolva, kirajzoljuk a normál frekvenciát
            if (!rtv::bfoOn) {
                // A maszk ("88 888.88") megfelelőnek tűnik az új formátumhoz
                // Kivettük a F("kHz") paramétert, külön rajzoljuk ki lejjebb
                drawFrequency(String(s), F("88 888.88"), d, colors);

                // A "kHz" felirat kirajzolása külön
                tft.setTextDatum(BC_DATUM);
                tft.setFreeFont();   // Font beállítása (biztonság kedvéért)
                tft.setTextSize(2);  // Méret beállítása (biztonság kedvéért)
                tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);

                constexpr uint16_t X_OFFSET = 215;  // X pozíció eltolás (a digit szélessége + 5 pixel)
                constexpr uint16_t Y_OFFSET = 85;
                tft.drawString(F("kHz"), freqDispX + X_OFFSET + d, freqDispY + Y_OFFSET);  // Y pozíció
            }

            drawStepUnderline(d, colors);  // Aláhúzás kirajzolása a lépésköz jelzésére
        }

        // Ha a BFO be van kapcsolva, kirajzoljuk a BFO értéket is
        if (rtv::bfoOn) {

            // BFO érték kirajzolása (a config.data.currentBFOmanu értéket használva)
            drawBfo(config.data.currentBFOmanu, d, colors);

            // A fő frekvencia kisebb méretben, a BFO mellett
            tft.setTextDatum(BR_DATUM);
            tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);

            // A formázott string (s) kiírása a megfelelő helyre
            constexpr uint16_t X_OFFSET = 230;
            constexpr uint16_t Y_OFFSET = 62;
            tft.drawString(String(s), freqDispX + X_OFFSET + d, freqDispY + Y_OFFSET);  // Pozíciót ellenőrizni!

            // Itt nem rajzolunk "kHz"-t, mert a BFO érték mellett van a "Hz"
        }

        tft.setTextDatum(BC_DATUM);  // Alapértelmezett szöveg igazítás visszaállítása

    } else {  // Nem SSB/CW mód (FM, AM, LW, MW)

        const __FlashStringHelper* unit = nullptr;  // Mértékegység pointere
        String freqStr;                             // Formázott frekvencia string
        const __FlashStringHelper* mask = nullptr;  // Kijelző maszk

        // FM mód
        if (currDemod == FM) {
            unit = F("MHz");

            // Az FM frekvencia 10kHz-es lépésekben van tárolva (pl. 9390 -> 93.90 MHz)
            float displayFreqMHz = currentFrequency / 100.0f;
            freqStr = String(displayFreqMHz, 2);  // Két tizedesjegy pontossággal
            mask = F("188.88");                   // FM maszk

            // FM esetén kicsit balrább toljuk a kijelzést (d-10)
            // Itt a drawFrequency rajzolja a mértékegységet a régi helyre (freqDispY + 60)
            drawFrequency(freqStr, mask, d - 10, colors, unit);

        } else {  // AM, LW, MW módok
            unit = F("kHz");
            // AM/LW/MW esetén a frekvencia kHz-ben van tárolva
            displayFreqHz = currentFrequency;
            freqStr = String(displayFreqHz);  // Nincs tizedesjegy

            // LW/MW esetén más maszkot használunk
            if (currentBandType == MW_BAND_TYPE || currentBandType == LW_BAND_TYPE) {
                mask = F("8888");

            } else {  // SW (rövidhullám)
                // SW esetén MHz-ben, 3 tizedessel jelenítjük meg
                unit = F("MHz");
                float displayFreqMHz = currentFrequency / 1000.0f;
                freqStr = String(displayFreqMHz, 3);
                mask = F("88.888");
            }
            // Itt a drawFrequency rajzolja a mértékegységet a régi helyre (freqDispY + 60)
            drawFrequency(freqStr, mask, d, colors, unit);
        }
    }
}
