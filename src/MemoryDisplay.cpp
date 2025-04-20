#include "MemoryDisplay.h"

#include "MessageDialog.h"          // Szükséges a törlés megerősítéséhez
#include "VirtualKeyboardDialog.h"  // Szükséges a név szerkesztéséhez
#include "defines.h"                // Szükséges a TFT_COLOR_BACKGROUND-hoz
#include "utils.h"                  // Szükséges a Utils::safeStrCpy-hez

// --- Konstansok ---
#define LIST_X_MARGIN 5
#define LIST_Y_MARGIN 5
#define LIST_ITEM_PADDING_Y 2
#define LIST_ITEM_TEXT_COLOR TFT_WHITE
#define LIST_ITEM_SELECTED_BG_COLOR TFT_BLUE
#define LIST_ITEM_SELECTED_TEXT_COLOR TFT_WHITE
#define LIST_ITEM_BG_COLOR TFT_BLACK  // Vagy TFT_COLOR_BACKGROUND
#define LIST_BORDER_COLOR TFT_WHITE
#define TITLE_TEXT_COLOR TFT_YELLOW

// --- MemoryDisplay Implementáció ---

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

    // Lista területének meghatározása
    uint16_t statusLineHeight = 20;                                         // Becsült magasság
    uint16_t bottomButtonsHeight = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2;  // Vízszintes gombok helye alul

    listX = LIST_X_MARGIN;
    listY = statusLineHeight + LIST_Y_MARGIN;
    listW = tft.width() - (SCRN_BTN_W + SCREEN_VBTNS_X_MARGIN * 2) - (LIST_X_MARGIN * 2);
    listH = tft.height() - listY - bottomButtonsHeight - LIST_Y_MARGIN;

    // Sor magasság és látható sorok számának becslése (Font 2-t használva)
    tft.setTextFont(2);  // Használt font beállítása a méréshez
    tft.setTextSize(1);
    lineHeight = tft.fontHeight() + LIST_ITEM_PADDING_Y * 2;  // Font magasság + padding
    if (lineHeight > 0) {
        visibleLines = listH / lineHeight;
    } else {
        visibleLines = 0;  // Hiba elkerülése
    }
    DEBUG("MemoryDisplay: List Area: x=%d, y=%d, w=%d, h=%d, lineHeight=%d, visibleLines=%d\n", listX, listY, listW, listH, lineHeight, visibleLines);

    // Képernyő gombok definiálása
    DisplayBase::BuildButtonData verticalButtonsData[] = {
        {"Back", TftButton::ButtonType::Pushable},
    };
    buildVerticalScreenButtons(verticalButtonsData, ARRAY_ITEM_COUNT(verticalButtonsData), false);

    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"Save Curr", TftButton::ButtonType::Pushable},
        {"Edit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},    // Kezdetben tiltva
        {"Delete", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},  // Kezdetben tiltva
        {"Tune", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},    // Kezdetben tiltva
    };
    buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), false);

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
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    // dawStatusLine();

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TITLE_TEXT_COLOR, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    const char* title = isFmMode ? "FM Memory" : "AM/LW/SW Memory";
    tft.drawString(title, tft.width() / 2, 2);

    tft.drawRect(listX, listY, listW, listH, LIST_BORDER_COLOR);
    drawStationList();
    drawScreenButtons();
    updateActionButtonsState();  // Gombok állapotának frissítése
}

/**
 * Állomáslista kirajzolása
 */
