#include "MemoryDisplay.h"

#include "MessageDialog.h"          // Szükséges a törlés megerősítéséhez
#include "VirtualKeyboardDialog.h"  // Szükséges a név szerkesztéséhez
#include "defines.h"                // Szükséges a TFT_COLOR_BACKGROUND-hoz
#include "utils.h"                  // Szükséges a Utils::safeStrCpy-hez

// --- Konstansok ---
namespace MemoryListConstants {
constexpr int LIST_X_MARGIN = 5;
constexpr int LIST_Y_MARGIN = 5; 
constexpr int ITEM_PADDING_Y = 5;         // MEGNÖVELVE A PADDING, HOGY A LINEHEIGHT BIZTOSAN ELÉG LEGYEN
constexpr int ITEM_TEXT_SIZE_NORMAL = 2;  // Nem kiválasztott elem betűmérete
constexpr uint16_t ITEM_TEXT_COLOR = TFT_WHITE;
constexpr uint16_t ITEM_BG_COLOR = TFT_BLACK;               // Vagy TFT_COLOR_BACKGROUND
constexpr uint16_t SELECTED_ITEM_TEXT_COLOR = TFT_BLACK;    // SetupDisplay-hez hasonlóan
constexpr uint16_t SELECTED_ITEM_BG_COLOR = TFT_LIGHTGREY;  // SetupDisplay-hez hasonlóan
constexpr uint16_t LIST_BORDER_COLOR = TFT_DARKGREY;        // SetupDisplay-hez hasonlóan
constexpr uint16_t TITLE_TEXT_COLOR = TFT_YELLOW;
constexpr uint16_t TUNED_ICON_COLOR = TFT_ORANGE;  // Legyen narancs a jobb láthatóságért
constexpr int ICON_PADDING_RIGHT = 5;
constexpr int MOD_FREQ_GAP = 10;  // Rés a moduláció és a frekvencia között
constexpr int NAME_MOD_GAP = 10;  // Rés a név és a moduláció között
}  // namespace MemoryListConstants

/**
 * Konstruktor
 */
MemoryDisplay::MemoryDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band) : DisplayBase(tft, si4735, band) {  // Base class konstruktor hívása

    DEBUG("MemoryDisplay::MemoryDisplay\n");

    // Aktuális mód meghatározása (FM vagy egyéb)
    isFmMode = (band.getCurrentBandType() == FM_BAND_TYPE);

    // Store pointerek beállítása
    pFmStore = &fmStationStore;  // Globális példányra mutat
    pAmStore = &amStationStore;  // Globális példányra mutat

    using namespace MemoryListConstants;
    // Lista területének meghatározása
    uint16_t statusLineHeight = 20;                                         // Becsült magasság
    uint16_t bottomButtonsHeight = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2;  // Vízszintes gombok helye alul

    listX = LIST_X_MARGIN;
    listY = statusLineHeight + LIST_Y_MARGIN + 15;  // Lejjebb toljuk a listát a cím miatt
    // A lista szélessége most már a teljes képernyő szélességét használhatja (mínusz margók), mert nincs függőleges gombsor
    listW = tft.width() - (LIST_X_MARGIN * 2);
    listH = tft.height() - listY - bottomButtonsHeight - LIST_Y_MARGIN;

    // Sor magasság és látható sorok számának számítása 2-es mérethez ---
    tft.setFreeFont();                                                        // Alap font használata
    tft.setTextSize(ITEM_TEXT_SIZE_NORMAL);                                   // Méret beállítása a méréshez
    lineHeight = tft.fontHeight() + MemoryListConstants::ITEM_PADDING_Y * 2;  // Font magasság + padding
    if (lineHeight > 0) {
        visibleLines = listH / lineHeight;
    } else {
        visibleLines = 0;  // Hiba elkerülése
    }

    // Képernyő gombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"SaveC", TftButton::ButtonType::Pushable},
        {"Edit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},    // Kezdetben tiltva
        {"Delete", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},  // Kezdetben tiltva
        {"Tune", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},    // Kezdetben tiltva
        {"Back", TftButton::ButtonType::Pushable},
    };
    buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), false);

    // "Back" gomb megkeresése
    TftButton* backButton = findButtonByLabel("Back");
    if (backButton != nullptr) {
        // Új X pozíció kiszámítása (jobbra igazítva)
        uint16_t backButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;  // Jobb szélhez igazítva
        // Y pozíció lekérdezése (az automatikus elrendezés már beállította)
        uint16_t backButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, ARRAY_ITEM_COUNT(horizontalButtonsData) - 1, false);  // Y pozíció lekérése az utolsó elemhez

        // Gomb átpozícionálása. A setPosition nem rajzolja újra a gombot, de a drawScreen() majd igen.
        backButton->setPosition(backButtonX, backButtonY);
    }

    // Kezdeti kiválasztás törlése
    selectedListIndex = -1;
    listScrollOffset = 0;
}

