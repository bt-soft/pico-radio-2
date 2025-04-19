#include "DisplayBase.h"

#include "FrequencyInputDialog.h"
#include "ValueChangeDialog.h"

namespace DisplayConstants {
// Status line méretek és pozíciók
constexpr int StatusLineHeight = 16;
constexpr int StatusLineWidth = 240;
constexpr int StatusLineBfoX = 20;
constexpr int StatusLineAgcX = 60;
constexpr int StatusLineModX = 95;
constexpr int StatusLineBandWidthX = 135;
constexpr int StatusLineBandNameX = 180;
constexpr int StatusLineStepX = 220;
constexpr int StatusLineAntCapX = 260;

// Gombok méretei és margói
constexpr int ButtonWidth = 39;
constexpr int ButtonHeight = 16;
constexpr int ButtonMargin = 5;

// Színek
constexpr uint16_t BfoStepColor = TFT_ORANGE;
constexpr uint16_t AgcColor = TFT_COLOR(255, 130, 0);
constexpr uint16_t StatusLineModeColor = TFT_YELLOW;
constexpr uint16_t StatusLineBandWidthColor = TFT_COLOR(255, 127, 255);  // Magenta
constexpr uint16_t StatusLineBandColor = TFT_CYAN;
constexpr uint16_t StatusLineStepColor = TFT_SKYBLUE;
constexpr uint16_t StatusLineAntCapDefaultColor = TFT_SILVER;  //  Alapértelmezett AntCap szín (szürke)
constexpr uint16_t StatusLineAntCapChangedColor = TFT_GREEN;   // Megváltozott AntCap szín (világoszöld)

// Egyéb
constexpr int VolumeMin = 0;
constexpr int VolumeMax = 63;
}  // namespace DisplayConstants

// Vízszintes gombok definíciói
#define SCREEN_HBTNS_X_START 5    // Horizontális gombok kezdő X koordinátája
#define SCREEN_HBTNS_Y_MARGIN 5   // Horizontális gombok alsó margója
#define SCREEN_BTN_ROW_SPACING 5  // Gombok sorai közötti távolság

// Vertical gombok definíciói
#define SCREEN_VBTNS_X_MARGIN 0  // A vertikális gombok jobb oldali margója

