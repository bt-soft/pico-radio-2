#include "FmDisplay.h"

#include <Arduino.h>

#include "FrequencyInputDialog.h"

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
        {"Freq", TftButton::ButtonType::Pushable},
        // //----
        // {"Popup", TftButton::ButtonType::Pushable},  //
        // {"Multi", TftButton::ButtonType::Pushable},
        // //
        // {"b-Val", TftButton::ButtonType::Pushable},  //
        // {"i-Val", TftButton::ButtonType::Pushable},  //
        // {"f-Val", TftButton::ButtonType::Pushable},  //
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
    } else if (STREQ("Freq", event.label)) {
        // Open the FrequencyInputDialog
        BandTable &currentBand = band.getCurrentBand();
        DisplayBase::pDialog = new FrequencyInputDialog(this, DisplayBase::tft, band, currentBand.varData.currFreq);
    }
    //  else if (STREQ("AM", event.label)) {
    //     ::newDisplay = DisplayBase::DisplayType::am;

    // } else if (STREQ("Popup", event.label)) {
    //     // Popup
    //     DisplayBase::pDialog = new MessageDialog(this, DisplayBase::tft, 280, 130, F("Dialog title"), F("Folytassuk?"), "Aha", "Ne!!");

    // } else if (STREQ("Multi", event.label)) {
    //     // Multi button Dialog
    //     const char *buttonLabels[] = {"Opt-1", "Opt-2", "Opt-3", "Opt-4", "Opt-5", "Opt-6", "Opt-7", "Opt-8", "Opt-9", "Opt-10", "Opt-11", "Opt-12"};
    //     int buttonsCount = ARRAY_ITEM_COUNT(buttonLabels);

    //     DisplayBase::pDialog = new MultiButtonDialog(this, DisplayBase::tft, 400, 180, F("Valasszon opciot!"), buttonLabels, buttonsCount);

    // } else if (STREQ("b-Val", event.label)) {
    //     // b-ValueChange
    //     DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 250, 150, F("LED state"), F("Value:"), &ledState, false, true, false,
    //                                                  [this](double newValue) { this->ledStateChanged(newValue); });

    // } else if (STREQ("i-Val", event.label)) {
    //     // i-ValueChange
    //     DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 250, 150, F("Volume"), F("Value:"), &volume, (int)0, (int)63, (int)1);

    // } else if (STREQ("f-Val", event.label)) {
    //     // f-ValueChange
    //     DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 250, 150, F("Temperature"), F("Value:"), &temperature, (float)-15.0, (float)+30.0, (float)0.05);
    // }
}

// /**
//  *
//  */
// void FmDisplay::processDialogButtonResponse(TftButton::ButtonTouchEvent &event) {

//     DEBUG("FmDisplay::processDialogButtonResponse() -> id: %d, label: %s, state: %s\n", event.id, event.label, TftButton::decodeState(event.state));

//     if (event.id == DLG_OK_BUTTON_ID && STREQ("OK", event.label)) {
//         // Get the entered frequency
//         uint32_t newFrequency = event.value;

//         // Do something with the new frequency (e.g., tune the radio)
//         BandTable &currentBand = band.getCurrentBand();
//         currentBand.varData.currFreq = newFrequency;
//         si4735.setFrequency(newFrequency);

//         // RDS törlés
//         pRds->clearRds();
//     }

//     // Call the base class method to close the dialog and redraw the screen
//     DisplayBase::processDialogButtonResponse(event);
// }

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
    char buffer[10];  // Useful to handle string
    sprintf(buffer, "%s", stereo ? "STEREO" : "MONO");
    tft.drawString(buffer, rtv::freqDispX + 210, rtv::freqDispY + 71);
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
