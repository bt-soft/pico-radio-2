#include "SetupDisplay.h"

#include "InfoDialog.h"
#include "MultiButtonDialog.h"
#include "ValueChangeDialog.h"
#include "utils.h"  // STREQ miatt

// Lista megjelenítési konstansok
namespace SetupListConstants {
constexpr int LIST_START_Y = 45;      // Lejjebb toltuk a listát
constexpr int LIST_AREA_X_START = 5;  // Bal oldali margó a lista területének
constexpr int ITEM_HEIGHT = 30;       // Egy listaelem magassága
constexpr int ITEM_PADDING_X = 10;    // Belső padding a szövegnek a lista területén belül
constexpr int ITEM_TEXT_SIZE = 2;
constexpr uint16_t ITEM_TEXT_COLOR = TFT_WHITE;
constexpr uint16_t ITEM_BG_COLOR = TFT_BLACK;
constexpr uint16_t SELECTED_ITEM_TEXT_COLOR = TFT_BLACK;
constexpr uint16_t SELECTED_ITEM_BG_COLOR = TFT_LIGHTGREY;  // Világosszürke háttér
constexpr uint16_t LIST_BORDER_COLOR = TFT_DARKGREY;        // Keret színe
constexpr uint16_t TITLE_COLOR = TFT_YELLOW;
}  // namespace SetupListConstants

/**
 * Konstruktor
 */
SetupDisplay::SetupDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band) {
    using namespace SetupList;

    // Beállítási lista elemeinek definiálása
    settingItems[0] = {"Brightness", ItemAction::BRIGHTNESS};        // Fényerő
    settingItems[1] = {"Squelch Basis", ItemAction::SQUELCH_BASIS};  // Squelch alapja
    settingItems[2] = {"Screen Saver", ItemAction::SAVER_TIMEOUT};   // Képernyővédő idő
    settingItems[3] = {"Digit Segments", ItemAction::DIGIT_LIGHT};   // Inaktív szegmensek
    settingItems[4] = {"Info", ItemAction::INFO};                    // Információ (most az utolsó)
    itemCount = MAX_SETTINGS;

    selectedItemIndex = 0;
    topItemIndex = 0;

    // Csak az "Exit" gombot hozzuk létre a horizontális gombsorból
    DisplayBase::BuildButtonData exitButtonData[] = {
        {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
    };
    DisplayBase::buildHorizontalScreenButtons(exitButtonData, ARRAY_ITEM_COUNT(exitButtonData), false);

    // Az "Exit" gombot jobbra igazítjuk
    TftButton *exitButton = findButtonByLabel("Exit");
    if (exitButton != nullptr) {
        uint16_t exitButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;
        // Az Y pozíciót az alapértelmezett horizontális elrendezésből vesszük, ami a képernyő alja
        uint16_t exitButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, 0, false);
        exitButton->setPosition(exitButtonX, exitButtonY);
    }
}

/**
 * Destruktor
 */
SetupDisplay::~SetupDisplay() {}

/**
 * Képernyő kirajzolása
 */
void SetupDisplay::drawScreen() {
    using namespace SetupListConstants;
    tft.setFreeFont();
    tft.fillScreen(TFT_BLACK);

    // Cím kiírása
    tft.setFreeFont(&FreeSansBold12pt7b);  // Új bold font a címhez
    tft.setTextColor(TITLE_COLOR, TFT_BLACK);
    tft.setTextSize(1);                              // Alapértelmezett szövegméret
    tft.setTextDatum(TC_DATUM);                      // Felül középre igazítás
    tft.drawString("Settings", tft.width() / 2, 5);  // Fő cím, már bold fonttal

    // Lista keretének kirajzolása
    uint16_t listAreaW = tft.width() - (LIST_AREA_X_START * 2);
    int screenHeight = tft.height();
    int bottomMargin = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5;
    uint16_t listAreaH = screenHeight - LIST_START_Y - bottomMargin;
    // A keretet a lista tényleges tartalma köré rajzoljuk
    tft.drawRect(LIST_AREA_X_START - 1, LIST_START_Y - 1, listAreaW + 2, listAreaH + 2, LIST_BORDER_COLOR);

    // Beállítási lista kirajzolása
    drawSettingsList();

    // "Exit" gomb kirajzolása (a DisplayBase::drawScreenButtons kezeli)
    DisplayBase::drawScreenButtons();
}