/**
 *  BFO Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawBfoStatus(bool initFont) {
    using namespace DisplayConstants;

    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
    }

    uint8_t currMod = band.getCurrentBand().varData.currMod;

    uint16_t bfoStepColor = TFT_SILVER;
    if ((currMod == LSB || currMod == USB || currMod == CW) && config.data.currentBFOmanu) {
        bfoStepColor = BfoStepColor;
    }
    tft.setTextColor(bfoStepColor, TFT_BLACK);

    // TODO: A BFO-t még ki kell találni
    if (!rtv::bfoOn) {
        tft.drawString(F(" BFO "), StatusLineBfoX, 15);
    }
    tft.drawRect(0, 2, ButtonWidth, ButtonHeight, bfoStepColor);
}

/**
 * AGC / ATT Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawAgcAttStatus(bool initFont) {

    // Fontot kell váltani?
    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
    }

    // AGC / ATT
    uint16_t agcColor = config.data.agcGain == 0 ? TFT_SILVER : DisplayConstants::AgcColor;
    tft.setTextColor(agcColor, TFT_BLACK);
    if (config.data.agcGain > 1) {
        tft.drawString("ATT" + String(config.data.currentAGCgain < 9 ? " " : "") + String(config.data.currentAGCgain), DisplayConstants::StatusLineAgcX, 15);
    } else {
        tft.drawString(F(" AGC "), DisplayConstants::StatusLineAgcX, 15);
    }
    tft.drawRect(40, 2, DisplayConstants::ButtonWidth, DisplayConstants::ButtonHeight, agcColor);
}

/**
 * Lépésköz kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawStepStatus(bool initFont) {
    using namespace DisplayConstants;

    // Fontot kell váltani?
    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
    }
    tft.setTextColor(StatusLineStepColor, TFT_BLACK);
    tft.drawString(band.currentStepSizeStr(), StatusLineStepX, 15);
    tft.drawRect(200, 2, ButtonWidth, ButtonHeight, StatusLineStepColor);
}

/**
 * Antenna Tuning Capacitor Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawAntCapStatus(bool initFont) {

    using namespace DisplayConstants;

    // Fontot kell váltani?
    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
    }

    // Töröljük a területet, mielőtt rajzolunk, pontosan a kocka méretével
    tft.fillRect(StatusLineAntCapX - (ButtonWidth / 2), 0, ButtonWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);

    // Kiírjuk az értéket
    uint16_t currentAntCap = band.getCurrentBand().varData.antCap;
    bool isDefault = (currentAntCap == band.getDefaultAntCapValue());
    uint16_t antCapColor = isDefault ? StatusLineAntCapDefaultColor : StatusLineAntCapChangedColor;
    tft.setTextColor(antCapColor, TFT_BLACK);

    String value;
    if (isDefault) {
        value = F("AntC");  // A String konstruktor tudja kezelni a F() makrót
    } else {
        // Explicit String konverzió a számhoz, majd hozzáfűzés
        value = String(currentAntCap);
        // if (currentAntCap < 1000) {
        value += F("pF");  // F() makró használata a "pF"-hez is memóriatakarékosabb lehet
        //}
    }

    // Kiírjuk az értéket (középre igazítva a téglalaphoz)
    tft.setTextDatum(MC_DATUM);                                          // Középre igazítás
    tft.drawString(value, StatusLineAntCapX, StatusLineHeight / 2 + 2);  // Y pozíció középre
    tft.setTextDatum(BC_DATUM);                                          // Visszaállítás az alapértelmezettre

    // Kirajzoljuk a keretet
    tft.drawRect(StatusLineAntCapX - (ButtonWidth / 2), 2, ButtonWidth, ButtonHeight, antCapColor);
}

/**
 * Státusz sor a képernyő tetején
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::dawStatusLine() {
    using namespace DisplayConstants;

    // tft.fillRect(0, 0, StatusLineWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);

    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);

    // BFO Step
    drawBfoStatus();

    // AGC Status
    drawAgcAttStatus();

    // Demodulációs mód
    tft.setTextColor(StatusLineModeColor, TFT_BLACK);
    const char *modtext = (rtv::CWShift ? "CW" : band.getCurrentBandModeDesc());
    tft.drawString(modtext, StatusLineModX, 15);
    tft.drawRect(80, 2, 29, ButtonHeight, StatusLineModeColor);

    // BandWidth
    tft.setTextColor(StatusLineBandWidthColor, TFT_BLACK);
    String bwText = band.getCurrentBandWidthLabel();
    if (bwText == "AUTO") {
        tft.drawString("F AUTO", StatusLineBandWidthX, 15);
    } else {
        tft.drawString("F" + bwText + "KHz", StatusLineBandWidthX, 15);
    }
    tft.drawRect(110, 2, 49, ButtonHeight, StatusLineBandWidthColor);

    // Band name
    tft.setTextColor(StatusLineBandColor, TFT_BLACK);
    tft.drawString(band.getCurrentBandName(), StatusLineBandNameX, 15);
    tft.drawRect(160, 2, ButtonWidth, ButtonHeight, StatusLineBandColor);

    // Frequency step
    drawStepStatus();

    // Antenna Tuning Capacitor
    drawAntCapStatus();
}

/**
 * Gombok automatikus pozicionálása
 *
 * @param orientation orientáció (Horizontal/Vertical)
 * @param index hányadik gomb?
 * @param isXpos az X pozíciót számoljuk ki?
 */
uint16_t DisplayBase::getAutoButtonPosition(ButtonOrientation orientation, uint8_t index, bool isXpos) {

    if (orientation == ButtonOrientation::Horizontal) {

        if (isXpos) {
            uint8_t buttonsPerRow = tft.width() / (SCRN_BTN_W + SCREEN_BTNS_GAP);
            return SCREEN_HBTNS_X_START + ((SCRN_BTN_W + SCREEN_BTNS_GAP) * (index % buttonsPerRow));

        } else {
            uint8_t buttonsPerRow = tft.width() / (SCRN_BTN_W + SCREEN_BTNS_GAP);
            uint8_t row = index / buttonsPerRow;                                                               // Hányadik sorban van a gomb
            uint8_t rowCount = (horizontalScreenButtonsCount + buttonsPerRow - 1) / buttonsPerRow;             // Összes sor száma
            uint16_t totalHeight = rowCount * (SCRN_BTN_H + SCREEN_BTN_ROW_SPACING) - SCREEN_BTN_ROW_SPACING;  // Az összes sor magassága
            uint16_t startY = tft.height() - totalHeight - SCREEN_HBTNS_Y_MARGIN;                              // A legalsó sor kezdő Y koordinátája
            return startY + row * (SCRN_BTN_H + SCREEN_BTN_ROW_SPACING);                                       // Az adott sor Y koordinátája
        }

    } else {

        // Vertical
        if (isXpos) {
            // Új X koordináta számítás
            uint8_t buttonsPerColumn = tft.height() / (SCRN_BTN_H + SCREEN_BTNS_GAP);
            uint8_t requiredColumns = (verticalScreenButtonsCount + buttonsPerColumn - 1) / buttonsPerColumn;
            uint8_t column = index / buttonsPerColumn;

            // Ha több oszlop van, akkor az oszlopokat jobbra igazítjuk
            if (requiredColumns > 1) {
                // Ha az utolsó oszlopban vagyunk, akkor a képernyő jobb oldalához igazítjuk
                if (column == requiredColumns - 1) {
                    return tft.width() - SCREEN_VBTNS_X_MARGIN - SCRN_BTN_W;
                } else {
                    // Ha nem az utolsó oszlopban vagyunk, akkor a következő oszlop bal oldalához igazítjuk, gap-el
                    return tft.width() - SCREEN_VBTNS_X_MARGIN - SCRN_BTN_W - ((requiredColumns - column - 1) * (SCRN_BTN_W + SCREEN_BTNS_GAP));
                }
            } else {
                // Ha csak egy oszlop van, akkor a jobb oldalra igazítjuk
                return tft.width() - SCREEN_VBTNS_X_MARGIN - SCRN_BTN_W;
            }
        } else {
            // Új Y koordináta számítás
            uint8_t buttonsPerColumn = tft.height() / (SCRN_BTN_H + SCREEN_BTNS_GAP);
            uint8_t row = index % buttonsPerColumn;
            return row * (SCRN_BTN_H + SCREEN_BTNS_GAP);
        }
    }
}

