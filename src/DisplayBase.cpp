#include "DisplayBase.h"

#include "FrequencyInputDialog.h"
#include "PicoSensorUtils.h"
#include "ValueChangeDialog.h"

namespace DisplayConstants {
// Status line méretek és pozíciók
constexpr int StatusLineRectWidth = 39;
constexpr int StatusLineHeight = 16;
constexpr int StatusLineWidth = 240;

constexpr int StatusLineBfoX = 20;
constexpr int StatusLineAgcX = 60;
constexpr int StatusLineModX = 95;
constexpr int StatusLineBandWidthX = 135;
constexpr int StatusLineBandNameX = 180;
constexpr int StatusLineStepX = 220;
constexpr int StatusLineAntCapX = 260;
constexpr int StatusLineTempX = 300;  // Új pozíció a hőmérsékletnek
constexpr int StatusLineVbusX = 340;  // Új pozíció a Vbus-nak

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
constexpr uint16_t StatusLineTempColor = TFT_YELLOW;           // Hőmérséklet színe
constexpr uint16_t StatusLineVbusColor = TFT_GREENYELLOW;      // Vbus színe

// Egyéb
constexpr int VolumeMin = 0;
constexpr int VolumeMax = 63;
}  // namespace DisplayConstants

/**
 *  BFO Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawBfoStatus(bool initFont) {
    using namespace DisplayConstants;

    tft.fillRect(0, 2, StatusLineRectWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);  // Törlés háttérszínnel

    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
    }

    uint8_t currMod = band.getCurrentBand().varData.currMod;

    uint16_t color = TFT_SILVER;
    if ((currMod == LSB || currMod == USB || currMod == CW) && config.data.currentBFOmanu != 0) {
        color = BfoStepColor;
    }
    tft.setTextColor(color, TFT_BLACK);

    // BFO státusz szöveg előállítás
    char bfoText[10];  // Buffer a szövegnek (pl. "25Hz" vagy " BFO ")
    if (rtv::bfoOn) {
        // Formázzuk a lépésközt és a "Hz"-t a bufferbe
        snprintf(bfoText, sizeof(bfoText), "%dHz", config.data.currentBFOStep);
    } else {
        // Másoljuk a flash memóriában tárolt stringet a bufferbe
        strcpy_P(bfoText, PSTR(" BFO "));
    }
    tft.drawString(bfoText, StatusLineBfoX, 15);
    tft.drawRect(0, 2, StatusLineRectWidth, StatusLineHeight, color);
}

/**
 * AGC / ATT Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik (itt már nem használjuk)
 */
