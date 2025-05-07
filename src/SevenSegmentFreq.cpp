#include "SevenSegmentFreq.h"

#include "DSEG7_Classic_Mini_Regular_34.h"
#include "utils.h"  // Beep miatt

namespace SevenSegmentConstants {
constexpr int DigitXStart[] = {141, 171, 200};     // Digit X koordináták kezdőértékei az aláhúzáshoz
constexpr int DigitWidth = 25;                     // Egy digit szélessége az aláhúzáshoz
constexpr int DigitHeight = FREQ_7SEGMENT_HEIGHT;  // Digit magassága
constexpr int DigitYStart = 20;                    // Digit Y kezdőértéke
constexpr int UnderlineYOffset = 60;               // Aláhúzás Y eltolása
constexpr int UnderlineHeight = 5;                 // Aláhúzás magassága

// Sprite és Unit pozíciók
constexpr uint16_t SpriteYOffset = 20;  // Sprite Y eltolása a freqDispY-hoz képest
constexpr uint16_t UnitXOffset = 5;     // Mértékegység X eltolása a sprite jobb szélétől

// Referencia X pozíciók a jobb igazításhoz
constexpr uint16_t RefXDefault = 222;  // Alapértelmezett (SSB/CW BFO nélkül, MW, LW)
constexpr uint16_t RefXSeek = 144;     // SEEK módban
constexpr uint16_t RefXBfo = 115;      // BFO módban
constexpr uint16_t RefXFmAm = 190;     // FM/AM módban

// BFO mód specifikus pozíciók és méretek
constexpr uint16_t BfoLabelRectXOffset = 156;
constexpr uint16_t BfoLabelRectYOffset = 21;
constexpr uint16_t BfoLabelRectW = 42;
constexpr uint16_t BfoLabelRectH = 20;
constexpr uint16_t BfoLabelTextXOffset = 160;  // BFO szöveg X eltolása
constexpr uint16_t BfoLabelTextYOffset = 40;   // BFO szöveg Y eltolása (BL datumhoz)
constexpr uint16_t BfoHzLabelXOffset = 120;    // Hz felirat X eltolása (BL datumhoz)
constexpr uint16_t BfoHzLabelYOffset = 40;     // Hz felirat Y eltolása (BL datumhoz)
constexpr uint16_t BfoMiniFreqX = 220;         // Kicsinyített fő frekvencia X (BR datumhoz)
constexpr uint16_t BfoMiniFreqY = 62;          // Kicsinyített fő frekvencia Y (BR datumhoz)
constexpr uint16_t BfoMiniUnitXOffset = 20;    // Kicsinyített kHz/MHz X eltolása a mini frekvenciától
constexpr uint16_t SsbCwUnitXOffset = 215;     // "kHz" felirat X eltolása SSB/CW módban (BFO nélkül)
constexpr uint16_t SsbCwUnitYOffset = 80;      // "kHz" felirat Y eltolása SSB/CW módban (BFO nélkül)

// Törlési terület konstansai
constexpr uint16_t ClearAreaBaseWidth = 240;                          // Törlési szélesség (lehet, hogy finomítani kell)
constexpr uint16_t ClearAreaHeightCorrection = UnderlineHeight + 15;  // Magasság korrekció (aláhúzás + unit)
}  // namespace SevenSegmentConstants

// Színek a különböző módokhoz
const SegmentColors normalColors = {TFT_GOLD, TFT_COLOR(50, 50, 50), TFT_YELLOW};
const SegmentColors screenSaverColors = {TFT_SKYBLUE, TFT_COLOR(50, 50, 50), TFT_SKYBLUE};
const SegmentColors bfoColors = {TFT_ORANGE, TFT_BROWN, TFT_ORANGE};

/**
 * @brief Kiszámítja a frekvencia kijelzés Sprite jobb szélének referencia X pozícióját a fő képernyőn.
 * @return A kiszámított X pozíció.
 */
uint32_t SevenSegmentFreq::calcFreqSpriteXPosition() const {
    using namespace SevenSegmentConstants;
    uint8_t currentDemod = band.getCurrentBand().varData.currMod;
    uint32_t x = RefXDefault;

    if (rtv::SEEK) {
        x = RefXSeek;
    } else if (rtv::bfoOn and !screenSaverActive) {
        x = RefXBfo;
    } else if (currentDemod == FM or currentDemod == AM) {
        x = RefXFmAm;
    }
    return x;
}