/**
 * Gombok legyártása
 */
TftButton **DisplayBase::buildScreenButtons(ButtonOrientation orientation, BuildButtonData buttonsData[], uint8_t buttonsDataLength, uint8_t startId, uint8_t &buttonsCount) {
    // Dinamikusan létrehozzuk a gombokat
    buttonsCount = buttonsDataLength;

    // Ha nincsenek képernyő gombok, akkor nem megyünk tovább
    if (buttonsCount == 0) {
        return nullptr;
    }

    // Lefoglaljuk a gombok tömbjét
    TftButton **screenButtons = new TftButton *[buttonsCount];

    // Létrehozzuk a gombokat
    for (uint8_t i = 0; i < buttonsCount; i++) {
        screenButtons[i] = new TftButton(startId++,                                     // A gomb ID-je
                                         tft,                                           // TFT objektum
                                         getAutoButtonPosition(orientation, i, true),   // Gomb X koordinátájának kiszámítása
                                         getAutoButtonPosition(orientation, i, false),  // Gomb Y koordinátájának kiszámítása
                                         SCRN_BTN_W,                                    // Gomb szélessége
                                         SCRN_BTN_H,                                    // Gomb magassága
                                         buttonsData[i].label,                          // Gomb szövege (label)
                                         buttonsData[i].type,                           // Gomb típusa
                                         buttonsData[i].state                           // Gomb állapota
        );
    }
    return screenButtons;
}

/**
 * Gombok kirajzolása
 */
void DisplayBase::drawButtons(TftButton **buttons, uint8_t buttonsCount) {
    if (buttons) {
        for (uint8_t i = 0; i < buttonsCount; i++) {
            buttons[i]->draw();
        }
    }
}

/**
 * Képernyő gombok kirajzolása
 */
void DisplayBase::drawScreenButtons() {
    drawButtons(horizontalScreenButtons, horizontalScreenButtonsCount);
    drawButtons(verticalScreenButtons, verticalScreenButtonsCount);

    updateButtonStatus();
}

/**
 * Gombok törlése
 */
void DisplayBase::deleteButtons(TftButton **buttons, uint8_t buttonsCount) {
    if (buttons) {
        for (int i = 0; i < buttonsCount; i++) {
            delete buttons[i];
        }
        delete[] buttons;
    }
}

/**
 * Gombok touch eseményének kezelése
 */
bool DisplayBase::handleButtonTouch(TftButton **buttons, uint8_t buttonsCount, bool touched, uint16_t tx, uint16_t ty) {
    if (buttons) {
        for (uint8_t i = 0; i < buttonsCount; i++) {
            if (buttons[i]->handleTouch(touched, tx, ty)) {
                screenButtonTouchEvent = buttons[i]->buildButtonTouchEvent();
                return true;
            }
        }
    }
    return false;
}

/**
 * Megkeresi a gombot a label alapján a megadott tömbben
 *
 * @param buttons A gombok tömbje
 * @param buttonsCount A gombok száma
 * @param label A keresett gomb label-je
 * @return A TftButton pointere, vagy nullptr, ha nincs ilyen gomb
 */
TftButton *DisplayBase::findButtonInArray(TftButton **buttons, uint8_t buttonsCount, const char *label) {

    if (buttons == nullptr || label == nullptr) {
        return nullptr;
    }

    for (uint8_t i = 0; i < buttonsCount; ++i) {
        if (buttons[i] != nullptr && STREQ(buttons[i]->getLabel(), label)) {
            return buttons[i];
        }
    }

    return nullptr;
}

/**
 * Megkeresi a gombot a label alapján
 *
 * @param label A keresett gomb label-je
 * @return A TftButton pointere, vagy nullptr, ha nincs ilyen gomb
 */