/**
 * Destruktor
 */
MemoryDisplay::~MemoryDisplay() {}

/**
 * Képernyő kirajzolása
 */
void MemoryDisplay::drawScreen() {
    using namespace MemoryListConstants;
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    // Cím kiírása (SetupDisplay stílusában)
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);  // FreeFont esetén a méretet a font adja
    tft.setTextColor(TITLE_TEXT_COLOR, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    const char* title = isFmMode ? "FM Memory" : "AM/LW/SW Memory";
    tft.drawString(title, tft.width() / 2, 5);

    // Lista keretének kirajzolása
    // A keretet a lista tényleges tartalma köré rajzoljuk
    tft.drawRect(listX, listY, listW, listH, LIST_BORDER_COLOR);
    drawStationList();
    drawScreenButtons();
    updateActionButtonsState();  // Gombok állapotának frissítése
}

/**
 * Kirajzol egyetlen listaelemet a megadott indexre.
 * Formátum: [>] Név (max MAX_STATION_NAME_LEN )... Mod Freq
 * @param index A kirajzolandó elem indexe a teljes listában.
 */
void MemoryDisplay::drawListItem(int index) {
    using namespace MemoryListConstants;

    // Ellenőrzés, hogy az index érvényes-e és látható-e
    if (index < 0 || index >= getCurrentStationCount() || index < listScrollOffset || index >= listScrollOffset + visibleLines) {
        return;
    }

    const StationData* station = getStationData(index);
    if (!station) return;

    // --- Aktuális hangolási adatok lekérdezése (változatlan) ---
    uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
    uint8_t currentTunedBandIdx = config.data.bandIdx;

    // --- Állapotok meghatározása (változatlan) ---
    bool isSelected = (index == selectedListIndex);
    bool isTuned = (station->frequency == currentTunedFreq && station->bandIndex == currentTunedBandIdx);

    // --- Színek beállítása a kiválasztás alapján (változatlan) ---
    uint16_t bgColor = isSelected ? MemoryListConstants::SELECTED_ITEM_BG_COLOR : MemoryListConstants::ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? MemoryListConstants::SELECTED_ITEM_TEXT_COLOR : MemoryListConstants::ITEM_TEXT_COLOR;

    // Y pozíció kiszámítása a sor tetejéhez
    int yPos = listY + 1 + (index - listScrollOffset) * lineHeight;

    // Háttér rajzolása (csak az adott sor)
    // Explicit módon a bgColor-t használjuk, ami az isSelected alapján dől el.
    // Ez biztosítja, hogy ha egy elemről elnavigálunk, akkor az ITEM_BG_COLOR-ral (fekete) törlődjön.
    // Ha pedig egy elem kiválasztott lesz, akkor a SELECTED_ITEM_BG_COLOR-ral (világosszürke) rajzolódik a háttér.
    tft.fillRect(listX + 1, yPos, listW - 2, lineHeight, bgColor); 

    // --- Font beállítása a kiválasztás alapján ---
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);  // Vastagított font a kiválasztotthoz
        tft.setTextSize(1);                   // FreeFont-hoz általában 1
    } else {
        tft.setFreeFont();  // Alapértelmezett font
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
    }
    tft.setTextPadding(0);                 // Fontos, hogy ne legyen extra padding, ami belerondíthat
    tft.setTextColor(textColor, bgColor);  // Szövegszín beállítása, a háttér a bgColor!

    // Szöveg függőleges középpontja
    int textCenterY = yPos + lineHeight / 2;  // Próbáld meg ezt finomhangolni: yPos + lineHeight / 2 -1; vagy +1;

    // --- Kezdő X pozíció és ikon helyének kiszámítása ---
    int iconStartX = listX + MemoryListConstants::ICON_PADDING_RIGHT;
    int iconWidth = tft.textWidth(">");  // Ennek a fontnak a méretével mér
    int iconSpaceWidth = iconWidth + MemoryListConstants::ICON_PADDING_RIGHT;

    // --- Hangolásjelző ikon kirajzolása (ha szükséges) ---
    if (isTuned) {
        // Az ikonhoz is be kell állítani a fontot/méretet, ha eltérne a labelétől
        // De itt valószínűleg ugyanazt használjuk
        tft.setTextColor(MemoryListConstants::TUNED_ICON_COLOR, bgColor);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(">", iconStartX, textCenterY);
    }

    // --- Állomásnév X pozíciójának beállítása (mindig az ikon helye után) ---
    int textStartX = iconStartX + iconSpaceWidth;

    // --- Állomásnév Előkészítése és Levágása ---
    // Font és méret beállítása a névhez (ha isSelected, akkor bold, egyébként normál)
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
    } else {
        tft.setFreeFont();
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
    }
    tft.setTextColor(textColor, bgColor);  // Szín újra, biztos ami biztos
    tft.setTextDatum(ML_DATUM);

    char displayName[STATION_NAME_BUFFER_SIZE];
    strncpy(displayName, station->name, STATION_NAME_BUFFER_SIZE - 1);
    displayName[STATION_NAME_BUFFER_SIZE - 1] = '\0';

    if (strlen(displayName) > MAX_STATION_NAME_LEN) {
        displayName[MAX_STATION_NAME_LEN] = '\0';
    }

    // --- Frekvencia string előkészítése ---
    String freqStr;
    if (station->modulation == FM) {
        freqStr = String(station->frequency / 100.0f, 2) + " MHz";
    } else if (station->modulation == LSB || station->modulation == USB || station->modulation == CW) {
        uint32_t displayFreqHz = (uint32_t)station->frequency * 1000 - station->bfoOffset;
        long khz_part = displayFreqHz / 1000;
        int hz_tens_part = abs((int)(displayFreqHz % 1000)) / 10;
        char s[12];
        sprintf(s, "%ld.%02d", khz_part, hz_tens_part);
        freqStr = String(s) + " kHz";
    } else {
        freqStr = String(station->frequency) + " kHz";
    }

    // Frekvencia szélességének mérése a KISEBB fonttal
    tft.setFreeFont();
    tft.setTextSize(1);  // Kisebb font az értékhez
    int freqWidth = tft.textWidth(freqStr);

    // Visszaállítás a label fonthoz/mérethez
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
    } else {
        tft.setFreeFont();
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
    }
    tft.setTextColor(textColor, bgColor);  // Szín visszaállítása

    // --- Moduláció string előkészítése ---
    const char* modStr = band.getBandModeDescByIndex(station->modulation);
    // Moduláció szélességének mérése a label aktuális fontjával
    int modWidth = tft.textWidth(modStr);

    // --- Név további levágása az elérhető hely alapján ---
    int availableNameWidth = (listX + listW - MemoryListConstants::ICON_PADDING_RIGHT) - textStartX - modWidth - freqWidth  // Kisebb fonttal mért szélesség
                             - MemoryListConstants::NAME_MOD_GAP - MemoryListConstants::MOD_FREQ_GAP;

    // Font beállítása a displayName levágásához (fontos, hogy ugyanaz legyen, mint a rajzolásnál)
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
    } else {
        tft.setFreeFont();
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
    }
    // A setTextColor itt nem kell, mert csak mérünk

    while (tft.textWidth(displayName) > availableNameWidth && strlen(displayName) > 0) {
        displayName[strlen(displayName) - 1] = '\0';
    }
    int nameWidth = tft.textWidth(displayName);

    // --- Pozíciók kiszámítása ---
    int nameX = textStartX;
    int modX = nameX + nameWidth + MemoryListConstants::NAME_MOD_GAP;
    int freqX = listX + listW - 5;

    // --- Rajzolás ---
    // Név (font és szín már beállítva a méréshez)
    tft.setTextDatum(ML_DATUM);            // Biztosítjuk az igazítást
    tft.setTextColor(textColor, bgColor);  // Szín újra beállítása a rajzoláshoz
    tft.drawString(displayName, nameX, textCenterY);

    // Moduláció (font és szín már beállítva a méréshez)
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(textColor, bgColor);
    tft.drawString(modStr, modX, textCenterY);

    // Frekvencia (kisebb fonttal)
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(textColor, bgColor);  // Fontos a háttérszín itt is!
    tft.setTextDatum(MR_DATUM);
    tft.drawString(freqStr, freqX, textCenterY);

    // Visszaállítás az alapértelmezett szövegbeállításokra a ciklus végén, ha szükséges
    tft.setTextDatum(TL_DATUM);  // Pl. Top-Left
    tft.setFreeFont();           // Alapértelmezett GFX font
    tft.setTextSize(1);          // Alapértelmezett méret
}