/**
 * @brief Kirajzolja a frekvenciát a megadott formátumban.
 *
 * @param freq A megjelenítendő frekvencia.
 * @param mask A nem aktív szegmensek maszkja.
 * @param colors A szegmensek színei.
 * @param unit A mértékegység.
 */
void SevenSegmentFreq::drawFrequency(const String& freq, const __FlashStringHelper* mask, const SegmentColors& colors, const __FlashStringHelper* unit) {
    using namespace SevenSegmentConstants;

    // --- Sprite szélességének meghatározása a maszk alapján ---
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    uint16_t contentWidth = spr.textWidth(mask);  // Szélesség lekérése a sprite kontextusában

    // Referencia X pozíció kiszámítása a segédfüggvénnyel
    uint32_t x = calcFreqSpriteXPosition();

    // --- Sprite létrehozása és rajzolás ---
    // Sprite X pozíciójának kiszámítása a jobb szél igazításához
    uint16_t spritePushX = freqDispX + x - contentWidth;
    uint16_t spritePushY = freqDispY + SpriteYOffset;

    spr.createSprite(contentWidth, FREQ_7SEGMENT_HEIGHT);
    spr.fillScreen(TFT_COLOR_BACKGROUND);  // Sprite belső tartalmának törlése (freki törlés)
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);  // Font beállítása a sprite-on is
    spr.setTextDatum(BR_DATUM);                       // Jobbra-alulra igazítás

    // Először a maszkot rajzoljuk ki
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.drawString(mask, contentWidth, FREQ_7SEGMENT_HEIGHT);  // Jobbra igazítva a sprite-on belül
    }

    // Majd utána a frekvenciát
    spr.setTextColor(colors.active);
    spr.drawString(freq, contentWidth, FREQ_7SEGMENT_HEIGHT);  // Jobbra igazítva a sprite-on belül

    // Megjelenítés
    spr.pushSprite(spritePushX, spritePushY);
    spr.deleteSprite();

    uint16_t spriteRightEdgeX = spritePushX + contentWidth;  // A sprite jobb szélének X koordinátája

    // DEBUG: Sárga keret rajzolása a sprite köré a fő TFT-re
    // tft.drawRect(spritePushX, spritePushY, contentWidth, FREQ_7SEGMENT_HEIGHT, TFT_YELLOW);

    // kHz/MHz mértékegység kirajzolása
    if (unit != nullptr) {
        // Betűtípus és méret beállítása a méretek lekérdezése és a rajzolás előtt
        tft.setFreeFont();  // Standard font használata a mértékegységhez
        tft.setTextSize(2);

        // Szöveg tulajdonságainak beállítása a rajzoláshoz
        tft.setTextDatum(BL_DATUM);  // Bottom-Left (bal alsó) igazítás használata a számjegyek utáni pozicionáláshoz
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);

        // X pozíció kiszámítása a frekvencia számjegyek jobb széléhez képest
        uint16_t unitX = spriteRightEdgeX + UnitXOffset;

        // Y pozíció kiszámítása, hogy az alapvonal a számjegyek alapvonalához igazodjon
        uint16_t unitY = spritePushY + FREQ_7SEGMENT_HEIGHT;  // Igazítás a sprite alsó széléhez (alapvonal)

        // Terület törlése és a mértékegység kiírása
        tft.drawString(unit, unitX, unitY);
    }
}

/**
 * @brief Kirajzolja a frekvencia lépésének jelzésére az aláhúzást.
 *
 * @param colors A színek.
 */
void SevenSegmentFreq::drawStepUnderline(const SegmentColors& colors) {

    // Ha nem nincs touch, akkor aláhúzás sem kell
    if (isDisableHandleTouch()) {
        return;
    }

    using namespace SevenSegmentConstants;

    // Töröljük a korábbi aláhúzást
    tft.fillRect(freqDispX + DigitXStart[0], freqDispY + UnderlineYOffset, DigitWidth * 3, UnderlineHeight, TFT_COLOR_BACKGROUND);

    // Rajzoljuk ki az aktuális aláhúzást
    tft.fillRect(freqDispX + DigitXStart[rtv::freqstepnr], freqDispY + UnderlineYOffset, DigitWidth, UnderlineHeight, colors.indicator);
}