TftButton *DisplayBase::findButtonByLabel(const char *label) {
    // Először a horizontális gombok között keresünk
    TftButton *button = findButtonInArray(horizontalScreenButtons, horizontalScreenButtonsCount, label);
    if (button != nullptr) {
        return button;
    }

    // Ha nem találtuk meg, akkor a vertikális gombok között keresünk
    return findButtonInArray(verticalScreenButtons, verticalScreenButtonsCount, label);
}

/**
 * Vertikális képernyő menügombok legyártása
 *
 * @param buttonsData A gombok adatai
 * @param buttonsDataLength A gombok száma
 * @param isMandatoryNeed Ha true, akkor a kötelező gombokat az elejéhez másolja
 */
void DisplayBase::buildVerticalScreenButtons(BuildButtonData screenVButtonsData[], uint8_t screenVButtonsDataLength, bool isMandatoryNeed) {

    // Kötelező vertikális Képernyőgombok definiálása
    DisplayBase::BuildButtonData mandatoryVButtons[] = {
        {"Mute", TftButton::ButtonType::Toggleable, TFT_TOGGLE_BUTTON_STATE(rtv::muteStat)},         //
        {"Volum", TftButton::ButtonType::Pushable},                                                  //
        {"AGC", TftButton::ButtonType::Toggleable, TFT_TOGGLE_BUTTON_STATE(si4735.isAgcEnabled())},  //
        {"Att", TftButton::ButtonType::Pushable},                                                    //
        {"Freq", TftButton::ButtonType::Pushable},                                                   //
        {"Setup", TftButton::ButtonType::Pushable},                                                  //
    };
    uint8_t mandatoryVButtonsLength = ARRAY_ITEM_COUNT(mandatoryVButtons);

    // Eredmény tömb és hossz változók
    BuildButtonData *mergedButtons = nullptr;
    uint8_t mergedLength = 0;

    if (isMandatoryNeed) {
        // Ha kellenek a kötelező gombok, összefűzzük őket
        mergedLength = mandatoryVButtonsLength + screenVButtonsDataLength;
        mergedButtons = new BuildButtonData[mergedLength];  // Dinamikus allokáció
        Utils::mergeArrays(mandatoryVButtons, mandatoryVButtonsLength, screenVButtonsData, screenVButtonsDataLength, mergedButtons, mergedLength);
    } else {
        // Ha nem kellenek a kötelező gombok, csak a kapottakat használjuk
        mergedLength = screenVButtonsDataLength;
        // Ha vannak egyedi gombok, akkor allokálunk és másolunk
        if (mergedLength > 0) {
            mergedButtons = new BuildButtonData[mergedLength];  // Dinamikus allokáció
            memcpy(mergedButtons, screenVButtonsData, mergedLength * sizeof(BuildButtonData));
        }
        // Ha nincsenek egyedi gombok sem (mergedLength == 0), akkor a mergedButtons nullptr marad
    }

    // Gombok legyártása a (potenciálisan összefűzött) tömb alapján
    // Fontos: A buildScreenButtons most már beállítja a verticalScreenButtonsCount értékét
    verticalScreenButtons = buildScreenButtons(ButtonOrientation::Vertical, mergedButtons, mergedLength, SCRN_VBTNS_ID_START, verticalScreenButtonsCount);

    // Felszabadítjuk a dinamikusan allokált memóriát, ha volt allokálva
    if (mergedButtons != nullptr) {
        delete[] mergedButtons;
    }
}

/**
 * Horizontális képernyő menügombok legyártása
 *
 * @param buttonsData A gombok adatai
 * @param buttonsDataLength A gombok száma
 * @param isMandatoryNeed Ha true, akkor a kötelező gombokat az elejéhez másolja
 */
