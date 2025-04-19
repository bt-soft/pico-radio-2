#include "FmDisplay.h"

#include <Arduino.h>

/**
 * Konstruktor
 */
FmDisplay::FmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band), pSMeter(nullptr), pRds(nullptr), pSevenSegmentFreq(nullptr) {

    DEBUG("FmDisplay::FmDisplay\n");

    // SMeter példányosítása
    pSMeter = new SMeter(tft, 0, 80);

    // RDS példányosítása
    pRds = new Rds(tft, si4735, 80, 62,  // Station x,y
                   0, 80,                // Message x,y
                   2, 42,                // Time x,y
                   0, 140                // program type x,y
    );

    // Frekvencia kijelzés pédányosítása
    pSevenSegmentFreq = new SevenSegmentFreq(tft, rtv::freqDispX, rtv::freqDispY, band);

    // Függőleges gombok legyártása, nincs saját függőleges gombsor
    DisplayBase::buildVerticalScreenButtons(nullptr, 0);

    // Horizontális Képernyőgombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"RDS", TftButton::ButtonType::Toggleable, TFT_TOGGLE_BUTTON_STATE(config.data.rdsEnabled)},  //
    };

    // Horizontális képernyőgombok legyártása
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData));
}

/**
 *
 */
FmDisplay::~FmDisplay() {

    // SMeter trölése
    if (pSMeter) {
        delete pSMeter;
    }

    // RDS trölése
    if (pRds) {
        delete pRds;
    }

    // Frekvencia kijelző törlése
    if (pSevenSegmentFreq) {
        delete pSevenSegmentFreq;
    }
}

/**
 * Képernyő kirajzolása
 * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
 */
void FmDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    DisplayBase::dawStatusLine();

    // RSSI skála kirajzoltatása
    pSMeter->drawSmeterScale();

    BandTable &currentBand = band.getCurrentBand();

    // RSSI aktuális érték
    si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = si4735.getCurrentRSSI();
    uint8_t snr = si4735.getCurrentSNR();
    pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

    // RDS (erőből a 'valamilyen' adatok megjelenítése)
    if (config.data.rdsEnabled) {
        pRds->displayRds(true);
    }

    // Mono/Stereo aktuális érték
    this->showMonoStereo(si4735.getCurrentPilot());

    // Frekvencia
    float currFreq = currentBand.varData.currFreq;  // A Rotary változtatásakor már eltettük a Band táblába
    pSevenSegmentFreq->freqDispl(currFreq);

    // Gombok kirajzolása
    DisplayBase::drawScreenButtons();
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void FmDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    DEBUG("FmDisplay::processScreenButtonTouchEvent() -> id: %d, label: %s, state: %s\n", event.id, event.label, TftButton::decodeState(event.state));

    if (STREQ("RDS", event.label)) {
        // Radio Data System
        config.data.rdsEnabled = event.state == TftButton::ButtonState::On;
        if (config.data.rdsEnabled) {
            pRds->displayRds(true);
        } else {
            pRds->clearRds();
        }
    }
}

/**
 * Touch (nem képrnyő button) esemény lekezelése
 * A további gui elemek vezérléséhez
 */
bool FmDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; }

/**
 * Mono/Stereo vétel megjelenítése
 */
void FmDisplay::showMonoStereo(bool stereo) {

    // STEREO/MONO háttér
    uint32_t backGroundColor = stereo ? TFT_RED : TFT_BLUE;
    tft.fillRect(rtv::freqDispX + 191, rtv::freqDispY + 60, 38, 12, backGroundColor);

    // Felirat
    tft.setFreeFont();
    tft.setTextColor(TFT_WHITE, backGroundColor);
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);
    tft.setTextPadding(0);
    tft.drawString(stereo ? "STEREO" : "MONO", rtv::freqDispX + 210, rtv::freqDispY + 71);
}

/**
 * Rotary encoder esemény lekezelése
 */
bool FmDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {

    BandTable &currentBand = band.getCurrentBand();

    // Kiszámítjuk a frekvencia lépés nagyságát
    uint16_t step = encoderState.value * currentBand.varData.currStep;  // A lépés nagysága

    // Beállítjuk a frekvenciát
    si4735.setFrequency(si4735.getFrequency() + step);

    // Elmentjük a band táblába az aktuális frekvencia értékét
    currentBand.varData.currFreq = si4735.getFrequency();

    // RDS törlés
    pRds->clearRds();

    // Beállítjuk, hogy kell majd új frekvenciakijelzés
    DisplayBase::frequencyChanged = true;

    return true;
}

/**
 * Esemény nélküli display loop -> Adatok periódikus megjelenítése
 */
void FmDisplay::displayLoop() {

    // Ha van dialóg, akkor nem frissítjük a komponenseket
    if (DisplayBase::pDialog != nullptr) {
        return;
    }

    BandTable &currentBand = band.getCurrentBand();

    // Néhány adatot csak ritkábban frissítünk
    static uint32_t elapsedTimedValues = 0;  // Kezdőérték nulla
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {

        // RSSI
        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();
        pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

        // RDS
        if (config.data.rdsEnabled) {
            pRds->showRDS(snr);
        }

        // Mono/Stereo
        static bool prevStereo = false;
        bool stereo = si4735.getCurrentPilot();
        // Ha változott, akkor frissítünk
        if (stereo != prevStereo) {
            this->showMonoStereo(stereo);
            prevStereo = stereo;  // Frissítsük az előző értéket
        }

        // Frissítjük az időbélyeget
        elapsedTimedValues = millis();
    }

    // A Frekvenciát azonnal frissítjuk, de csak ha változott
    if (DisplayBase::frequencyChanged) {
        pSevenSegmentFreq->freqDispl(currentBand.varData.currFreq);
        DisplayBase::frequencyChanged = false;  // Reset
    }
}
