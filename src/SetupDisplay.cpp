#include "SetupDisplay.h"

#include "InfoDialog.h"
#include "MultiButtonDialog.h"
#include "ValueChangeDialog.h"
#include "defines.h"

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
SetupDisplay::SetupDisplay(TFT_eSPI &tft_ref, SI4735 &si4735_ref, Band &band_ref)
    : DisplayBase(tft_ref, si4735_ref, band_ref),
      scrollListComponent(tft_ref, SetupListConstants::LIST_AREA_X_START, SetupListConstants::LIST_START_Y, tft_ref.width() - (SetupListConstants::LIST_AREA_X_START * 2),
                          tft_ref.height() - SetupListConstants::LIST_START_Y - (SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5),
                          this),   // dataSource ez a SetupDisplay példány
      nestedDialogOpened(false) {  // Új jelzőbit inicializálása
    using namespace SetupList;

    // Beállítási lista elemeinek definiálása
    settingItems[0] = {"Brightness", ItemAction::BRIGHTNESS};                         // Fényerő
    settingItems[1] = {"Squelch Basis", ItemAction::SQUELCH_BASIS};                   // Squelch alapja
    settingItems[2] = {"Screen Saver", ItemAction::SAVER_TIMEOUT};                    // Képernyővédő idő
    settingItems[3] = {"Inactive Digit Segments", ItemAction::INACTIVE_DIGIT_LIGHT};  // Inaktív szegmensek
    settingItems[4] = {"Beeper", ItemAction::BEEPER_ENABLED};                         // Beeper engedélyezése (4)
    settingItems[5] = {"FFT Config (AM)", ItemAction::FFT_CONFIG_AM};                 // ÚJ, összevont
    settingItems[6] = {"FFT Config (FM)", ItemAction::FFT_CONFIG_FM};
    settingItems[7] = {"Info", ItemAction::INFO};                    // Információ
    settingItems[8] = {"Factory Reset", ItemAction::FACTORY_RESET};  // Gyári beállítások visszaállítása (5)

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
    tft.setFreeFont(&FreeSansBold12pt7b);  // Bold font a címhez
    tft.setTextColor(TITLE_COLOR, TFT_BLACK);
    tft.setTextSize(1);                              // Alapértelmezett szövegméret
    tft.setTextDatum(TC_DATUM);                      // Felül középre igazítás
    tft.drawString("Settings", tft.width() / 2, 5);  // Fő cím, már bold fonttal

    // Lista keretének kirajzolása

    // A keretet a lista tényleges tartalma köré rajzoljuk
    int bottomMargin = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2 + 5;
    uint16_t listAreaH = tft.height() - LIST_START_Y - bottomMargin;
    uint16_t listAreaW = tft.width() - (LIST_AREA_X_START * 2);
    tft.drawRect(LIST_AREA_X_START - 1, LIST_START_Y - 1, listAreaW + 2, listAreaH + 2, LIST_BORDER_COLOR);

    // Beállítási lista kirajzolása a komponens segítségével
    scrollListComponent.refresh();  // Adatok betöltése és rajzolás

    // "Exit" gomb kirajzolása (a DisplayBase::drawScreenButtons kezeli)
    DisplayBase::drawScreenButtons();
}

/**
 * Egy beállítási listaelem kirajzolása
 */
void SetupDisplay::drawListItem(TFT_eSPI &tft_ref, int itemIndex, int x, int y, int w, int h, bool isSelected) {
    using namespace SetupListConstants;
    uint16_t bgColor = isSelected ? SELECTED_ITEM_BG_COLOR : ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? SELECTED_ITEM_TEXT_COLOR : ITEM_TEXT_COLOR;

    // 1. Terület törlése a háttérszínnel
    // A kiválasztott elem háttérrajzolási paramétereinek módosítása a 2px-es margóhoz
    int bgX = x;
    int bgY = y;
    int bgW = w;
    int bgH = h;

    // Kis margót hagyunk a kiválasztott elem körül
    if (isSelected) {
        bgX += 4;
        bgY += 4;
        bgW -= 4;
        bgH -= 4;
    }
    tft_ref.fillRect(bgX, bgY, bgW, bgH, bgColor);

    // 2. Szöveg tulajdonságainak beállítása
    tft_ref.setFreeFont(&FreeSansBold9pt7b);
    tft_ref.setTextColor(textColor, bgColor);
    tft_ref.setTextDatum(ML_DATUM);  // Középre balra

    // 3. A címke (label) kirajzolása
    tft_ref.drawString(settingItems[itemIndex].label, x + ITEM_PADDING_X, y + h / 2);

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
        case SetupList::ItemAction::INACTIVE_DIGIT_LIGHT:
            valueStr = config.data.tftDigitLigth ? "ON" : "OFF";
            break;
        case SetupList::ItemAction::BEEPER_ENABLED:
            valueStr = config.data.beeperEnabled ? "ON" : "OFF";  // Flash string
            break;
        case SetupList::ItemAction::FFT_CONFIG_AM: {
            float val = config.data.miniAudioFftConfigAm;
            if (val == -1.0f)
                valueStr = "Disabled";
            else if (val == 0.0f)
                valueStr = "Auto Gain";
            else
                valueStr = "Manual: " + String(val, 1) + "x";
        } break;
        case SetupList::ItemAction::FFT_CONFIG_FM: {
            float val = config.data.miniAudioFftConfigFm;
            if (val == -1.0f)
                valueStr = "Disabled";
            else if (val == 0.0f)
                valueStr = "Auto Gain";
            else
                valueStr = "Manual: " + String(val, 1) + "x";
        } break;
        case SetupList::ItemAction::INFO:
        case SetupList::ItemAction::FACTORY_RESET:
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
        tft_ref.drawString(valueStr, x + w - ITEM_PADDING_X, y + h / 2);
        // A következő elem rajzolásakor a setTextDatum újra be lesz állítva ML_DATUM-ra a labelhez.
    }
}