void DisplayBase::buildHorizontalScreenButtons(BuildButtonData screenHButtonsData[], uint8_t screenHButtonsDataLength, bool isMandatoryNeed) {

    // Kötelező horizontális Képernyőgombok definiálása
    DisplayBase::BuildButtonData mandatoryHButtons[] = {
        {"Ham", TftButton::ButtonType::Pushable},    //
        {"Band", TftButton::ButtonType::Pushable},   //
        {"DeMod", TftButton::ButtonType::Pushable},  //
        {"BndW", TftButton::ButtonType::Pushable, band.getCurrentBand().varData.currMod == FM ? TftButton::ButtonState::Disabled : TftButton::ButtonState::Off},
        {"Step", TftButton::ButtonType::Pushable},  //
        {"Scan", TftButton::ButtonType::Pushable},  //
    };
    uint8_t mandatoryHButtonsLength = ARRAY_ITEM_COUNT(mandatoryHButtons);

    // Eredmény tömb és hossz változók
    BuildButtonData *mergedButtons = nullptr;
    uint8_t mergedLength = 0;

    if (isMandatoryNeed) {
        // Ha kellenek a kötelező gombok, összefűzzük őket
        mergedLength = mandatoryHButtonsLength + screenHButtonsDataLength;
        mergedButtons = new BuildButtonData[mergedLength];  // Dinamikus allokáció
        Utils::mergeArrays(mandatoryHButtons, mandatoryHButtonsLength, screenHButtonsData, screenHButtonsDataLength, mergedButtons, mergedLength);
    } else {
        // Ha nem kellenek a kötelező gombok, csak a kapottakat használjuk
        mergedLength = screenHButtonsDataLength;
        // Ha vannak egyedi gombok, akkor allokálunk és másolunk
        if (mergedLength > 0) {
            mergedButtons = new BuildButtonData[mergedLength];  // Dinamikus allokáció
            memcpy(mergedButtons, screenHButtonsData, mergedLength * sizeof(BuildButtonData));
        }
        // Ha nincsenek egyedi gombok sem (mergedLength == 0), akkor a mergedButtons nullptr marad
    }

    // Gombok legyártása a (potenciálisan összefűzött) tömb alapján
    // Fontos: A buildScreenButtons most már beállítja a horizontalScreenButtonsCount értékét
    horizontalScreenButtons = buildScreenButtons(ButtonOrientation::Horizontal, mergedButtons, mergedLength, SCRN_HBTNS_ID_START, horizontalScreenButtonsCount);

    // Felszabadítjuk a dinamikusan allokált memóriát, ha volt allokálva
    if (mergedButtons != nullptr) {
        delete[] mergedButtons;
    }
}

/**
 * Gombok állapotának frissítése
 */
void DisplayBase::updateButtonStatus() {
    BandTable &currentBand = band.getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;

    // A "Step" gomb SSB/CW módban tiltva van
    TftButton *btnStep = findButtonByLabel("Step");
    if (btnStep != nullptr) {
        bool stepDisabled = (currMod == LSB or currMod == USB or currMod == CW);
        btnStep->setState(stepDisabled ? TftButton::ButtonState::Disabled : TftButton::ButtonState::Off);
    }
}

/**
 *  Minden képernyőn látható közös (kötelező) gombok eseményeinek kezelése
 */