/**
 * Állomáslista kirajzolása
 */
void MemoryDisplay::drawStationList() {
    using namespace MemoryListConstants;
    tft.fillRect(listX + 1, listY + 1, listW - 2, listH - 2, ITEM_BG_COLOR);

    uint8_t count = getCurrentStationCount();
    if (count == 0) {
        tft.setFreeFont();
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
        tft.setTextColor(MemoryListConstants::ITEM_TEXT_COLOR, MemoryListConstants::ITEM_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Memory Empty", listX + listW / 2, listY + listH / 2);
        return;
    }

    if (listScrollOffset < 0) listScrollOffset = 0;
    if (listScrollOffset > max(0, count - visibleLines)) listScrollOffset = max(0, count - visibleLines);
    if (selectedListIndex >= count) selectedListIndex = -1;  // Vagy count -1, ha mindig kell lennie kiválasztottnak

    for (int i = 0; i < visibleLines; ++i) {
        int currentItemIndex = listScrollOffset + i;
        if (currentItemIndex < count) {
            drawListItem(currentItemIndex);
        } else {
            // Ha kevesebb elem van, mint a látható hely, a maradékot is töröljük az alap háttérszínnel
            int yPos = listY + 1 + i * lineHeight;
            tft.fillRect(listX + 1, yPos, listW - 2, lineHeight, ITEM_BG_COLOR);
        }
    }
}

/**
 * Az Edit, Delete, Tune gombok állapotának frissítése a kiválasztás alapján
 */
void MemoryDisplay::updateActionButtonsState() {
    bool itemSelected = (selectedListIndex != -1);
    TftButton* editButton = findButtonByLabel("Edit");
    if (editButton) editButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    TftButton* deleteButton = findButtonByLabel("Delete");
    if (deleteButton) deleteButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    TftButton* tuneButton = findButtonByLabel("Tune");
    if (tuneButton) tuneButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
}

/**
 * Rotary encoder esemény lekezelése
 */
bool MemoryDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    uint8_t count = getCurrentStationCount();
    if (count == 0 || pDialog != nullptr) return false;

    bool selectionChanged = false;
    int newSelectedItemIndex = selectedListIndex;

    if (encoderState.direction == RotaryEncoder::Direction::Up) {
        newSelectedItemIndex = (selectedListIndex == -1) ? 0 : min(selectedListIndex + 1, count - 1);
        if (newSelectedItemIndex != selectedListIndex) {
            selectionChanged = true;
        }
    } else if (encoderState.direction == RotaryEncoder::Direction::Down) {
        newSelectedItemIndex = (selectedListIndex == -1) ? count - 1 : max(0, selectedListIndex - 1);
        if (newSelectedItemIndex != selectedListIndex) {
            selectionChanged = true;
        }
    } else if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        if (selectedListIndex != -1) {
            tuneToSelectedStation();
            return true;
        }
    } else if (encoderState.buttonState == RotaryEncoder::ButtonState::DoubleClicked) {
        if (selectedListIndex != -1) {
            editSelectedStation();
            return true;
        }
    }

    if (selectionChanged) {
        int oldSelectedItemIndex = selectedListIndex;
        selectedListIndex = newSelectedItemIndex;
        int oldScrollOffset = listScrollOffset;

        if (selectedListIndex < listScrollOffset) {
            listScrollOffset = selectedListIndex;
        } else if (selectedListIndex >= listScrollOffset + visibleLines) {
            listScrollOffset = selectedListIndex - visibleLines + 1;
        }
        if (listScrollOffset < 0) listScrollOffset = 0;

        if (oldScrollOffset != listScrollOffset) {
            drawStationList();
        } else {
            if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < count && oldSelectedItemIndex >= oldScrollOffset && oldSelectedItemIndex < oldScrollOffset + visibleLines) {
                drawListItem(oldSelectedItemIndex);
            }
            if (selectedListIndex >= 0 && selectedListIndex < count && selectedListIndex >= listScrollOffset && selectedListIndex < listScrollOffset + visibleLines) {
                drawListItem(selectedListIndex);
            }
        }
        updateActionButtonsState();
        return true;
    }
    if (encoderState.direction != RotaryEncoder::Direction::None) {
        return true;
    }
    return false;
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool MemoryDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    uint8_t count = getCurrentStationCount();
    if (count == 0 || pDialog != nullptr) return false;

    if (touched && tx >= listX && tx < (listX + listW) && ty >= listY && ty < (listY + listH)) {
        int touchedRow = (ty - (listY + 1)) / lineHeight;
        int touchedIndex = listScrollOffset + touchedRow;

        if (touchedIndex >= 0 && touchedIndex < count) {
            int oldSelectedItemIndex = selectedListIndex;
            int oldScrollOffset = listScrollOffset;  // Bár érintésnél nem görgetünk, a láthatósághoz kell

            if (touchedIndex != selectedListIndex) {
                selectedListIndex = touchedIndex;
                if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < count && oldSelectedItemIndex >= oldScrollOffset && oldSelectedItemIndex < oldScrollOffset + visibleLines) {
                    drawListItem(oldSelectedItemIndex);
                }
                // Az új kiválasztott elem rajzolása (mivel a selectedListIndex már frissült, ez "kiválasztottként" fogja rajzolni)
                if (selectedListIndex >= 0 && selectedListIndex < count && selectedListIndex >= listScrollOffset && selectedListIndex < listScrollOffset + visibleLines) {
                    drawListItem(selectedListIndex);
                }
                updateActionButtonsState();
            } else {
                static unsigned long lastTouchTime = 0;
                static int lastTouchedIndex = -1;
                if (selectedListIndex == lastTouchedIndex && millis() - lastTouchTime < 500) {
                    tuneToSelectedStation();
                    lastTouchTime = 0;
                    lastTouchedIndex = -1;
                } else {
                    lastTouchTime = millis();
                    lastTouchedIndex = selectedListIndex;
                }
            }
            return true;
        }
    }
    return false;
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void MemoryDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) {
    if (STREQ("SaveC", event.label)) {
        saveCurrentStation();
    } else if (STREQ("Edit", event.label)) {
        editSelectedStation();
    } else if (STREQ("Delete", event.label)) {
        deleteSelectedStation();
    } else if (STREQ("Tune", event.label)) {
        tuneToSelectedStation();
    } else if (STREQ("Back", event.label)) {
        ::newDisplay = prevDisplay;
    }
}

