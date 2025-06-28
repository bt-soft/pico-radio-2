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

// --- Fő maszkok és egységek konstanssá tétele ---
constexpr const char* MASK_MAIN = "88 888.88";
constexpr const char* MASK_FM = "188.88";
constexpr const char* MASK_SW = "88.888";
constexpr const char* MASK_MW_LW = "8888";
constexpr const char* MASK_BFO = "-888";
constexpr const char* UNIT_KHZ = "kHz";
constexpr const char* UNIT_MHZ = "MHz";
constexpr const char* UNIT_HZ = "Hz";

// --- Egység kiírás helper ---
// drawUnitLabel-t SevenSegmentFreq::drawUnitLabel-ként, tagfüggvényként valósítjuk meg, hogy elérje a tft-t
void SevenSegmentFreq::drawUnitLabel(uint16_t x, uint16_t y, const char* unit, uint16_t color, uint16_t bg) {
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(color, bg);
    tft.drawString(unit, x, y);
}

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
void SevenSegmentFreq::drawFrequency(const String& freq, const char* mask, const SegmentColors& colors, const char* unit) {
    using namespace SevenSegmentConstants;
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    uint16_t contentWidth = spr.textWidth(mask);
    uint32_t x = calcFreqSpriteXPosition();
    uint16_t spritePushX = freqDispX + x - contentWidth;
    uint16_t spritePushY = freqDispY + SpriteYOffset;
    spr.createSprite(contentWidth, FREQ_7SEGMENT_HEIGHT);
    spr.fillScreen(TFT_COLOR_BACKGROUND);
    spr.setTextSize(1);
    spr.setTextPadding(0);
    spr.setFreeFont(&DSEG7_Classic_Mini_Regular_34);
    spr.setTextDatum(BR_DATUM);
    if (config.data.tftDigitLigth) {
        spr.setTextColor(colors.inactive);
        spr.drawString(mask, contentWidth, FREQ_7SEGMENT_HEIGHT);
    }
    if (mask != nullptr) {
        int maskLen = strlen(mask);
        int freqLen = freq.length();
        char buf[maskLen + 1];
        int freqIdx = freqLen - 1;
        for (int i = maskLen - 1; i >= 0; --i) {
            if (mask[i] == ' ') {
                buf[i] = ' ';
            } else {
                buf[i] = (freqIdx >= 0) ? freq[freqIdx--] : ' ';
            }
        }
        buf[maskLen] = '\0';
        spr.setTextColor(colors.active);
        spr.drawString(buf, contentWidth, FREQ_7SEGMENT_HEIGHT);
    } else {
        spr.setTextColor(colors.active);
        spr.drawString(freq.c_str(), contentWidth, FREQ_7SEGMENT_HEIGHT);
    }
    spr.pushSprite(spritePushX, spritePushY);
    spr.deleteSprite();
    uint16_t spriteRightEdgeX = spritePushX + contentWidth;
    if (unit != nullptr) {
        this->drawUnitLabel(spriteRightEdgeX + UnitXOffset, spritePushY + FREQ_7SEGMENT_HEIGHT, unit, colors.indicator, TFT_COLOR_BACKGROUND);
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

    // Töröljük a korábbi aláhúzást a teljes lehetséges területről
    const int underlineAreaWidth = (DigitXStart[2] + DigitWidth) - DigitXStart[0];
    tft.fillRect(freqDispX + DigitXStart[0], freqDispY + UnderlineYOffset, underlineAreaWidth, UnderlineHeight, TFT_COLOR_BACKGROUND);

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
    if (isDisableHandleTouch()) {
        return false;
    }
    using namespace SevenSegmentConstants;
    if (ty >= freqDispY + DigitYStart && ty <= freqDispY + DigitYStart + DigitHeight) {
        for (int i = 0; i <= 2; ++i) {
            if (tx >= freqDispX + DigitXStart[i] && tx < freqDispX + DigitXStart[i] + DigitWidth) {
                if (rtv::freqstepnr == i) {
                    break;
                }
                rtv::freqstepnr = i;
                if (rtv::freqstepnr == 0)
                    rtv::freqstep = 1000;
                else if (rtv::freqstepnr == 1)
                    rtv::freqstep = 100;
                else
                    rtv::freqstep = 10;
                break;
            }
        }
        drawStepUnderline(normalColors);
        return true;
    }
    return false;
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
    uint32_t bfoOffset = simpleMode ? 0 : currentBand.varData.lastBFO;
    uint32_t displayFreqHz = (uint32_t)currentFrequency * 1000 - bfoOffset;
    char s[12];
    long khz_part = displayFreqHz / 1000;
    int hz_tens_part = abs((int)(displayFreqHz % 1000)) / 10;
    sprintf(s, "%ld.%02d", khz_part, hz_tens_part);
    if (!rtv::bfoOn || rtv::bfoTr) {
        tft.setFreeFont();
        tft.setTextDatum(BR_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
        if (rtv::bfoTr) {
            rtv::bfoTr = false;
            for (uint8_t i = 4; i > 1; i--) {
                tft.setTextSize(rtv::bfoOn ? i : (6 - i));
                clearDisplayArea();
                tft.drawString(s, freqDispX + BfoMiniFreqX, freqDispY + BfoMiniFreqY);
                delay(100);
            }
        }
        if (!rtv::bfoOn) {
            drawFrequency(s, MASK_MAIN, colors, nullptr);
            tft.setTextDatum(BC_DATUM);
            tft.setFreeFont();
            tft.setTextSize(2);
            tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
            tft.drawString(UNIT_KHZ, freqDispX + SsbCwUnitXOffset, freqDispY + SsbCwUnitYOffset);
            drawStepUnderline(colors);
        }
    }
    if (rtv::bfoOn) {
        drawFrequency(String(config.data.currentBFOmanu), MASK_BFO, colors, nullptr);
        tft.setTextSize(2);
        tft.setTextDatum(BL_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
        tft.drawString(UNIT_HZ, freqDispX + BfoHzLabelXOffset, freqDispY + BfoHzLabelYOffset);
        tft.setTextColor(TFT_BLACK, colors.active);
        tft.fillRect(freqDispX + BfoLabelRectXOffset, freqDispY + BfoLabelRectYOffset, BfoLabelRectW, BfoLabelRectH, colors.active);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("BFO", freqDispX + BfoLabelRectXOffset + BfoLabelRectW / 2, freqDispY + BfoLabelRectYOffset + BfoLabelRectH / 2);
        tft.setTextSize(2);
        tft.setTextDatum(BR_DATUM);
        tft.setTextColor(colors.indicator, TFT_COLOR_BACKGROUND);
        tft.drawString(s, freqDispX + BfoMiniFreqX, freqDispY + BfoMiniFreqY);
        tft.setTextSize(1);
        tft.drawString(UNIT_KHZ, freqDispX + BfoMiniFreqX + BfoMiniUnitXOffset, freqDispY + BfoMiniFreqY);
    }
}

/**
 * @brief FM/AM/LW/MW frekvencia kijelzése.
 * @param currentFrequency Az aktuális frekvencia (10kHz FM, kHz egyébként).
 * @param colors A használandó színek.
 * @param d Az X eltolás.
 */
void SevenSegmentFreq::displayFmAmFrequency(uint16_t currentFrequency, const SegmentColors& colors) {
    const char* unit = nullptr;
    const char* mask = nullptr;
    uint8_t currentBandType = band.getCurrentBandType();
    uint8_t currDemod = band.getCurrentBand().varData.currMod;
    char freqStr[12];
    if (currDemod == FM) {
        unit = UNIT_MHZ;
        mask = MASK_FM;
        float displayFreqMHz = currentFrequency / 100.0f;
        dtostrf(displayFreqMHz, 0, 2, freqStr);
        drawFrequency(freqStr, mask, colors, unit);
    } else {
        if (currentBandType == MW_BAND_TYPE || currentBandType == LW_BAND_TYPE) {
            unit = UNIT_KHZ;
            mask = MASK_MW_LW;
            sprintf(freqStr, "%u", currentFrequency);
        } else {
            unit = UNIT_MHZ;
            mask = MASK_SW;
            float swFreq = currentFrequency / 1000.0f;
            dtostrf(swFreq, 0, 3, freqStr);
        }
        drawFrequency(freqStr, mask, colors, unit);
    }
}