// IScrollableListDataSource implementációk
int SetupDisplay::getItemCount() const { return MAX_SETTINGS; }

void SetupDisplay::activateListItem(int index) {
    if (index >= 0 && index < MAX_SETTINGS) {
        activateSetting(settingItems[index].action, index);
    }
}

int SetupDisplay::getItemHeight() const { return SetupListConstants::ITEM_HEIGHT; }

int SetupDisplay::loadData() {
    // Statikus adatok, itt nincs szükség specifikus betöltési műveletre a SetupDisplay esetén
    return -1;  // Nincs specifikus elem, amit ki kellene választani
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
void SetupDisplay::activateSetting(SetupList::ItemAction action, int itemIndex) {
    switch (action) {

        case SetupList::ItemAction::BRIGHTNESS:
            DisplayBase::pDialog =
                new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("TFT Brightness"), F("Value:"), &config.data.tftBackgroundBrightness,
                                      (uint8_t)TFT_BACKGROUND_LED_MIN_BRIGHTNESS, (uint8_t)TFT_BACKGROUND_LED_MAX_BRIGHTNESS, (uint8_t)10, [this](uint8_t newBrightness) {
                                          // Be is állítjuk a háttérvilágítást
                                          analogWrite(PIN_TFT_BACKGROUND_LED, newBrightness);
                                      });
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
                                                         (uint8_t)1, (uint8_t)30, (uint8_t)1, [this](uint8_t newTimeout) {
                                                             // Nincs szükség további műveletre.
                                                         });
            break;

        case SetupList::ItemAction::INACTIVE_DIGIT_LIGHT:
            DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Inactive Digit Segments"), F("State:"), &config.data.tftDigitLigth, false, true, true,
                                                         [this](bool newValue) {
                                                             // Nincs szükség további műveletre.
                                                         });
            break;

        case SetupList::ItemAction::BEEPER_ENABLED:
            DisplayBase::pDialog =
                new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Beeper"), F("State:"), &config.data.beeperEnabled, false, true, true, [this](bool newValue) {
                    // A ValueChangeDialog már beállította a config.data.beeperEnabled értékét.
                    // Ha az új érték true, csipogunk egyet.
                    if (newValue) {
                        Utils::beepTick();
                    }
                });
            break;

        case SetupList::ItemAction::FFT_CONFIG_AM:
        case SetupList::ItemAction::FFT_CONFIG_FM: {
            bool isAm = (action == SetupList::ItemAction::FFT_CONFIG_AM);
            const char *dialogTitle = isAm ? "FFT Config (AM)" : "FFT Config (FM)";
            float *targetConfigField = isAm ? &config.data.miniAudioFftConfigAm : &config.data.miniAudioFftConfigFm;

            const char *currentActiveFFTLabel = nullptr;
            const char *options[] = {"Disabled", "Auto G", "Manu G"};
            if (*targetConfigField == -1.0f) {
                currentActiveFFTLabel = options[0];  // "Disabled"
            } else if (*targetConfigField == 0.0f) {
                currentActiveFFTLabel = options[1];  // "Auto Gain"
            } else if (*targetConfigField > 0.0f) {
                currentActiveFFTLabel = options[2];  // "Set Manual..."
            }
            DisplayBase::pDialog = new MultiButtonDialog(
                this, DisplayBase::tft, 280, 80, F(dialogTitle), options, 3,
                [this, targetConfigField, itemIndex](TftButton::ButtonTouchEvent ev) {  // Lambda callback
                    // A MultiButtonDialog gombjainak eseményei
                    if (STREQ(ev.label, "Disabled")) {
                        *targetConfigField = -1.0f;

                    } else if (STREQ(ev.label, "Auto G")) {
                        *targetConfigField = 0.0f;

                    } else if (STREQ(ev.label, "Manu G")) {

                        // Új dialógus a manuális értékhez
                        float currentManualGain = (*targetConfigField > 0.0f) ? *targetConfigField : 1.0f;  // Alapértelmezett, ha nem volt manuális
                        this->pendingCloseDialog = DisplayBase::pDialog; // Elmentjük a MultiButtonDialog-ot törlésre
                        DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Set Manual FFT Gain"), F("Factor (0.1-10.0):"),
                                                                     // A pDialog-ot felülírjuk, a MultiButtonDialog NEM záródik be azonnal a return miatt
                                                                     &currentManualGain,                                             // float*
                                                                     0.1f,                                                           // min
                                                                     10.0f,                                                          // max
                                                                     0.1f,                                                           // step
                                                                     [this, targetConfigField, itemIndex](double newValue_double) {  // Lambda for ValueChangeDialog
                                                                         *targetConfigField = static_cast<float>(newValue_double);
                                                                         // A lista frissítése a ValueChangeDialog bezárása után történik
                                                                         // a processDialogButtonResponse-ban, ami meghívja a drawScreen()-t.
                                                                     });
                        this->nestedDialogOpened = true;  // Jelezzük, hogy beágyazott dialógus nyílt
                        return;                           // Ne zárja be a MultiButtonDialog-ot még, a ValueChangeDialog nyílik
                    }
                    // A lista frissítése a MultiButtonDialog bezárása után történik
                    // a processDialogButtonResponse-ban, ami meghívja a drawScreen()-t.
                },
                currentActiveFFTLabel  // currentActiveButtonLabel
            );
        } break;

        case SetupList::ItemAction::FACTORY_RESET:
            // Megerősítő dialógus megnyitása
            pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm"), F("Reset to factory defaults?"), "Yes", "No");
            break;

        case SetupList::ItemAction::INFO:
            pDialog = new InfoDialog(this, tft, si4735);
            break;

        case SetupList::ItemAction::NONE:
            break;
    }
}