/**
 * Beállítási lista kirajzolása
 */
void SetupDisplay::drawSettingsList() {
    using namespace SetupListConstants;
    int yPos = LIST_START_Y;
    int screenHeight = tft.height();
    uint16_t listAreaW = tft.width() - (LIST_AREA_X_START * 2);

    // Az alsó gombok helyének kihagyása a lista magasságából
    int bottomMargin = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5;  // 5px extra hely

    int visibleItems = (screenHeight - LIST_START_Y - bottomMargin) / ITEM_HEIGHT;

    for (int i = 0; i < visibleItems; ++i) {
        int currentItemIndex = topItemIndex + i;
        if (currentItemIndex < itemCount) {
            drawSettingItem(currentItemIndex, yPos, currentItemIndex == selectedItemIndex);
            yPos += ITEM_HEIGHT;
        } else {
            // Ha nincs több elem, üres területet rajzolunk
            tft.fillRect(LIST_AREA_X_START, yPos, listAreaW, ITEM_HEIGHT, ITEM_BG_COLOR);
            yPos += ITEM_HEIGHT;
        }
    }
    // Ha kevesebb elem van, mint a látható hely, a maradékot is töröljük
    if (itemCount < visibleItems) {
        tft.fillRect(LIST_AREA_X_START, LIST_START_Y + itemCount * ITEM_HEIGHT, listAreaW, screenHeight - (LIST_START_Y + itemCount * ITEM_HEIGHT) - bottomMargin, ITEM_BG_COLOR);
    }
}

/**
 * Egy beállítási listaelem kirajzolása
 */
void SetupDisplay::drawSettingItem(int itemIndex, int yPos, bool isSelected) {
    using namespace SetupListConstants;
    uint16_t bgColor = isSelected ? SELECTED_ITEM_BG_COLOR : ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? SELECTED_ITEM_TEXT_COLOR : ITEM_TEXT_COLOR;
    uint16_t listAreaW = tft.width() - (LIST_AREA_X_START * 2);

    // 1. Terület törlése a háttérszínnel
    tft.fillRect(LIST_AREA_X_START, yPos, listAreaW, ITEM_HEIGHT, bgColor);

    // 2. Szöveg tulajdonságainak beállítása
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);  // Bold font a kiválasztott elemhez
        tft.setTextSize(1);                   // Normál szövegméret
    } else {
        tft.setFreeFont();   // Normál font a nem kiválasztott elemhez
        tft.setTextSize(2);  // Normál szövegméret
    }

    tft.setTextColor(textColor, bgColor);
    tft.setTextDatum(ML_DATUM);  // Középre balra

    // 3. A címke (label) kirajzolása
    tft.drawString(settingItems[itemIndex].label, LIST_AREA_X_START + ITEM_PADDING_X, yPos + ITEM_HEIGHT / 2);

    // 4. Az aktuális érték stringjének előkészítése és kirajzolása
    String valueStr = "";
    bool hasValue = true;
    switch (settingItems[itemIndex].action) {
        case SetupList::ItemAction::BRIGHTNESS:
            valueStr = String(config.data.tftBackgroundBrightness);
            break;
        case SetupList::ItemAction::SQUELCH_BASIS:
            valueStr = config.data.squelchUsesRSSI ? "RSSI" : "SNR";
            break;
        case SetupList::ItemAction::SAVER_TIMEOUT:
            valueStr = String(config.data.screenSaverTimeoutMinutes) + " min";
            break;
        case SetupList::ItemAction::DIGIT_LIGHT:
            valueStr = config.data.tftDigitLigth ? "ON" : "OFF";
            break;
        case SetupList::ItemAction::INFO:
        case SetupList::ItemAction::NONE:
        default:
            hasValue = false;  // Az Info-nak és a None-nak nincs megjelenítendő értéke
            break;
    }

    if (hasValue) {
        // Kisebb betűméret beállítása az értékhez
        tft.setFreeFont();   // Visszaváltás alapértelmezett vagy számozott fontra
        tft.setTextSize(1);  // Kisebb betűméret

        // A textColor és bgColor már be van állítva a `isSelected` alapján
        tft.setTextDatum(MR_DATUM);  // Középre jobbra igazítás az értékhez

        // Az érték kirajzolása a sor jobb szélére, belső paddinggel
        tft.drawString(valueStr, LIST_AREA_X_START + listAreaW - ITEM_PADDING_X, yPos + ITEM_HEIGHT / 2);
        // A következő elem rajzolásakor a setTextDatum újra be lesz állítva ML_DATUM-ra a labelhez.
    }
}