void MemoryDisplay::drawStationList() {
    tft.fillRect(listX + 1, listY + 1, listW - 2, listH - 2, LIST_ITEM_BG_COLOR);

    uint8_t count = getCurrentStationCount();
    if (count == 0) {
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, LIST_ITEM_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Memory Empty", listX + listW / 2, listY + listH / 2);
        return;
    }

    if (listScrollOffset < 0) listScrollOffset = 0;
    if (listScrollOffset >= count) listScrollOffset = max(0, count - 1);
    if (selectedListIndex >= count) selectedListIndex = -1;

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextPadding(0);

    int yPos = listY + 1 + LIST_ITEM_PADDING_Y;

    for (int i = listScrollOffset; i < count && (i - listScrollOffset) < visibleLines; ++i) {
        const StationData* station = getStationData(i);
        if (!station) continue;

        bool isSelected = (i == selectedListIndex);
        uint16_t bgColor = isSelected ? LIST_ITEM_SELECTED_BG_COLOR : LIST_ITEM_BG_COLOR;
        uint16_t textColor = isSelected ? LIST_ITEM_SELECTED_TEXT_COLOR : LIST_ITEM_TEXT_COLOR;

        tft.fillRect(listX + 1, yPos - LIST_ITEM_PADDING_Y, listW - 2, lineHeight, bgColor);
        tft.setTextColor(textColor, bgColor);

        // Állomásnév
        tft.setTextDatum(TL_DATUM);
        char displayName[STATION_NAME_BUFFER_SIZE];
        strncpy(displayName, station->name, STATION_NAME_BUFFER_SIZE - 1);
        displayName[STATION_NAME_BUFFER_SIZE - 1] = '\0';
        // TODO: Szélesség alapján levágni
        tft.drawString(displayName, listX + 5, yPos);

        // Frekvencia
        String freqStr;
        const BandTable* pBandData = &band.getBandByIdx(station->bandIndex);
        if (pBandData && pBandData->pConstData) {
            if (pBandData->pConstData->bandType == FM_BAND_TYPE) {
                freqStr = String(station->frequency / 100.0f, 2) + " MHz";
            } else {
                freqStr = String(station->frequency) + " kHz";
            }
        } else {
            freqStr = String(station->frequency) + " ?";
        }
        tft.setTextDatum(TR_DATUM);
        tft.drawString(freqStr, listX + listW - 5, yPos);

        yPos += lineHeight;
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
    if (count == 0) return false;

    bool changed = false;

    if (encoderState.direction == RotaryEncoder::Direction::Up) {
        selectedListIndex = (selectedListIndex == -1) ? 0 : min(selectedListIndex + 1, count - 1);
        changed = true;
    } else if (encoderState.direction == RotaryEncoder::Direction::Down) {
        selectedListIndex = (selectedListIndex == -1) ? count - 1 : max(0, selectedListIndex - 1);
        changed = true;
    } else if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        if (selectedListIndex != -1) {
            tuneToSelectedStation();
            return true;
        }
    }

    if (changed) {
        // Scroll offset beállítása
        if (selectedListIndex < listScrollOffset) {
            listScrollOffset = selectedListIndex;
        } else if (selectedListIndex >= listScrollOffset + visibleLines) {
            listScrollOffset = selectedListIndex - visibleLines + 1;
        }
        listScrollOffset = constrain(listScrollOffset, 0, max(0, count - visibleLines));

        drawStationList();
        updateActionButtonsState();  // Gombok frissítése
        return true;
    }

    return false;
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool MemoryDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    uint8_t count = getCurrentStationCount();
    if (count == 0) return false;

    if (touched && tx >= listX && tx < (listX + listW) && ty >= listY && ty < (listY + listH)) {
        int touchedRow = (ty - listY) / lineHeight;
        int touchedIndex = listScrollOffset + touchedRow;

        if (touchedIndex >= 0 && touchedIndex < count) {
            if (touchedIndex != selectedListIndex) {
                selectedListIndex = touchedIndex;
                drawStationList();
                updateActionButtonsState();  // Gombok frissítése
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

    if (STREQ("Save Curr", event.label)) {
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

    bool redrawListNeeded = false;
    bool closeDialog = true;

    if (pDialog) {
        // Billentyűzet válasza
        if (currentDialogPurpose == ActiveDialogPurpose::SAVE_NEW_STATION || currentDialogPurpose == ActiveDialogPurpose::EDIT_STATION_NAME) {
            if (event.id == DLG_OK_BUTTON_ID) {
                // A stationNameBuffer tartalmazza az eredményt

                if (currentDialogPurpose == ActiveDialogPurpose::SAVE_NEW_STATION) {
                    // Új állomás mentése
                    Utils::safeStrCpy(pendingStationData.name, stationNameBuffer.c_str());
                    if (addStationInternal(pendingStationData)) {
                        DEBUG("New station saved successfully: %s\n", pendingStationData.name);
                        redrawListNeeded = true;
                    } else {
                        DEBUG("Failed to save new station.\n");
                        delete pDialog;  // Billentyűzet törlése
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Memory full or station exists!"), "OK");
                        currentDialogPurpose = ActiveDialogPurpose::NONE;
                        closeDialog = false;  // Ne zárja be a MessageDialog-ot
                    }
                } else {  // EDIT_STATION_NAME
                    // Meglévő állomás szerkesztése
                    const StationData* currentStation = getStationData(selectedListIndex);
                    if (currentStation) {
                        StationData updatedStation = *currentStation;
                        Utils::safeStrCpy(updatedStation.name, stationNameBuffer.c_str());
                        if (updateStationInternal(selectedListIndex, updatedStation)) {
                            DEBUG("Station updated successfully at index %d: %s\n", selectedListIndex, updatedStation.name);
                            redrawListNeeded = true;
                        } else {
                            DEBUG("Failed to update station.\n");
                            // Hibaüzenet?
                        }
                    }
                }
            } else {  // Cancel vagy X a billentyűzeten
                DEBUG("Station save/edit cancelled.\n");
            }
        }
        // Törlés megerősítésének válasza
        else if (currentDialogPurpose == ActiveDialogPurpose::DELETE_CONFIRM) {
            if (event.id == DLG_OK_BUTTON_ID) {  // "Delete" gomb
                if (selectedListIndex != -1) {
                    if (deleteStationInternal(selectedListIndex)) {
                        DEBUG("Station deleted successfully.\n");
                        selectedListIndex = -1;
                        redrawListNeeded = true;
                    } else {
                        DEBUG("Failed to delete station.\n");
                        // Hibaüzenet?
                    }
                }
            } else {  // Cancel vagy X a megerősítésen
                DEBUG("Station delete cancelled.\n");
            }
        }
    }

    if (closeDialog) {
        DisplayBase::processDialogButtonResponse(event);  // Bezárja és törli a pDialog-ot, újrarajzolja a képernyőt
        currentDialogPurpose = ActiveDialogPurpose::NONE;

        if (redrawListNeeded) {
            // A Base::processDialogButtonResponse már újrarajzolta a képernyőt,
            // ami a listát is frissítette. De a gombok állapotát lehet, hogy külön kell.
            updateActionButtonsState();
        }
    }
}

/**
 * Esemény nélküli display loop
 */
void MemoryDisplay::displayLoop() {
    // Csak akkor hívjuk a dialógus loopját, ha van dialógus ÉS az egy virtuális billentyűzet
    if (pDialog != nullptr && (currentDialogPurpose == ActiveDialogPurpose::SAVE_NEW_STATION || currentDialogPurpose == ActiveDialogPurpose::EDIT_STATION_NAME)) {
        // Mivel tudjuk, hogy ilyenkor a pDialog egy VirtualKeyboardDialog-ra mutat,
        // biztonságosan hívhatjuk a displayLoop()-ját (ami a DialogBase-ból öröklődik).
        pDialog->displayLoop();
    }
}

// --- Private Helper Methods ---

/**
 * Aktuális állomás mentése dialógussal
 */
void MemoryDisplay::saveCurrentStation() {
    uint8_t count = getCurrentStationCount();
    uint8_t maxCount = isFmMode ? MAX_FM_STATIONS : MAX_AM_STATIONS;

    if (count >= maxCount) {
        DEBUG("Memory full. Cannot save.\n");
        pDialog = new MessageDialog(this, tft, 200, 100, F("Error"), F("Memory Full!"), "OK");
        // pDialog->drawDialog(); // Konstruktor rajzol
        currentDialogPurpose = ActiveDialogPurpose::NONE;  // Nincs aktív cél
        return;
    }

    BandTable& currentBandData = band.getCurrentBand();
    int existingIndex =
        isFmMode ? pFmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx) : pAmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx);

    if (existingIndex != -1) {
        DEBUG("Station already exists.\n");
        pDialog = new MessageDialog(this, tft, 250, 100, F("Info"), F("Station already saved!"), "OK");
        // pDialog->drawDialog(); // Konstruktor rajzol
        currentDialogPurpose = ActiveDialogPurpose::NONE;
        return;
    }

    // Ideiglenes adatok mentése
    pendingStationData.frequency = currentBandData.varData.currFreq;
    pendingStationData.bandIndex = config.data.bandIdx;
    pendingStationData.modulation = currentBandData.varData.currMod;
    // A nevet a billentyűzet után kapjuk meg

    stationNameBuffer = "";  // Ürítjük a buffert

    currentDialogPurpose = ActiveDialogPurpose::SAVE_NEW_STATION;
    selectedListIndex = -1;

    DEBUG("Opening virtual keyboard for new station name...\n");
    pDialog = new VirtualKeyboardDialog(this, tft, F("Enter Station Name"), stationNameBuffer);
}

/**
 * Kiválasztott állomás szerkesztése
 */
void MemoryDisplay::editSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) {
        DEBUG("No station selected for editing.\n");
        return;
    }
    const StationData* station = getStationData(selectedListIndex);
    if (!station) {
        DEBUG("Error getting station data for editing.\n");
        return;
    }

    stationNameBuffer = station->name;  // Kezdeti érték beállítása
    currentDialogPurpose = ActiveDialogPurpose::EDIT_STATION_NAME;

    DEBUG("Opening virtual keyboard for editing station: %s\n", station->name);
    pDialog = new VirtualKeyboardDialog(this, tft, F("Edit Station Name"), stationNameBuffer);
    // pDialog->drawDialog(); // Konstruktor rajzol
}