/**
 * Dialógus gomb eseményének feldolgozása
 */
void SetupDisplay::processDialogButtonResponse(TftButton::ButtonTouchEvent &event) {
    // Ha egy beágyazott dialógus nyílt meg (pl. a ValueChangeDialog a MultiButtonDialog-ból),
    // akkor az eredeti MultiButtonDialog válaszát nem szabad úgy feldolgozni,
    // hogy az bezárja az újonnan megnyitott ValueChangeDialog-ot.
    if (nestedDialogOpened) {
        nestedDialogOpened = false;  // Jelzőbit visszaállítása
        if (pendingCloseDialog) {
            delete pendingCloseDialog;
            pendingCloseDialog = nullptr;
        }
        // Nem hívjuk meg a DisplayBase::processDialogButtonResponse-t,
        // hogy ne záródjon be a pDialog (ami most a ValueChangeDialog).
        return;
    }

    // Csak ha van dialóg
    if (pDialog) {
        // Dialógus címének lekérése csak egyszer
        const char *dialogTitle = pDialog->getTitle();

        // Factory Reset dialógus specifikus kezelése
        if (dialogTitle && strcmp_P(PSTR("Confirm"), dialogTitle) == 0) {
            if (event.id == DLG_OK_BUTTON_ID) {  // "Yes" gomb
                config.loadDefaults();           // Alapértelmezett konfig betöltése + beállítása
                config.checkSave();              // Mentés indítása (ez menti az EEPROM-ba)
                Utils::beepTick();               // Visszajelzés
            }
        }
        // Más dialógusok (pl. ValueChangeDialog, MultiButtonDialog, InfoDialog) esetén
        // az értékek frissítését maga a dialógus vagy annak callback függvénye kezeli
        // mielőtt ez a metódus meghívódna. Itt csak a bezárásukról és a képernyő
        // frissítéséről kell gondoskodnunk.
    }

    // Minden dialógus esetén (beleértve a "Confirm"-ot is a specifikus logika után)
    // meghívjuk az ősosztály metódusát a dialógus bezárásához és a képernyő újrarajzolásához.
    DisplayBase::processDialogButtonResponse(event);
}

/**
 * Rotary encoder esemény lekezelése
 */
bool SetupDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {

    if (pDialog) return false;  // Ha dialógus aktív, nem kezeljük

    bool scrolled = scrollListComponent.handleRotaryScroll(encoderState);

    if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        int currentSelection = scrollListComponent.getSelectedItemIndex();
        if (currentSelection != -1) {
            // activateSetting(settingItems[currentSelection].action); // Közvetlen hívás
            scrollListComponent.activateSelectedItem();  // Komponensen keresztül
            return true;                                 // Kezeltük
        }
    }

    // Kezeltük, ha volt mozgás vagy gombnyomás (kivéve ha csak Open volt a gomb)
    return scrolled || (encoderState.buttonState != RotaryEncoder::ButtonState::Open);
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool SetupDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    using namespace SetupListConstants;
    if (pDialog) return false;

    return scrollListComponent.handleTouch(touched, tx, ty, true);  // activateOnTouch = true
}