void DisplayBase::drawAgcAttStatus(bool initFont /*= false*/) {  // initFont már nem szükséges itt
    using namespace DisplayConstants;                            // Használjuk a névtér konstansait

    // Font beállítása (mindig beállítjuk a biztonság kedvéért)
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);  // Bottom-Center igazítás

    Si4735Utils::AgcGainMode currentMode = static_cast<Si4735Utils::AgcGainMode>(config.data.agcGain);

    uint16_t agcColor;
    String labelText;

    // Töröljük a korábbi tartalmat (szöveg + téglalap)
    // A téglalap X pozícióját is figyelembe véve törlünk
    int rectX = StatusLineAgcX - (StatusLineRectWidth / 2);
    tft.fillRect(rectX, 0, StatusLineRectWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);  // Törlés háttérszínnel

    if (currentMode == Si4735Utils::AgcGainMode::Manual) {
        // Manual mode (ATT)
        agcColor = AgcColor;  // Használjuk a definiált AgcColor-t ATT-hez is
        // Biztosítunk helyet az egyjegyű számoknak is (pl. "ATT 5")
        labelText = "ATT" + String(config.data.currentAGCgain < 10 ? " " : "") + String(config.data.currentAGCgain);
    } else {
        // Automatic vagy Off mode (AGC)
        agcColor = (currentMode == Si4735Utils::AgcGainMode::Automatic) ? AgcColor : TFT_SILVER;  // AgcColor ha Auto, Silver ha Off
        labelText = F(" AGC ");                                                                   // Flash string használata
    }

    // Szöveg kirajzolása
    tft.setTextColor(agcColor, TFT_BLACK);          // Háttérszín itt már nem kell, mert töröltünk
    tft.drawString(labelText, StatusLineAgcX, 15);  // Y=15 marad

    // Téglalap keret kirajzolása (középre igazítva)
    tft.drawRect(rectX, 2, StatusLineRectWidth, StatusLineHeight, agcColor);
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
    constexpr uint32_t rectX = 200;  // A téglalap kezdő X koordinátája
    tft.fillRect(rectX, 0, StatusLineRectWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(BC_DATUM);  // Bottom-Center igazítás

    tft.setTextColor(StatusLineStepColor, TFT_BLACK);
    tft.drawString(band.currentStepSizeStr(), StatusLineStepX, 15);
    tft.drawRect(rectX, 2, StatusLineRectWidth, StatusLineHeight, StatusLineStepColor);
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
        value += F("pF");  // F() makró használata a "pF"-hez is memóriatakarékosabb lehet
    }

    // Kiírjuk az értéket (középre igazítva a téglalaphoz)
    tft.setTextDatum(MC_DATUM);                                          // Középre igazítás
    tft.drawString(value, StatusLineAntCapX, StatusLineHeight / 2 + 2);  // Y pozíció középre
    tft.setTextDatum(BC_DATUM);                                          // Visszaállítás az alapértelmezettre

    // Kirajzoljuk a keretet
    tft.drawRect(StatusLineAntCapX - (StatusLineRectWidth / 2), 2, StatusLineRectWidth, StatusLineHeight, antCapColor);
}

/**
 * Hőmérséklet Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawTemperatureStatus(bool initFont) {
    using namespace DisplayConstants;

    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
    }

    // Töröljük a területet
    int rectX = StatusLineTempX - (StatusLineRectWidth / 2);
    tft.fillRect(rectX, 0, StatusLineRectWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);

    tft.setTextColor(StatusLineTempColor, TFT_BLACK);
    String tempStr = isnan(lastTemperature) ? "---" : String(lastTemperature, 1);  // 1 tizedesjegy
    tft.drawString(tempStr + "C", StatusLineTempX, 15);

    // Keret
    tft.drawRect(rectX, 2, StatusLineRectWidth, StatusLineHeight, StatusLineTempColor);
}

/**
 * Vbus Status kirajzolása
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::drawVbusStatus(bool initFont) {
    using namespace DisplayConstants;

    if (initFont) {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
    }

    // Töröljük a területet
    int rectX = StatusLineVbusX - (StatusLineRectWidth / 2);
    tft.fillRect(rectX, 0, StatusLineRectWidth, StatusLineHeight, TFT_COLOR_BACKGROUND);

    tft.setTextColor(StatusLineVbusColor, TFT_BLACK);
    String vbusStr = isnan(lastVbus) ? "---" : String(lastVbus, 2);  // 2 tizedesjegy
    tft.drawString(vbusStr + "V", StatusLineVbusX, 15);

    // Keret
    tft.drawRect(rectX, 2, StatusLineRectWidth, StatusLineHeight, StatusLineVbusColor);
}

/**
 * Státusz sor a képernyő tetején
 * @param initFont Ha true, akkor a betűtípus inicializálása történik
 */