/**
 * Kiválasztott állomás törlése (megerősítéssel)
 */
void MemoryDisplay::deleteSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) {
        DEBUG("No station selected for deletion.\n");
        return;
    }
    const StationData* station = getStationData(selectedListIndex);
    if (!station) {
        DEBUG("Error getting station data for deletion.\n");
        return;
    }

    currentDialogPurpose = ActiveDialogPurpose::DELETE_CONFIRM;
    String msg = "Delete '" + String(station->name) + "'?";
    DEBUG("Showing delete confirmation dialog for index %d\n", selectedListIndex);
    pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm Delete"), F(msg.c_str()), "Delete", "Cancel");
    // pDialog->drawDialog(); // Konstruktor rajzol
}

/**
 * Behúzza a kiválasztott állomást
 */
void MemoryDisplay::tuneToSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= getCurrentStationCount()) {
        DEBUG("No station selected for tuning.\n");
        return;
    }
    const StationData* station = getStationData(selectedListIndex);
    if (!station) {
        DEBUG("Error getting station data for tuning.\n");
        return;
    }

    DEBUG("Tuning to station: %s (Freq: %d, BandIdx: %d, Mod: %d)\n", station->name, station->frequency, station->bandIndex, station->modulation);

    config.data.bandIdx = station->bandIndex;
    BandTable& targetBand = band.getBandByIdx(station->bandIndex);
    targetBand.varData.currFreq = station->frequency;
    targetBand.varData.currMod = station->modulation;
    band.bandSet(false);

    ::newDisplay = (band.getCurrentBandType() == FM_BAND_TYPE) ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;
}