/**
 * Dialóg Button touch esemény feldolgozása
 */
void MemoryDisplay::processDialogButtonResponse(TftButton::ButtonTouchEvent& event) {
    bool closeDialog = true;

    if (pDialog) {
        if (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME) {
            if (event.id == DLG_OK_BUTTON_ID) {
                if (currentDialogMode == DialogMode::SAVE_NEW_STATION) {
                    Utils::safeStrCpy(pendingStationData.name, stationNameBuffer.c_str());
                    if (addStationInternal(pendingStationData)) {
                        selectedListIndex = getCurrentStationCount() - 1;
                        if (selectedListIndex >= listScrollOffset + visibleLines) {
                            listScrollOffset = selectedListIndex - visibleLines + 1;
                        }
                        listScrollOffset = constrain(listScrollOffset, 0, max(0, (int)getCurrentStationCount() - visibleLines));
                    } else {
                        delete pDialog;
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Memory full or station exists!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;
                    }
                } else {  // EDIT_STATION_NAME
                    const StationData* currentStation = getStationData(selectedListIndex);
                    if (currentStation) {
                        StationData updatedStation = *currentStation;
                        Utils::safeStrCpy(updatedStation.name, stationNameBuffer.c_str());
                        if (!updateStationInternal(selectedListIndex, updatedStation)) {
                            delete pDialog;
                            pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Error in modify!"), "OK");
                            currentDialogMode = DialogMode::NONE;
                            closeDialog = false;
                        }
                    }
                }
            }
        } else if (currentDialogMode == DialogMode::DELETE_CONFIRM) {
            if (event.id == DLG_OK_BUTTON_ID) {
                if (selectedListIndex != -1) {
                    if (!deleteStationInternal(selectedListIndex)) {
                        delete pDialog;  // Régi dialógus törlése
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Failed to delete Station!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;
                    } else {
                        selectedListIndex = -1;  // Törlés után nincs kiválasztás
                    }
                }
            }
        }
    }

    if (closeDialog) {
        DisplayBase::processDialogButtonResponse(event);
        currentDialogMode = DialogMode::NONE;
        // A drawScreen() a DisplayBase-ben újrarajzolja a listát is.
        // Csak a gombokat kell frissíteni.
        updateActionButtonsState();
    } else if (pDialog) {       // Ha nem zártuk be a dialógust (hibaüzenet jelent meg)
        pDialog->drawDialog();  // Rajzoljuk újra a hibaüzenet dialógust
    }
}

