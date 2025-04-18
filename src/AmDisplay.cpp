#include "AmDisplay.h"

#include <Arduino.h>

/**
 * Konstruktor
 */
AmDisplay::AmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band) {

    DEBUG("AmDisplay::AmDisplay\n");

    // SMeter példányosítása
    pSMeter = new SMeter(tft, 0, 80);

    // Frekvencia kijelzés pédányosítása
    pSevenSegmentFreq = new SevenSegmentFreq(tft, rtv::freqDispX, rtv::freqDispY, band);

    // Függőleges gombok legyártása, nincs saját függőleges gombsor
    DisplayBase::buildVerticalScreenButtons(nullptr, 0);

    // Horizontális képernyőgombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"AntC", TftButton::ButtonType::Pushable},  //
    };

    // Horizontális képernyőgombok legyártása
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData));
}

/**
 * Destruktor
 */
AmDisplay::~AmDisplay() {
    // SMeter trölése
    if (pSMeter) {
        delete pSMeter;
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
void AmDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    DisplayBase::dawStatusLine();

    // RSSI skála kirajzoltatása
    pSMeter->drawSmeterScale();

    // RSSI aktuális érték
    si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = si4735.getCurrentRSSI();
    uint8_t snr = si4735.getCurrentSNR();
    pSMeter->showRSSI(rssi, snr, band.getCurrentBand().varData.currMod == FM);

    // Frekvencia
    float currFreq = band.getCurrentBand().varData.currFreq;  // A Rotary változtatásakor már eltettük a Band táblába
    pSevenSegmentFreq->freqDispl(currFreq);

    // Gombok kirajzolása
    DisplayBase::drawScreenButtons();
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void AmDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    if (STREQ("AntC", event.label)) {
        // If zero, the tuning capacitor value is selected automatically.
        // AM - the tuning capacitance is manually set as 95 fF x ANTCAP + 7 pF.  ANTCAP manual range is 1–6143;
        // FM - the valid range is 0 to 191.

        // Antenna kapacitás állítása
        int maxValue = band.getCurrentBand().varData.currMod == FM ? 191 : 6143;

        int antCapValue = 0;

        DisplayBase::pDialog =
            new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Antenna Tuning capacitor"), F("Capacitor value [pF]:"), &antCapValue, (int)0, (int)maxValue,
                                  (int)0,  // A rotary encoder értéke lesz a step
                                  [this](int newValue) {
                                      // Az új érték beállítása a Band táblába
                                      band.getCurrentBand().varData.antCap = newValue;

                                      // Az új érték beállítása a Si4735-be
                                      Si4735Utils::si4735.setTuneFrequencyAntennaCapacitor(newValue);

                                      // Frissítjük a státusvonalban a kiírást
                                      DisplayBase::drawAntCapStatus(true);
                                  });
    }
}

/**
 * Touch (nem képrnyő button) esemény lekezelése
 * A további gui elemek vezérléséhez
 */
bool AmDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {

    uint8_t currMod = band.getCurrentBand().varData.currMod;

    // A frekvencia kijelző kezeli a touch eseményeket SSB/CW módban
    if (currMod == LSB or currMod == USB or currMod == CW) {
        return pSevenSegmentFreq->handleTouch(touched, tx, ty);
    }

    return false;
}

/**
 * Rotary encoder esemény lekezelése
 */