bool DisplayBase::processMandatoryButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    bool processed = false;

    //
    //-- Kötelező függőleges gombok vizsgálata
    //
    if (STREQ("Mute", event.label)) {
        // Némítás
        rtv::muteStat = event.state == TftButton::ButtonState::On;
        Si4735Utils::si4735.setAudioMute(rtv::muteStat);
        processed = true;

    } else if (STREQ("Volum", event.label)) {
        // Hangerő állítása
        this->pDialog = new ValueChangeDialog(this, this->tft, 250, 150, F("Volume"), F("Value:"),                                                              //
                                              &config.data.currVolume, (uint8_t)DisplayConstants::VolumeMin, (uint8_t)DisplayConstants::VolumeMax, (uint8_t)1,  //
                                              [this](uint8_t newValue) { si4735.setVolume(newValue); });
        processed = true;
    } else if (STREQ("AGC", event.label)) {  // Automatikus AGC

        bool stateOn = (event.state == TftButton::ButtonState::On);
        config.data.agcGain = stateOn ? static_cast<uint8_t>(Si4735Utils::AgcGainMode::Automatic) : static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off);

        Si4735Utils::checkAGC();

        // Kijelzés frissítése
        drawAgcAttStatus(true);

        processed = true;

    } else if (STREQ("Att", event.label)) {  // Kézi AGC

        // Kikapcsoljuk az automatikus AGC gombot
        TftButton *agcButton = DisplayBase::findButtonByLabel("AGC");
        if (agcButton != nullptr) {
            agcButton->setState(TftButton::ButtonState::Off);
        }

        // AGCDIS This param selects whether the AGC is enabled or disabled (0 = AGC enabled; 1 = AGC disabled);
        // AGCIDX AGC Index (0 = Minimum attenuation (max gain); 1 – 36 = Intermediate attenuation);
        //  if >greater than 36 - Maximum attenuation (min gain) ).

#define MX_FM_AGC_GAIN 26
#define MX_AM_AGC_GAIN 37
        uint8_t maxValue = si4735.isCurrentTuneFM() ? MX_FM_AGC_GAIN : MX_AM_AGC_GAIN;
        config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Manual);  // 2

        DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("RF Attennuator"), F("Value:"),      //
                                                     &config.data.currentAGCgain, (uint8_t)1, (uint8_t)maxValue, (uint8_t)1,  //
                                                     [this](uint8_t currentAGCgain) {
                                                         si4735.setAutomaticGainControl(1, currentAGCgain);
                                                         DisplayBase::drawAgcAttStatus(true);
                                                     });
        processed = true;

    } else if (STREQ("Freq", event.label)) {
        // Open the FrequencyInputDialog
        BandTable &currentBand = band.getCurrentBand();
        uint16_t currentFreqInternal = currentBand.varData.currFreq;  // A rádió belső formátuma

        DisplayBase::pDialog = new FrequencyInputDialog(this, DisplayBase::tft, band, currentFreqInternal, [this](float freqValue) {
            // Itt kapjuk meg a dialógusból a float értéket (MHz/kHz)
            uint16_t targetFreq;  // Ez lesz kHz vagy 10kHz a rádiónak
            uint8_t bandType = band.getCurrentBandType();
            const char *unitStr = (bandType == FM_BAND_TYPE || bandType == SW_BAND_TYPE) ? "MHz" : "kHz";  // Meghatározzuk a dialógus által használt unitStr-t

            // Konverzió a rádió egységére (ugyanaz a logika, mint ami a dialógusban volt)
            if (bandType == FM_BAND_TYPE) {  // Bevitel MHz, cél 10kHz
                targetFreq = static_cast<uint16_t>(round(freqValue * 100.0));
            } else {                                // Bevitel kHz vagy MHz, cél kHz
                if (strcmp(unitStr, "MHz") == 0) {  // SW MHz bevitel
                    targetFreq = static_cast<uint16_t>(round(freqValue * 1000.0));
                } else {  // AM/LW kHz bevitel
                    targetFreq = static_cast<uint16_t>(round(freqValue));
                }
            }

            // Band adat frissítése és rádió hangolása ITT történik
            BandTable &currentBand = band.getCurrentBand();
            currentBand.varData.currFreq = targetFreq;
            Si4735Utils::si4735.setFrequency(targetFreq);  // Itt használjuk a Si4735Utils::si4735-öt
            DEBUG("Frequency set via Dialog to: %d (%s)\n", targetFreq, (bandType == FM_BAND_TYPE) ? "10kHz" : "kHz");

            // Opcionális: BFO nullázás (ha szükséges)
            // ...

            // Képernyő frissítése (pl. frekvencia kijelző)
            DisplayBase::frequencyChanged = true;  // Jelezzük a fő loopnak a frissítést
            // Vagy közvetlen hívás, ha a DisplayBase-nek van ilyen metódusa
            // updateFrequencyDisplayOnScreen();
        });

        processed = true;  // Jelöljük, hogy kezeltük az eseményt
    }

    else if (STREQ("Setup", event.label)) {              // Beállítások
        ::newDisplay = DisplayBase::DisplayType::setup;  // <<<--- ITT HÍVJUK MEG A changeDisplay-t!
        processed = true;
    }
    //
    //--- Kötelező vízszintes gombok vizsgálata
    //
    else if (STREQ("Ham", event.label)) {

        // Kigyűjtjük a HAM sávok neveit
        uint8_t hamBandCount;
        const char **hamBands = band.getBandNames(hamBandCount, true);

        // Multi button Dialog
        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, 400, 180, F("HAM Radio Bands"), hamBands, hamBandCount,  //
            [this](TftButton::ButtonTouchEvent event) {
                // Átállítjuk a használni kívánt BAND indexet
                config.data.bandIdx = band.getBandIdxByBandName(event.label);

                // Megkeressük, hogy ez FM vagy AM-e és arra állítjuk a display-t
                ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;
            },
            band.getCurrentBand().pConstData->bandName);
        processed = true;

    } else if (STREQ("Band", event.label)) {
        // Kigyűjtjük az összes NEM HAM sáv nevét
        uint8_t bandCount;
        const char **bandNames = band.getBandNames(bandCount, false);

        // Multi button Dialog
        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, 400, 250, F("All Radio Bands"), bandNames, bandCount,
            [this](TftButton::ButtonTouchEvent event) {
                // Átállítjuk a használni kívánt BAND indexet
                config.data.bandIdx = band.getBandIdxByBandName(event.label);

                // Megkeressük, hogy ez FM vagy AM-e és arra állítjuk a display-t
                ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;
            },
            band.getCurrentBand().pConstData->bandName);
        processed = true;

    } else if (STREQ("DeMod", event.label)) {

        // Kigyűjtjük az összes NEM HAM sáv nevét
        uint8_t demodCount;
        const char **demodModes = band.getAmDemodulationModes(demodCount);

        // Multi button Dialog
        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, 270, 150, F("Demodulation Mode"), demodModes, demodCount,  //
            [this](TftButton::ButtonTouchEvent event) {
                // Kikeressük az aktuális band-ot
                BandTable &currentBand = band.getCurrentBand();
                uint8_t newMod = event.id - DLG_MULTI_BTN_ID_START + 1;  // Az FM-et kihagyjuk!

                // // Ha CW módra váltunk, akkor nullázzuk a finomhangolási BFO-t
                if (newMod != CW and rtv::CWShift == true) {
                    currentBand.varData.lastBFO = 0;  // CW_SHIFT_FREQUENCY;
                    config.data.currentBFO = currentBand.varData.lastBFO;
                    rtv::CWShift = false;
                }

                // Átállítjuk a demodulációs módot
                currentBand.varData.currMod = newMod;

                // Újra beállítjuk a sávot az új móddal (false -> ne a preferáltat töltse)
                band.bandSet(false);

                // Státuszsor frissítése az új mód kijelzéséhez
                dawStatusLine();  // Frissíteni kell a státuszsort!
            },
            band.getCurrentBandModeDesc());
        processed = true;

    } else if (STREQ("BndW", event.label)) {

        uint8_t currMod = band.getCurrentBand().varData.currMod;  // Demodulációs mód

        // Megállapítjuk a lehetséges sávszélességek tömbjét
        const __FlashStringHelper *title;
        size_t labelsCount;
        const char **labels;
        uint16_t w = 250;
        uint16_t h = 170;

        if (currMod == FM) {
            title = F("FM Filter in kHz");

            labels = band.getBandWidthLabels(Band::bandWidthFM, labelsCount);

        } else if (currMod == AM) {
            title = F("AM Filter in kHz");
            w = 300;
            h = 180;

            labels = band.getBandWidthLabels(Band::bandWidthAM, labelsCount);

        } else {
            title = F("SSB/CW Filter in kHz");
            w = 300;
            h = 150;

            labels = band.getBandWidthLabels(Band::bandWidthSSB, labelsCount);
        }

        const char *currentBandWidthLabel = band.getCurrentBandWidthLabel();  // Az aktuális sávszélesség felirata

        // Multi button Dialog
        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, w, h, title, labels, labelsCount,  //
            [this](TftButton::ButtonTouchEvent event) {
                // A megnyomott gomb indexének kikeresése
                uint8_t currMod = band.getCurrentBand().varData.currMod;  // Demodulációs mód
                if (currMod == AM) {
                    config.data.bwIdxAM = band.getBandWidthIndexByLabel(Band::bandWidthAM, event.label);
                } else if (currMod == FM) {
                    config.data.bwIdxFM = band.getBandWidthIndexByLabel(Band::bandWidthFM, event.label);
                } else {
                    config.data.bwIdxSSB = band.getBandWidthIndexByLabel(Band::bandWidthSSB, event.label);
                }
                band.bandSet();
            },
            currentBandWidthLabel);  // Az aktuális sávszélesség felirata
        processed = true;

    } else if (STREQ("Step", event.label)) {

        uint8_t currentBandType = band.getCurrentBandType();      // Kikeressük az aktuális Band típust
        uint8_t currMod = band.getCurrentBand().varData.currMod;  // Demodulációs mód

        // Megállapítjuk a lehetséges lépések méretét
        const __FlashStringHelper *title;
        size_t labelsCount;
        const char **labels;
        uint16_t w = 310;
        uint16_t h = 100;

        if (currMod == FM) {
            title = F("Step tune FM");
            labels = band.getStepSizeLabels(Band::stepSizeFM, labelsCount);
        } else {
            title = F("Step tune AM/SSB");
            labels = band.getStepSizeLabels(Band::stepSizeAM, labelsCount);
            w = 250;
            h = 140;
        }

        // Az aktuális freki lépés felirata
        const char *currentStepStr = band.currentStepSizeStr();

        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, w, h, title, labels, labelsCount,  //
            [this](TftButton::ButtonTouchEvent event) {
                // A megnyomott gomb indexe
                uint8_t btnIdx = event.id - DLG_MULTI_BTN_ID_START;

                // Kikeressük az aktuális Band típust
                uint8_t currentBandType = band.getCurrentBandType();

                // Demodulációs mód
                uint8_t currMod = band.getCurrentBand().varData.currMod;

                // Beállítjuk a konfigban a stepSize-t
                if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
                    config.data.ssIdxMW = btnIdx;
                } else if (currMod == FM) {
                    config.data.ssIdxFM = btnIdx;
                } else {
                    config.data.ssIdxAM = btnIdx;
                }
                Si4735Utils::setStep();
            },
            currentStepStr);  // Az aktuális lépés felirata
        processed = true;

    } else if (STREQ("Scan", event.label)) {
        // Képernyő váltás !!!
        ::newDisplay = DisplayBase::DisplayType::freqScan;
        processed = true;
    }

    return processed;
}