/**
 * @brief Kiválasztja a megfelelő szegmens színeket az aktuális mód alapján.
 * @return A kiválasztott SegmentColors struktúra.
 */
const SegmentColors& SevenSegmentFreq::getSegmentColors() const {
    if (rtv::bfoOn) {
        return bfoColors;
    } else if (screenSaverActive) {
        return screenSaverColors;
    } else {
        return normalColors;
    }
}

// --- Helper method implementations ---

/**
 * @brief Kezeli az érintési eseményeket a frekvencia kijelzőn.
 *
 * @param touchX Az érintés X koordinátája.
 * @param touchY Az érintés Y koordinátája.
 * @return true, ha az eseményt kezeltük, false, ha nem.
 */
bool SevenSegmentFreq::handleTouch(bool touched, uint16_t tx, uint16_t ty) {

    // BFO, Képernyővédő vagy egyszerű üzemmódban nincs touch a digiteken
    if (isDisableHandleTouch()) {
        return false;  // Nem kezeltük az eseményt
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
        drawStepUnderline(normalColors);

        return true;  // Esemény kezelve
    }

    return false;  // Nem kezeltük az eseményt
}

/**
 * @brief Letörli a frekvencia kijelző területét.
 */
void SevenSegmentFreq::clearDisplayArea() {

    // Képernyővédő és egyszerű módban (ahol nincs aláhúzás és kevesebb helyet foglal) vagyunk, akkor nem törlünk
    if (screenSaverActive or simpleMode) {
        return;
    }

    // A törlendő terület magassága: mindig töröljük az aláhúzás helyét is, hogy a módváltáskor biztosan eltűnjön.
    uint32_t clearHeightCorr = SevenSegmentConstants::ClearAreaHeightCorrection;

    // A szélességnek elég nagynak kell lennie, hogy minden módot lefedjen
    tft.fillRect(freqDispX, freqDispY + SevenSegmentConstants::SpriteYOffset, SevenSegmentConstants::ClearAreaBaseWidth, FREQ_7SEGMENT_HEIGHT + clearHeightCorr,
                 TFT_COLOR_BACKGROUND);
}

/**
 * @brief Frekvencia kijelzése a megfelelő formátumban.
 *
 * @param currentFrequency Az aktuális frekvencia (kHz-ben AM/SSB/CW, vagy 10kHz-ben FM esetén).
 */
void SevenSegmentFreq::freqDispl(uint16_t currentFrequency) {

    // Lekérjük az aktuális színeket
    const SegmentColors& colors = getSegmentColors();

    // Mód alapján a megfelelő megjelenítő függvény hívása
    const uint8_t currDemod = band.getCurrentBand().varData.currMod;
    if (!screenSaverActive and (currDemod == LSB or currDemod == USB or currDemod == CW)) {
        // SSB vagy CW mód (BFO kezeléssel)
        displaySsbCwFrequency(currentFrequency, colors);
    } else {
        // FM, AM, LW, MW mód
        displayFmAmFrequency(currentFrequency, colors);
    }

    // Alapértelmezett szöveg igazítás visszaállítása, ha szükséges
    tft.setTextDatum(BC_DATUM);
}

/**
 * @brief SSB/CW frekvencia kijelzése (BFO-val vagy anélkül).
 * @param currentFrequency Az aktuális frekvencia (kHz).
 * @param colors A használandó színek.
 */