void DisplayBase::dawStatusLine() {
    using namespace DisplayConstants;

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
    tft.drawRect(80, 2, 29, StatusLineHeight, StatusLineModeColor);

    // BandWidth
    tft.setTextColor(StatusLineBandWidthColor, TFT_BLACK);
    String bwText = band.getCurrentBandWidthLabel();
    if (bwText == "AUTO") {
        tft.drawString("F AUTO", StatusLineBandWidthX, 15);
    } else {
        tft.drawString("F" + bwText + "KHz", StatusLineBandWidthX, 15);
    }
    tft.drawRect(110, 2, 49, StatusLineHeight, StatusLineBandWidthColor);

    // Band name
    tft.setTextColor(StatusLineBandColor, TFT_BLACK);
    tft.drawString(band.getCurrentBandName(), StatusLineBandNameX, 15);
    tft.drawRect(160, 2, StatusLineRectWidth, StatusLineHeight, StatusLineBandColor);

    // Frequency step
    drawStepStatus();

    // Antenna Tuning Capacitor
    drawAntCapStatus();

    // Hőmérséklet
    drawTemperatureStatus();

    // Vbus
    drawVbusStatus();
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
        for (uint8_t i = 0; i < buttonsCount; i++) {
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
 * Megkeresi a gombot az ID alapján
 *
 * @param id A keresett gomb ID-je
 * @return A TftButton pointere, vagy nullptr, ha nincs ilyen gomb
 */
TftButton *DisplayBase::findButtonById(uint8_t id) {
    // Keresés a horizontális gombok között
    if (horizontalScreenButtons != nullptr) {
        for (uint8_t i = 0; i < horizontalScreenButtonsCount; ++i) {
            if (horizontalScreenButtons[i] != nullptr && horizontalScreenButtons[i]->getId() == id) {
                return horizontalScreenButtons[i];
            }
        }
    }
    // Keresés a vertikális gombok között
    if (verticalScreenButtons != nullptr) {
        for (uint8_t i = 0; i < verticalScreenButtonsCount; ++i) {
            if (verticalScreenButtons[i] != nullptr && verticalScreenButtons[i]->getId() == id) {
                return verticalScreenButtons[i];
            }
        }
    }
    return nullptr;  // Nem található
}

/**
 * Vertikális képernyő menügombok legyártása
 *
 * @param buttonsData A gombok adatai
 * @param buttonsDataLength A gombok száma
 * @param isMandatoryNeed Ha true, akkor a kötelező gombokat az elejéhez másolja
 */
void DisplayBase::buildVerticalScreenButtons(BuildButtonData screenVButtonsData[], uint8_t screenVButtonsDataLength, bool isMandatoryNeed) {

    // Határozzuk meg az AGC gomb kezdeti állapotát és feliratát a config alapján
    Si4735Utils::AgcGainMode initialAgcMode = static_cast<Si4735Utils::AgcGainMode>(config.data.agcGain);

    // Ha Automatikus AGC mód van, akkor a gomb állapota On
    //  Ha Manual módban vagyunk, akkor a gomb állapota On, ha a gain > 1
    TftButton::ButtonState initialAgcButtonState = TftButton::ButtonState::Off;
    if (initialAgcMode == Si4735Utils::AgcGainMode::Automatic) {
        initialAgcButtonState = TftButton::ButtonState::On;
    } else if (initialAgcMode == Si4735Utils::AgcGainMode::Manual) {
        if (config.data.currentAGCgain > 1) {
            initialAgcButtonState = TftButton::ButtonState::On;
        }
    }

    // Kötelező vertikális Képernyőgombok definiálása
    DisplayBase::BuildButtonData mandatoryVButtons[] = {
        {"Mute", TftButton::ButtonType::Toggleable, TFT_TOGGLE_BUTTON_STATE(rtv::muteStat)},
        {"Volum", TftButton::ButtonType::Pushable},
        {"AGC", TftButton::ButtonType::Toggleable, initialAgcButtonState},
        {"Squel", TftButton::ButtonType::Pushable},
        {"Freq", TftButton::ButtonType::Pushable},
        {"Setup", TftButton::ButtonType::Pushable},
        {"Memo", TftButton::ButtonType::Pushable},
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

    // AGC gomb megkeresése és ID mentése
    agcButtonId = TFT_BUTTON_INVALID_ID;  // Reset
    TftButton *agcButton = findButtonInArray(verticalScreenButtons, verticalScreenButtonsCount, "AGC");
    if (agcButton != nullptr) {
        agcButtonId = agcButton->getId();

        // Átállítjuk "Att"-ra, ha Manual módban vagyunk
        if (initialAgcMode == Si4735Utils::AgcGainMode::Manual) {
            agcButton->setLabel("Att");
        }
    } else {
        DEBUG("Error: AGC button not found after creation!\n");
    }

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
        {"Ham", TftButton::ButtonType::Pushable},                                 //
        {"Band", TftButton::ButtonType::Pushable},                                //
        {"DeMod", TftButton::ButtonType::Pushable},                               //
        {"BndW", TftButton::ButtonType::Pushable},                                //
        {"BFO", TftButton::ButtonType::Toggleable, TftButton::ButtonState::Off},  // BFO gomb hozzáadása
        {"Step", TftButton::ButtonType::Pushable},                                //
        {"Scan", TftButton::ButtonType::Pushable},                                //
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

    TftButton *btnSquelch = findButtonByLabel("Squel");
    if (btnSquelch != nullptr) {
        // Ellenőrizzük a squelch értékét
        bool squelchActive = (config.data.currentSquelch > 0);
        // Beállítjuk a gomb állapotát (On, ha aktív, Off, ha nem)
        // A setState() újrarajzolja a gombot a megfelelő LED-del
        btnSquelch->setState(squelchActive ? TftButton::ButtonState::On : TftButton::ButtonState::Off);
    }

    TftButton *btnBndW = findButtonByLabel("BndW");
    if (btnBndW != nullptr) {
        // FM módban a BandWidth gomb tiltva van
        bool bndwDisabled = (currMod == FM);
        btnBndW->setState(bndwDisabled ? TftButton::ButtonState::Disabled : TftButton::ButtonState::Off);
    }

    // BFO gomb állapotának frissítése
    TftButton *btnBfo = findButtonByLabel("BFO");
    if (btnBfo != nullptr) {
        // A gomb csak akkor engedélyezett, ha a mód LSB, USB vagy CW.
        bool bfoDisabled = !(currMod == LSB or currMod == USB or currMod == CW);
        btnBfo->setState(bfoDisabled ? TftButton::ButtonState::Disabled : (rtv::bfoOn ? TftButton::ButtonState::On : TftButton::ButtonState::Off));
    }

    // A "Step" gomb SSB/CW módban csak akkor engedélyezett, ha a BFO be van kapcsolva
    TftButton *btnStep = findButtonByLabel("Step");
    if (btnStep != nullptr) {
        // A gomb csak akkor engedélyezett, ha a BFO be van kapcsolva ÉS a mód LSB/USB/CW.
        // Minden más esetben le van tiltva.
        bool stepDisabled = !(rtv::bfoOn and (currMod == LSB or currMod == USB or currMod == CW));
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

    }
    // AGC/Att gomb kezelése
    else if (event.id == agcButtonId && agcButtonId != TFT_BUTTON_INVALID_ID) {

        // Gomb pointer lekérése
        TftButton *agcButton = findButtonById(agcButtonId);
        if (agcButton == nullptr) {
            DEBUG("Error: AGC button not found by ID %d\n", agcButtonId);
            return false;  // Gomb nem található, nem tudjuk kezelni az eseményt
        }

        // // Határozzuk meg az AGC gomb kezdeti állapotát és feliratát a config alapján
        Si4735Utils::AgcGainMode currAgcMode = static_cast<Si4735Utils::AgcGainMode>(config.data.agcGain);

        // Ha "AGC" a felirat, akkor AGC módban vagyunk
        bool isButtonAgcMode = STREQ("AGC", agcButton->getLabel());

        // --- ÚJ: Hosszú és rövid nyomás megkülönböztetése ---
        if (event.state == TftButton::ButtonState::LongPressed) {
            // --- HOSSZÚ NYOMÁS -> Felirat átállítása AGC <-> Att-ra

            if (isButtonAgcMode) {  // AGC-ben vagyunk -> a gomb feliratának átállítása "Att"-ra
                agcButton->setLabel("Att");

            } else {  // Att-ban vagyunkjk -> átállunk AGC-ra

                // Gomb feliratának átállítása "AGC"-re
                agcButton->setLabel("AGC");

                // Gomb állapotának beállítása
                agcButton->setState(currAgcMode == Si4735Utils::AgcGainMode::Automatic ? TftButton::ButtonState::On : TftButton::ButtonState::Off);
            }

        } else if (event.state == TftButton::ButtonState::On || event.state == TftButton::ButtonState::Off) {
            // --- RÖVID NYOMÁS (Toggle On/Off)

            if (isButtonAgcMode) {
                if (currAgcMode == Si4735Utils::AgcGainMode::Automatic) {
                    config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off);  // AGC OFF
                } else {
                    config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Automatic);  // AGC ON
                }

            } else {

                if (currAgcMode == Si4735Utils::AgcGainMode::Off) {
                    config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Manual);  // Att

                    // 3. ValueChangeDialog megnyitása az Att értékhez
                    constexpr uint8_t MX_FM_AGC_GAIN = 26;
                    constexpr uint8_t MX_AM_AGC_GAIN = 37;

                    uint8_t maxValue = si4735.isCurrentTuneFM() ? MX_FM_AGC_GAIN : MX_AM_AGC_GAIN;
                    DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("RF Attenuator"), F("Value:"), &config.data.currentAGCgain, (uint8_t)1,
                                                                 (uint8_t)maxValue, (uint8_t)1,
                                                                 [this](double currentAGCgain_double) {  // Callback double-t vár
                                                                     uint8_t currentAGCgain = static_cast<uint8_t>(currentAGCgain_double);

                                                                     // Itt már Manual módban vagyunk, csak a szintet kell állítani
                                                                     // si4735.setAutomaticGainControl(1, currentAGCgain);  // AGC disabled, manual index set
                                                                     Si4735Utils::checkAGC();              // Chip beállítása
                                                                     DisplayBase::drawAgcAttStatus(true);  // Státuszsor frissítése
                                                                 });
                } else {
                    config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off);  // AGC OFF
                }
            }

            // Gomb állapotának beállítása
            agcButton->setState(config.data.agcGain != static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off) ? TftButton::ButtonState::On : TftButton::ButtonState::Off);

            Si4735Utils::checkAGC();  // Chip beállítása
            drawAgcAttStatus(true);   // Státuszsor frissítése
        }

        processed = true;

    } else if (STREQ("Squel", event.label)) {
        // Squelch értékének állítása
        const __FlashStringHelper *squelchPrompt;
        if (config.data.squelchUsesRSSI) {
            squelchPrompt = F("RSSI Value[dBuV]:");  // Flash string használata
        } else {
            squelchPrompt = F("SNR Value[dB]:");  // Flash string használata
        }

        // A MIN_SQUELCH és MAX_SQUELCH konstansok az rtVars.h-ban vannak definiálva
        this->pDialog = new ValueChangeDialog(this, this->tft, 250, 150, F("Squelch Level"), squelchPrompt,
                                              &config.data.currentSquelch,  // Pointer a config értékre
                                              (uint8_t)MIN_SQUELCH,         // Minimum érték (rtVars.h-ból)
                                              (uint8_t)MAX_SQUELCH,         // Maximum érték (rtVars.h-ból)
                                              (uint8_t)1,                   // Lépésköz 1
                                              [this](double newValue) {
                                                  // A ValueChangeDialog már beállította a config.data.currentSquelch értékét az OK gomb megnyomásakor vagy a rotary click-re.
                                                  // A Si4735Utils::manageSquelch() a következő ciklusban már az új értéket fogja használni.
                                                  // Itt nincs szükség további explicit műveletre a chip felé, mert a manageSquelch periodikusan fut és olvassa a configot.
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

            // CW shift/BFO nullázás
            currentBand.varData.lastmanuBFO = 0;
            if (currentBand.varData.currMod == CW) {
                currentBand.varData.lastBFO = 0;  // CW_SHIFT_FREQUENCY;
                config.data.currentBFO = currentBand.varData.lastBFO;
                rtv::CWShift = false;  // CWShift állapot visszaállítása
            }

            // Képernyő frissítése (pl. frekvencia kijelző)
            DisplayBase::frequencyChanged = true;  // Jelezzük a fő loopnak a frissítést
        });

        processed = true;  // Jelöljük, hogy kezeltük az eseményt
    }

    else if (STREQ("Setup", event.label)) {              // Beállítások
        ::newDisplay = DisplayBase::DisplayType::setup;  // <<<--- ITT HÍVJUK MEG A changeDisplay-t!
        processed = true;

    } else if (STREQ("Memo", event.label)) {              // Beállítások
        ::newDisplay = DisplayBase::DisplayType::memory;  // <<<--- ITT HÍVJUK MEG A changeDisplay-t!
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
                band.bandSet(false);  // false -> ne a preferált adatokat töltse be
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
        uint16_t w = 200;
        uint16_t h = 130;

        if (rtv::bfoOn) {
            title = F("Step tune BFO");
            labels = band.getStepSizeLabels(Band::stepSizeBFO, labelsCount);

        } else if (currMod == FM) {
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
        DEBUG("currentStepStr: %s\n", currentStepStr);

        DisplayBase::pDialog = new MultiButtonDialog(
            this, DisplayBase::tft, w, h, title, labels, labelsCount,  //
            [this](TftButton::ButtonTouchEvent event) {
                // A megnyomott gomb indexe
                uint8_t btnIdx = event.id - DLG_MULTI_BTN_ID_START;

                // Kikeressük az aktuális Band típust
                uint8_t currentBandType = band.getCurrentBandType();

                // Demodulációs mód
                uint8_t currMod = band.getCurrentBand().varData.currMod;

                if (rtv::bfoOn) {
                    // BFO step állítás
                    config.data.currentBFOStep = band.getStepSizeByIndex(Band::stepSizeBFO, btnIdx);

                } else {
                    // Beállítjuk a konfigban a stepSize-t
                    if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
                        config.data.ssIdxMW = btnIdx;

                    } else if (currMod == FM) {
                        config.data.ssIdxFM = btnIdx;

                    } else {
                        config.data.ssIdxAM = btnIdx;
                    }
                    Si4735Utils::setStep();
                }
            },
            currentStepStr);  // Az aktuális lépés felirata

        processed = true;

    } else if (STREQ("Scan", event.label)) {
        // Képernyő váltás !!!
        ::newDisplay = DisplayBase::DisplayType::freqScan;
        processed = true;

    } else if (STREQ("BFO", event.label)) {

        // Csak SSB/CW módban engedélyezzük
        uint8_t currMod = band.getCurrentBand().varData.currMod;
        if (currMod == LSB or currMod == USB or currMod == CW) {
            rtv::bfoOn = event.state == TftButton::ButtonState::On;  // Állapot beállítása a gomb állapota alapján
            rtv::bfoTr = true;                                       // Animáció indítása

            // Step gomb tiltása/engedélyezése BFO módban
            TftButton *btnStep = findButtonByLabel("Step");
            if (btnStep != nullptr) {
                btnStep->setState(rtv::bfoOn ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
            }
            drawBfoStatus(true);      // BFO állapotának frissítése a státuszsoron
            frequencyChanged = true;  // Kijelző frissítés kérése
            processed = true;

        } else {
            Utils::beepError();  // Hiba hangjelzés, ha nem támogatott módban nyomják meg
        }
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
 * Szenzor adatok frissítése és kijelzése
 */
void DisplayBase::updateSensorReadings() {

    // Szenzor olvasási intervallum (ms)
#define SENSOR_READ_INTERVAL_MS 1000 * 10  // 10 másodperc

    if (lastSensorReadTime == 0 or millis() - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS) {

        lastSensorReadTime = millis();

        lastTemperature = PicoSensorUtils::readCoreTemperature();
        lastVbus = PicoSensorUtils::readVBus();

        // Kijelző frissítése
        drawTemperatureStatus(true);  // true: állítsa be a fontot
        drawVbusStatus(true);         // true: állítsa be a fontot
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

    // Csak rádió módban (AM/FM) mérjük a szenzorokat
    DisplayType displayType = this->getDisplayType();
    if (displayType == DisplayBase::DisplayType::fm || displayType == DisplayBase::DisplayType::am) {
        // Szenzor adatok frissítése
        updateSensorReadings();
    }

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