/**
 * Kiválasztás frissítése és görgetés kezelése
 */
void SetupDisplay::updateSelection(int newIndex, bool fromRotary) {
    using namespace SetupListConstants;

    // Korai kilépés, ha az index érvénytelen, vagy érintésnél nem változott a kiválasztás
    if (newIndex < 0 || newIndex >= itemCount) return;
    if (!fromRotary && newIndex == selectedItemIndex) return;

    int oldSelectedItemIndex = selectedItemIndex;
    int oldTopItemIndex = topItemIndex;  // Mentsük el a régi topItemIndexet is
    selectedItemIndex = newIndex;

    // Görgetés kezelése
    int screenHeight = tft.height();
    int bottomMargin = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5;
    int visibleItems = (screenHeight - LIST_START_Y - bottomMargin) / ITEM_HEIGHT;
    if (visibleItems <= 0) visibleItems = 1;  // Biztosítjuk, hogy legalább 1 elem látható legyen

    if (selectedItemIndex < topItemIndex) {
        topItemIndex = selectedItemIndex;
    } else if (selectedItemIndex >= topItemIndex + visibleItems) {
        topItemIndex = selectedItemIndex - visibleItems + 1;
    }
    // Biztosítjuk, hogy a topItemIndex ne legyen negatív
    if (topItemIndex < 0) topItemIndex = 0;

    // Csak akkor rajzoljuk újra a teljes listát, ha a lista görgetődött (topItemIndex változott)
    if (oldTopItemIndex != topItemIndex) {
        // akkor a teljes látható listát újra kell rajzolni.
        drawSettingsList();

    } else if (oldSelectedItemIndex != selectedItemIndex) {
        // Ha csak a kiválasztás változott, de a lista nem görgetődött,
        // akkor csak a régi és az új kiválasztott elemet rajzoljuk újra.
        // Biztosítjuk, hogy a régi kiválasztott index érvényes és látható volt
        if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < itemCount && oldSelectedItemIndex >= oldTopItemIndex && oldSelectedItemIndex < oldTopItemIndex + visibleItems) {
            drawSettingItem(oldSelectedItemIndex, LIST_START_Y + (oldSelectedItemIndex - topItemIndex) * ITEM_HEIGHT, false);
        }
        if (selectedItemIndex >= topItemIndex && selectedItemIndex < topItemIndex + visibleItems) {
            drawSettingItem(selectedItemIndex, LIST_START_Y + (selectedItemIndex - topItemIndex) * ITEM_HEIGHT, true);
        }
    }
}

/**
 * Képernyő menügomb esemény feldolgozása (már csak az Exit gombhoz)
 */
void SetupDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {
    if (STREQ("Exit", event.label)) {
        ::newDisplay = prevDisplay;
    }
}

/**
 * Kiválasztott beállítás aktiválása
 */