/**
 * Esemény nélküli display loop
 */
void MemoryDisplay::displayLoop() {
    if (pDialog != nullptr && (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME)) {
        pDialog->displayLoop();
    }
}

// --- Private Helper Methods ---

void MemoryDisplay::saveCurrentStation() {
    uint8_t count = getCurrentStationCount();
    uint8_t maxCount = isFmMode ? MAX_FM_STATIONS : MAX_AM_STATIONS;

    if (count >= maxCount) {
        pDialog = new MessageDialog(this, tft, 200, 100, F("Error"), F("Memory Full!"), "OK");
        currentDialogMode = DialogMode::NONE;
        return;
    }

    BandTable& currentBandData = band.getCurrentBand();
    // A findStation-nek a BFO-t is figyelembe kell vennie SSB/CW esetén
    // Ezt a StationStore::findStation már kezeli, ha a StationData tartalmazza a BFO-t
    int existingIndex =
        isFmMode ? pFmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx) : pAmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx);

    // Pontosabb ellenőrzés SSB/CW esetén a BFO-ra
    if (existingIndex != -1) {
        const StationData* existingStation = getStationData(existingIndex);
        if (existingStation && (currentBandData.varData.currMod == LSB || currentBandData.varData.currMod == USB || currentBandData.varData.currMod == CW)) {
            if (existingStation->bfoOffset != currentBandData.varData.lastBFO) {
                existingIndex = -1;  // Nem ugyanaz az állomás, ha a BFO eltér
            }
        }
    }

    if (existingIndex != -1) {
        pDialog = new MessageDialog(this, tft, 250, 100, F("Info"), F("Station already saved!"), "OK");
        currentDialogMode = DialogMode::NONE;
        return;
    }

    pendingStationData.frequency = currentBandData.varData.currFreq;
    if (currentBandData.varData.currMod == LSB || currentBandData.varData.currMod == USB || currentBandData.varData.currMod == CW) {
        pendingStationData.bfoOffset = currentBandData.varData.lastBFO;
    } else {
        pendingStationData.bfoOffset = 0;
    }
    pendingStationData.bandIndex = config.data.bandIdx;
    pendingStationData.modulation = currentBandData.varData.currMod;

    uint8_t currentMod = currentBandData.varData.currMod;
    if (currentMod == FM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxFM;
    } else if (currentMod == AM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxAM;
    } else {
        pendingStationData.bandwidthIndex = config.data.bwIdxSSB;
    }

    stationNameBuffer = "";
    currentDialogMode = DialogMode::SAVE_NEW_STATION;
    selectedListIndex = -1;

    pDialog = new VirtualKeyboardDialog(this, tft, F("Enter Station Name"), stationNameBuffer);
}