bool AmDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {

    BandTable &currentBand = band.getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;
    uint16_t currentFrequency = si4735.getFrequency();

    // SSB vagy CW módban a BFO-val finomhangolunk, de csak akkor ha a lépés nem 1000Hz, amit az si4735 amúgy is támogat
    if (currMod == LSB or currMod == USB or currMod == CW) {

        if (encoderState.direction == RotaryEncoder::Direction::Up) {

            // Felfelé hangolásnál
            rtv::freqDec = rtv::freqDec - rtv::freqstep;
            uint32_t freqTot = (uint32_t)(currentFrequency * 1000) + (rtv::freqDec * -1);
            if (freqTot > (uint32_t)(currentBand.pConstData->maximumFreq * 1000)) {
                si4735.setFrequency(currentBand.pConstData->maximumFreq);
                rtv::freqDec = 0;
            }

            if (rtv::freqDec <= -16000) {
                rtv::freqDec = rtv::freqDec + 16000;         // freqDec = 0
                int16_t freqPlus16 = currentFrequency + 16;  // freqPlus16 = 14073
                Si4735Utils::hardwareAudioMuteOn();
                si4735.setFrequency(freqPlus16);
                delay(10);  // fontos, mert az BFO 0 értéknél elcsúszhat a beállított ferekvenciától
            }

        } else {

            // Lefelé hangolásnál
            rtv::freqDec = rtv::freqDec + rtv::freqstep;
            uint32_t freqTot = (uint32_t)(currentFrequency * 1000) - rtv::freqDec;
            if (freqTot < (uint32_t)(currentBand.pConstData->minimumFreq * 1000)) {
                si4735.setFrequency(currentBand.pConstData->minimumFreq);
                rtv::freqDec = 0;
            }

            if (rtv::freqDec >= 16000) {                    // Lefelé átfordulás ága
                rtv::freqDec = rtv::freqDec - 16000;        // freqDec = 0
                int16_t freqMin16 = currentFrequency - 16;  // freqMin16 = 14042
                Si4735Utils::hardwareAudioMuteOn();
                si4735.setFrequency(freqMin16);
                delay(10);  // fontos, mert az BFO 0 értéknél elcsúszhat a beállított ferekvenciától
            }
        }

        config.data.currentBFO = rtv::freqDec;                 // freqDec a finomhangolás mértéke
        currentBand.varData.lastBFO = config.data.currentBFO;  // Mentsük el a finomhangolást
        const int16_t cwBaseOffset = (currMod == CW) ? CW_SHIFT_FREQUENCY : 0;
        int16_t bfoToSet = cwBaseOffset + config.data.currentBFO + config.data.currentBFOmanu;
        si4735.setSSBBfo(bfoToSet);
        checkAGC();

    } else {
        // AM - sima frekvencia léptetés sávhatár ellenőrzéssel
        // Használjuk a rotary encoder gyorsítását (encoderState.value)
        // és a SÁVHOZ BEÁLLÍTOTT LÉPÉSKÖZT (currentBand.varData.currStep)

        // 1. Lépésköz lekérdezése az aktuális sávból (ez már kHz-ben van)
        uint16_t configuredStep = currentBand.varData.currStep;

        // 2. Teljes frekvenciaváltozás kiszámítása
        //    encoderState.value: Hány "logikai" lépést tett az enkóder (gyorsítással)
        //    configuredStep: Az egy logikai lépéshez tartozó frekvenciaváltozás (pl. 9 kHz)
        //    Fontos: int32_t-t használunk a túlcsordulás elkerülésére a szorzásnál
        int32_t change = (int32_t)encoderState.value * configuredStep;

        // 3. Új frekvencia kiszámítása
        //    Szintén int32_t-t használunk, hogy a sávhatárokon kívüli értékeket is kezelni tudjuk
        int32_t newFrequency = (int32_t)currentFrequency + change;

        // 4. Sávhatárok lekérdezése
        uint16_t minFreq = currentBand.pConstData->minimumFreq;
        uint16_t maxFreq = currentBand.pConstData->maximumFreq;

        // 5. Ellenőrzés és korlátozás a sávhatárokra
        if (newFrequency < minFreq) {
            newFrequency = minFreq;
        } else if (newFrequency > maxFreq) {
            newFrequency = maxFreq;
        }

        // 6. Új frekvencia beállítása, csak ha változott
        //    Az összehasonlításhoz és a setFrequency híváshoz vissza kell kasztolni uint16_t-re
        if ((uint16_t)newFrequency != currentFrequency) {
            si4735.setFrequency((uint16_t)newFrequency);
            // Mivel setFrequency-t használunk, az AGC-t is ellenőrizni kellhet,
            // bár valószínűleg AM módban az automatikus AGC jól működik.
            Si4735Utils::checkAGC();  // Szükség esetén
        }
    }

    // Elmentjük a beállított frekvenciát a Band táblába
    currentBand.varData.currFreq = si4735.getFrequency();

    // Beállítjuk, hogy kell majd új frekvenciakijelzés
    DisplayBase::frequencyChanged = true;

    return true;
}

/**
 * Esemény nélküli display loop -> Adatok periódikus megjelenítése
 */
void AmDisplay::displayLoop() {

    // Ha van dialóg, akkor nem frissítjük a komponenseket
    if (DisplayBase::pDialog != nullptr) {
        return;
    }

    BandTable &currentBand = band.getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;  // Aktuális mód lekérdezése

    // Néhány adatot csak ritkábban frissítünk
    static uint32_t elapsedTimedValues = 0;  // Kezdőérték nulla
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {

        // RSSI
        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();
        pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

        // Frissítjük az időbélyeget
        elapsedTimedValues = millis();
    }

    // A Frekvenciát azonnal frissítjuk, de csak ha változott
    if (DisplayBase::frequencyChanged) {
        pSevenSegmentFreq->freqDispl(currentBand.varData.currFreq);
        DisplayBase::frequencyChanged = false;  // Reset
    }
}