void SetupDisplay::activateSetting(SetupList::ItemAction action) {
    switch (action) {

        case SetupList::ItemAction::BRIGHTNESS:
            DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("TFT Brightness"), F("Value:"), &config.data.tftBackgroundBrightness,
                                                         (uint8_t)TFT_BACKGROUND_LED_MIN_BRIGHTNESS, (uint8_t)TFT_BACKGROUND_LED_MAX_BRIGHTNESS, (uint8_t)10,
                                                         [this](uint8_t newBrightness) { analogWrite(PIN_TFT_BACKGROUND_LED, newBrightness); });
            break;

        case SetupList::ItemAction::INFO:
            pDialog = new InfoDialog(this, tft, si4735);
            break;

        case SetupList::ItemAction::SQUELCH_BASIS: {
            const char *options[] = {"SNR", "RSSI"};
            uint8_t optionCount = ARRAY_ITEM_COUNT(options);
            const char *currentValue = config.data.squelchUsesRSSI ? "RSSI" : "SNR";
            DisplayBase::pDialog = new MultiButtonDialog(
                this, DisplayBase::tft, 250, 120, F("Squelch Basis"), options, optionCount,
                [this](TftButton::ButtonTouchEvent ev) {
                    if (STREQ(ev.label, "RSSI")) {
                        config.data.squelchUsesRSSI = true;
                    } else if (STREQ(ev.label, "SNR")) {
                        config.data.squelchUsesRSSI = false;
                    }
                },
                currentValue);
        } break;

        case SetupList::ItemAction::SAVER_TIMEOUT:
            DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Screen Saver Timeout"), F("Minutes (1-30):"), &config.data.screenSaverTimeoutMinutes,
                                                         (uint8_t)1, (uint8_t)30, (uint8_t)1, [this](uint8_t newTimeout) {});
            break;

        case SetupList::ItemAction::DIGIT_LIGHT:
            DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Inactive Digit Segments"), F("State:"), &config.data.tftDigitLigth, false, true, true,
                                                         [this](bool newValue) {});
            break;

        case SetupList::ItemAction::NONE:
            break;
    }
}

/**
 * Esemény nélküli display loop
 */
void SetupDisplay::displayLoop() {}

/**
 * Rotary encoder esemény lekezelése
 */
bool SetupDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    if (pDialog) return false;  // Ha dialógus aktív, nem kezeljük

    int newSelectedItemIndex = selectedItemIndex;
    bool selectionChanged = false;

    if (encoderState.direction == RotaryEncoder::Direction::Up) {
        if (selectedItemIndex < itemCount - 1) {
            newSelectedItemIndex = selectedItemIndex + 1;
            selectionChanged = true;
        }
    } else if (encoderState.direction == RotaryEncoder::Direction::Down) {
        if (selectedItemIndex > 0) {
            newSelectedItemIndex = selectedItemIndex - 1;
            selectionChanged = true;
        }
    }

    if (selectionChanged) {
        updateSelection(newSelectedItemIndex, true);
    }

    if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        if (selectedItemIndex >= 0 && selectedItemIndex < itemCount) {
            activateSetting(settingItems[selectedItemIndex].action);
            return true;  // Kezeltük
        }
    }
    return selectionChanged || (encoderState.buttonState != RotaryEncoder::ButtonState::Open);  // Kezeltük, ha volt mozgás vagy gombnyomás
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool SetupDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    using namespace SetupListConstants;
    if (pDialog || !touched) return false;  // Ha dialógus aktív vagy nincs érintés
    uint16_t listAreaW = tft.width() - (LIST_AREA_X_START * 2);

    // Lista területének ellenőrzése
    int screenHeight = tft.height();
    int bottomMargin = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5;
    int listHeight = screenHeight - LIST_START_Y - bottomMargin;

    if (tx >= LIST_AREA_X_START && tx < (LIST_AREA_X_START + listAreaW) && ty >= LIST_START_Y && ty < (LIST_START_Y + listHeight)) {
        int touchedItemRelative = (ty - LIST_START_Y) / ITEM_HEIGHT;
        int touchedItemAbsolute = topItemIndex + touchedItemRelative;

        if (touchedItemAbsolute >= 0 && touchedItemAbsolute < itemCount) {
            updateSelection(touchedItemAbsolute, false);
            activateSetting(settingItems[touchedItemAbsolute].action);
            return true;  // Kezeltük
        }
    }
    return false;  // Nem a listán volt az érintés
}
