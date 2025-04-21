#include "SetupDisplay.h"

#include "MultiButtonDialog.h"
#include "ValueChangeDialog.h"

/**
 * Konstruktor
 */
SetupDisplay::SetupDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band) {

    // Horizontális képernyőgombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"Bright", TftButton::ButtonType::Pushable},                             //
        {"Squelch", TftButton::ButtonType::Pushable},                            //
        {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},  //
    };

    // Horizontális képernyőgombok legyártása
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), false);

    // Az "Exit" gombot továbbra is jobbra igazíthatjuk
    TftButton *exitButton = findButtonByLabel("Exit");
    if (exitButton != nullptr) {
        uint16_t exitButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;
        uint16_t exitButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, ARRAY_ITEM_COUNT(horizontalButtonsData) - 1, false);
        exitButton->setPosition(exitButtonX, exitButtonY);
    }
}

/**
 * Destruktor
 */
SetupDisplay::~SetupDisplay() {}

/**
 * Képernyő kirajzolása
 * A ScreenSaver rögtön a képernyő törlésével kezd
 */
void SetupDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_BLACK);
    tft.setTextFont(2);

    // Gombok kirajzolása
    DisplayBase::drawScreenButtons();

    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);  // Középre igazítás
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Setup display", tft.width() / 2, tft.height() / 2);
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void SetupDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    DEBUG("SetupDisplay::processScreenButtonTouchEvent() -> id: %d, label: %s, state: %s\n", event.id, event.label, TftButton::decodeState(event.state));

    if (STREQ("Exit", event.label)) {
        ::newDisplay = prevDisplay;  // <<<--- ITT HÍVJUK MEG A changeDisplay-t!
    } else if (STREQ("Bright", event.label)) {
        DisplayBase::pDialog =
            new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("TFT Brightness"), F("Value:"),                                                                         //
                                  &config.data.tftBackgroundBrightness, (uint8_t)TFT_BACKGROUND_LED_MIN_BRIGHTNESS, (uint8_t)TFT_BACKGROUND_LED_MAX_BRIGHTNESS, (uint8_t)10,  //
                                  [this](uint8_t newBrightness) { analogWrite(PIN_TFT_BACKGROUND_LED, newBrightness); });
    }
    // --- ÚJ RÉSZ KEZDETE ---
    else if (STREQ("Squelch", event.label)) {
        // Squelch alapjának kiválasztása (SNR vagy RSSI)
        const char *options[] = {"SNR", "RSSI"};
        uint8_t optionCount = ARRAY_ITEM_COUNT(options);
        const char *currentValue = config.data.squelchUsesRSSI ? "RSSI" : "SNR";  // Aktuális érték meghatározása

        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, 250, 120,  // Méretet igazítsd szükség szerint
            F("Squelch Basis"), options, optionCount,
            [this](TftButton::ButtonTouchEvent event) {
                // A callback függvény, ami lefut, ha a dialógusban gombot nyomnak
                if (STREQ(event.label, "RSSI")) {
                    config.data.squelchUsesRSSI = true;
                    DEBUG("Squelch basis set to RSSI\n");
                } else if (STREQ(event.label, "SNR")) {
                    config.data.squelchUsesRSSI = false;
                    DEBUG("Squelch basis set to SNR\n");
                }
                // Nincs szükség a képernyő újrarajzolására itt,
                // mert a DialogBase::processDialogButtonResponse megteszi.
                // A config mentése automatikusan megtörténik később.
            },
            currentValue  // Az aktuálisan kiválasztott érték átadása a dialógusnak
        );
    }
}

/**
 * Esemény nélküli display loop - ScreenSaver futtatása
 * Nem kell figyelni a touch eseményt, azt már a főprogram figyeli és leállítja/törli a ScreenSaver-t
 */
void SetupDisplay::displayLoop() {}

/**
 * Rotary encoder esemény lekezelése
 */
bool SetupDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) { return false; }

/**
 * Touch (nem képrnyő button) esemény lekezelése
 * A további gui elemek vezérléséhez
 */
bool SetupDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; }