void SevenSegmentFreq::displaySsbCwFrequency(uint16_t currentFrequency, const SegmentColors& colors) {
    using namespace SevenSegmentConstants;

    BandTable& currentBand = band.getCurrentBand();
    uint8_t currDemod = currentBand.varData.currMod;

    // Kiszámítjuk a pontos frekvenciát Hz-ben a BFO eltolással
    uint32_t bfoOffset = simpleMode ? 0 : currentBand.varData.lastBFO;
    uint32_t displayFreqHz = (uint32_t)currentFrequency * 1000 - bfoOffset;

    // Formázás: kHz érték és a 100Hz/10Hz rész kiszámítása
    char s[12];
    long khz_part = displayFreqHz / 1000;
    int hz_tens_part = abs((int)(displayFreqHz % 1000)) / 10;  // Előjel nélküli érték kell
    sprintf(s, "%ld.%02d", khz_part, hz_tens_part);

    // --- BFO kijelzés kezelése ---
    if (!rtv::bfoOn or rtv::bfoTr) {
        tft.setFreeFont();
        tft.setTextDatum(BR_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);

        // Animáció a BFO be/ki kapcsolásakor
        if (rtv::bfoTr) {
            rtv::bfoTr = false;
            for (uint8_t i = 4; i > 1; i--) {
                tft.setTextSize(rtv::bfoOn ? i : (6 - i));
                clearDisplayArea();  // Törlés minden lépésben
                tft.drawString(String(s), freqDispX + BfoMiniFreqX, freqDispY + BfoMiniFreqY);
                delay(100);
            }
        }

        // Ha a BFO nincs bekapcsolva, kirajzoljuk a normál frekvenciát + a métrékegységet
        if (!rtv::bfoOn) {
            drawFrequency(String(s), F("88 888.88"), colors, nullptr);  // Fő frekvencia

            // A "kHz" felirat nem  a frekvencia mellett van, hanem alatta
            tft.setTextDatum(BC_DATUM);
            tft.setFreeFont();
            tft.setTextSize(2);
            tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
            tft.drawString(F("kHz"), freqDispX + SsbCwUnitXOffset, freqDispY + SsbCwUnitYOffset);

            drawStepUnderline(colors);  // Aláhúzás
        }
    }

    // Ha a BFO be van kapcsolva, kirajzoljuk a BFO értéket
    if (rtv::bfoOn) {

        // 1. BFO érték kirajzolása a 7 szegmensesre
        drawFrequency(String(config.data.currentBFOmanu), F("-888"), colors, nullptr);

        // A BFO frekvencia 'Hz' felirata
        tft.setTextSize(2);
        tft.setTextDatum(BL_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);  // Háttérszínnel töröljünk
        tft.drawString("Hz", freqDispX + BfoHzLabelXOffset, freqDispY + BfoHzLabelYOffset);

        // BFO felirat
        tft.setTextColor(TFT_BLACK, colors.active);
        tft.fillRect(freqDispX + BfoLabelRectXOffset, freqDispY + BfoLabelRectYOffset, BfoLabelRectW, BfoLabelRectH, colors.active);      // Háttér
        tft.setTextDatum(MC_DATUM);                                                                                                       // Középre igazítás
        tft.drawString("BFO", freqDispX + BfoLabelRectXOffset + BfoLabelRectW / 2, freqDispY + BfoLabelRectYOffset + BfoLabelRectH / 2);  // Szöveg

        // 2. Fő frekvencia kisebb méretben
        tft.setTextSize(2);
        tft.setTextDatum(BR_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);  // Háttérszínnel töröl
        tft.drawString(String(s), freqDispX + BfoMiniFreqX, freqDispY + BfoMiniFreqY);

        // 3. Fő frekvencia "kHz" felirata még kisebb méretben
        tft.setTextSize(1);
        tft.drawString("kHz", freqDispX + BfoMiniFreqX + BfoMiniUnitXOffset, freqDispY + BfoMiniFreqY);
    }
}

/**
 * @brief FM/AM/LW/MW frekvencia kijelzése.
 * @param currentFrequency Az aktuális frekvencia (10kHz FM, kHz egyébként).
 * @param colors A használandó színek.
 * @param d Az X eltolás.
 */
void SevenSegmentFreq::displayFmAmFrequency(uint16_t currentFrequency, const SegmentColors& colors) {
    String freqStr;
    const __FlashStringHelper* unit = nullptr;
    const __FlashStringHelper* mask = nullptr;
    uint8_t currentBandType = band.getCurrentBandType();
    uint8_t currDemod = band.getCurrentBand().varData.currMod;  // Szükséges az FM ellenőrzéshez

    if (currDemod == FM) {
        unit = F("MHz");
        mask = F("188.88");
        float displayFreqMHz = currentFrequency / 100.0f;
        freqStr = String(displayFreqMHz, 2);
        drawFrequency(freqStr, mask, colors, unit);  // FM eltolás

    } else {  // AM, LW, MW, SW
        unit = F("kHz");
        mask = (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) ? F("8888") : F("88.888");
        freqStr = (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) ? String(currentFrequency) : String(currentFrequency / 1000.0f, 3);
        if (currentBandType != MW_BAND_TYPE and currentBandType != LW_BAND_TYPE) unit = F("MHz");  // SW MHz-ben
        drawFrequency(freqStr, mask, colors, unit);
    }
}