/**
 * Helper: Visszaadja az aktuális módhoz tartozó állomások számát
 */
uint8_t MemoryDisplay::getCurrentStationCount() const { return isFmMode ? (pFmStore ? pFmStore->getStationCount() : 0) : (pAmStore ? pAmStore->getStationCount() : 0); }

/**
 * Helper: Visszaadja a megadott indexű állomás adatait
 */
const StationData* MemoryDisplay::getStationData(uint8_t index) const {
    return isFmMode ? (pFmStore ? pFmStore->getStationByIndex(index) : nullptr) : (pAmStore ? pAmStore->getStationByIndex(index) : nullptr);
}

/**
 * Helper: Hozzáad egy állomást a megfelelő store-hoz
 */
bool MemoryDisplay::addStationInternal(const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->addStation(station) : false) : (pAmStore ? pAmStore->addStation(station) : false);
}

/**
 * Helper: Frissít egy állomást a megfelelő store-ban
 */
bool MemoryDisplay::updateStationInternal(uint8_t index, const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->updateStation(index, station) : false) : (pAmStore ? pAmStore->updateStation(index, station) : false);
}

/**
 * Helper: Töröl egy állomást a megfelelő store-ból
 */
bool MemoryDisplay::deleteStationInternal(uint8_t index) {
    return isFmMode ? (pFmStore ? pFmStore->deleteStation(index) : false) : (pAmStore ? pAmStore->deleteStation(index) : false);
}