/**
 * Konstruktor
 */
DisplayBase::DisplayBase(TFT_eSPI &tft, SI4735 &si4735, Band &band) : Si4735Utils(si4735, band), tft(tft), pDialog(nullptr) {
    DEBUG("DisplayBase::DisplayBase\n");  //
}

/**
 * Destruktor
 */
DisplayBase::~DisplayBase() {

    deleteButtons(horizontalScreenButtons, horizontalScreenButtonsCount);
    horizontalScreenButtons = nullptr;
    horizontalScreenButtonsCount = 0;

    deleteButtons(verticalScreenButtons, verticalScreenButtonsCount);
    verticalScreenButtons = nullptr;
    verticalScreenButtonsCount = 0;

    // Dialóg törlése, ha van
    if (pDialog) {
        delete pDialog;
        pDialog = nullptr;
    }
}

/**
 * Arduino loop hívás (a leszármazott nem írhatja felül)
 *
 * @param encoderState enkóder állapot
 * @return true -> ha volt valalami touch vagy rotary esemény kezelés, a screensavert resetelni kell ilyenkor
 */
bool DisplayBase::loop(RotaryEncoder::EncoderState encoderState) {

    // Az ős loop hívása a squelch kezelésére
    Si4735Utils::loop();

    // Touch adatok változói
    uint16_t tx, ty;
    bool touched = false;

    // Ha van az előző körből feldolgozandó esemény, akkor azzal foglalkozunk először
    if (screenButtonTouchEvent == TftButton::noTouchEvent and dialogButtonResponse == TftButton::noTouchEvent) {

        // Ha nincs feldolgozandó képernyő vagy dialóg gomb esemény, akkor ...

        // Rotary esemény vizsgálata (ha nem tekergetik vagy nem nyomogatják, akkor nem reagálunk rá)
        if (encoderState.buttonState != RotaryEncoder::Open or encoderState.direction != RotaryEncoder::Direction::None) {

            // Ha van dialóg, akkor annak passzoljuk a rotary eseményt, de csak ha van esemény
            if (pDialog) {
                pDialog->handleRotary(encoderState);
            } else {
                // Ha nincs dialóg, akkor a leszármazott képernyőnek, de csak ha van esemény
                this->handleRotary(encoderState);  // Az IGuiEvents interfészből
            }

            // Egyszerre tekergetni vagy gombot nyomogatni nem lehet a Touch-al
            // Ha volt rotary esemény, akkor nem lehet touch, így nem megyünk tovább
            return true;
        }

        //
        // Touch esemény vizsgálata
        //
        touched = tft.getTouch(&tx, &ty, 40);  // A treshold értékét megnöveljük a default 20msec-ről 40-re

        // Ha van dialóg, de még nincs dialogButtonResponse, akkor meghívjuk a dialóg touch handlerét
        if (pDialog != nullptr and dialogButtonResponse == TftButton::noTouchEvent and pDialog->handleTouch(touched, tx, ty)) {

            // Ha ide értünk, akkor be van állítva a dialogButtonResponse
            return true;

        } else if (pDialog == nullptr and screenButtonTouchEvent == TftButton::noTouchEvent) {
            // Ha nincs dialóg, de vannak képernyő gombok és még nincs scrrenButton esemény, akkor azok kapják meg a touch adatokat

            // Elküldjük a touch adatokat a függőleges gomboknak
            if (handleButtonTouch(verticalScreenButtons, verticalScreenButtonsCount, touched, tx, ty)) {
                // Ha volt esemény a függőleges gombokon, akkor nem vizsgáljuk a vízszintes gombokat
            } else {
                // Elküldjük a touch adatokat a vízszintes gomboknak
                handleButtonTouch(horizontalScreenButtons, horizontalScreenButtonsCount, touched, tx, ty);
            }
        }
    }

    // Ha volt screenButton touch event, akkor azt továbbítjuk a képernyőnek
    if (screenButtonTouchEvent != TftButton::noTouchEvent) {

        // Ha a kötelező gombok NEM kezelték le az eseményt, akkor ...
        if (!this->processMandatoryButtonTouchEvent(screenButtonTouchEvent)) {

            // Továbbítjuk a touch eseményt a képernyő gomboknak, hogy ők kezeljék le
            this->processScreenButtonTouchEvent(screenButtonTouchEvent);
        }

        // Töröljük a screenButton eseményt
        screenButtonTouchEvent = TftButton::noTouchEvent;

    } else if (dialogButtonResponse != TftButton::noTouchEvent) {

        // Volt dialóg touch response, feldolgozzuk
        this->processDialogButtonResponse(dialogButtonResponse);

        // Töröljük a dialogButtonResponse eseményt
        dialogButtonResponse = TftButton::noTouchEvent;

    } else if (touched) {
        // Ha nincs screeButton touch event, de nyomtak valamit a képernyőn

        this->handleTouch(touched, tx, ty);  // Az IGuiEvents interfészből

    } else {
        // Semmilyen touch esemény nem volt, meghívjuk a képernyő loop-ját
        this->displayLoop();
    }

    return touched;
}