void MemoryDisplay::editSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) return;
    const StationData* station = getStationData(selectedListIndex);
    if (!station) return;

    stationNameBuffer = station->name;
    currentDialogMode = DialogMode::EDIT_STATION_NAME;
    pDialog = new VirtualKeyboardDialog(this, tft, F("Edit Station Name"), stationNameBuffer);
}

void MemoryDisplay::deleteSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) return;
    const StationData* station = getStationData(selectedListIndex);
    if (!station) return;

    currentDialogMode = DialogMode::DELETE_CONFIRM;
    String msg = "Delete '" + String(station->name) + "'?";
    pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm Delete"), F(msg.c_str()), "Delete", "Cancel");
}

void MemoryDisplay::tuneToSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) return;
    const StationData* station = getStationData(selectedListIndex);
    if (!station) return;

    int oldTunedIndex = -1;
    uint16_t oldFreq = band.getCurrentBand().varData.currFreq;
    uint8_t oldBandIdx = config.data.bandIdx;
    uint8_t count = getCurrentStationCount();
    for (int i = 0; i < count; ++i) {
        const StationData* s = getStationData(i);
        if (s && s->frequency == oldFreq && s->bandIndex == oldBandIdx) {
            // SSB/CW esetén a BFO-t is ellenőrizni kellene, ha pontosan ugyanazt az "előzőleg hangolt"
            // elemet akarjuk megtalálni. De a `isTuned` már ezt kezeli.
            oldTunedIndex = i;
            break;
        }
    }

    band.tuneMemoryStation(station->frequency, station->bfoOffset, station->bandIndex, station->modulation, station->bandwidthIndex);
    Si4735Utils::checkAGC();

    // Lista frissítése: csak a régi és az új "tuned" állapotú elemet kell újrarajzolni
    if (oldTunedIndex != -1 && oldTunedIndex != selectedListIndex && oldTunedIndex >= listScrollOffset && oldTunedIndex < listScrollOffset + visibleLines) {
        drawListItem(oldTunedIndex);  // Régi "tuned" most már nem "tuned"
    }
    // Az új "tuned" (ami egyben a selected is) újrarajzolása
    if (selectedListIndex >= listScrollOffset && selectedListIndex < listScrollOffset + visibleLines) {
        drawListItem(selectedListIndex);
    }

    DisplayBase::frequencyChanged = true;
}

uint8_t MemoryDisplay::getCurrentStationCount() const { return isFmMode ? (pFmStore ? pFmStore->getStationCount() : 0) : (pAmStore ? pAmStore->getStationCount() : 0); }

const StationData* MemoryDisplay::getStationData(uint8_t index) const {
    return isFmMode ? (pFmStore ? pFmStore->getStationByIndex(index) : nullptr) : (pAmStore ? pAmStore->getStationByIndex(index) : nullptr);
}

bool MemoryDisplay::addStationInternal(const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->addStation(station) : false) : (pAmStore ? pAmStore->addStation(station) : false);
}

bool MemoryDisplay::updateStationInternal(uint8_t index, const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->updateStation(index, station) : false) : (pAmStore ? pAmStore->updateStation(index, station) : false);
}

bool MemoryDisplay::deleteStationInternal(uint8_t index) {
    return isFmMode ? (pFmStore ? pFmStore->deleteStation(index) : false) : (pAmStore ? pAmStore->deleteStation(index) : false);
}
