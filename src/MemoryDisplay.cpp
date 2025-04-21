#include "MemoryDisplay.h"

#include "MessageDialog.h"          // Szükséges a törlés megerősítéséhez
#include "VirtualKeyboardDialog.h"  // Szükséges a név szerkesztéséhez
#include "defines.h"                // Szükséges a TFT_COLOR_BACKGROUND-hoz
#include "utils.h"                  // Szükséges a Utils::safeStrCpy-hez

// --- Konstansok ---
#define LIST_X_MARGIN 5
#define LIST_Y_MARGIN 5
#define LIST_ITEM_PADDING_Y 3  // Kicsit növeljük a paddinget a nagyobb font miatt
#define LIST_ITEM_TEXT_COLOR TFT_WHITE
#define LIST_ITEM_SELECTED_BG_COLOR TFT_BLUE
#define LIST_ITEM_SELECTED_TEXT_COLOR TFT_WHITE
#define LIST_ITEM_BG_COLOR TFT_BLACK  // Vagy TFT_COLOR_BACKGROUND
#define LIST_BORDER_COLOR TFT_WHITE
#define TITLE_TEXT_COLOR TFT_YELLOW
#define LIST_ITEM_TUNED_ICON_COLOR TFT_YELLOW
#define ICON_PADDING_RIGHT 3
#define MOD_FREQ_GAP 10  // Rés a moduláció és a frekvencia között
#define NAME_MOD_GAP 10  // Rés a név és a moduláció között

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
    // A lista szélessége most már a teljes képernyő szélességét használhatja (mínusz margók), mert nincs függőleges gombsor
    listW = tft.width() - (LIST_X_MARGIN * 2);
    listH = tft.height() - listY - bottomButtonsHeight - LIST_Y_MARGIN;

    // Sor magasság és látható sorok számának számítása 2-es mérethez ---
    tft.setFreeFont();                                        // Alap font használata
    tft.setTextSize(2);                                       // Méret beállítása a méréshez
    lineHeight = tft.fontHeight() + LIST_ITEM_PADDING_Y * 2;  // Font magasság + padding
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
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    tft.setTextFont(2);  // Visszaállítjuk a fontot a címhez (ha szükséges)
    tft.setTextSize(1);
    tft.setTextColor(TITLE_TEXT_COLOR, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    const char* title = isFmMode ? "FM Memory" : "AM/LW/SW Memory";
    tft.drawString(title, tft.width() / 2, 2);  // Státuszsor alá

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
    uint16_t bgColor = isSelected ? LIST_ITEM_SELECTED_BG_COLOR : LIST_ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? LIST_ITEM_SELECTED_TEXT_COLOR : LIST_ITEM_TEXT_COLOR;

    // Y pozíció kiszámítása a sor tetejéhez (változatlan)
    int yPos = listY + 1 + (index - listScrollOffset) * lineHeight;

    // --- Font beállítása (változatlan) ---
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextPadding(0);

    // Háttér rajzolása (csak az adott sor) (változatlan)
    tft.fillRect(listX + 1, yPos, listW - 2, lineHeight, bgColor);

    // Szöveg függőleges középpontja (változatlan)
    int textCenterY = yPos + lineHeight / 2;

    // --- Kezdő X pozíció és ikon helyének kiszámítása (változatlan) ---
    int iconStartX = listX + 5;
    int iconWidth = tft.textWidth(">");
    int iconSpaceWidth = iconWidth + ICON_PADDING_RIGHT;

    // --- Hangolásjelző ikon kirajzolása (ha szükséges) (változatlan) ---
    if (isTuned) {
        tft.setTextColor(LIST_ITEM_TUNED_ICON_COLOR, bgColor);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(">", iconStartX, textCenterY);
    }

    // --- Állomásnév X pozíciójának beállítása (mindig az ikon helye után) (változatlan) ---
    int textStartX = iconStartX + iconSpaceWidth;

    // --- Állomásnév Előkészítése és Levágása ---
    tft.setTextColor(textColor, bgColor);  // Normál szövegszín
    tft.setTextDatum(ML_DATUM);            // Balra igazítás a névhez
    char displayName[STATION_NAME_BUFFER_SIZE];
    strncpy(displayName, station->name, STATION_NAME_BUFFER_SIZE - 1);
    displayName[STATION_NAME_BUFFER_SIZE - 1] = '\0';

    // Levágás MAX_STATION_NAME_LEN karakterre
    if (strlen(displayName) > MAX_STATION_NAME_LEN) {
        displayName[MAX_STATION_NAME_LEN] = '\0';
    }

    // --- Frekvencia string előkészítése (szélesség számításhoz kell) ---
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
    int freqWidth = tft.textWidth(freqStr);

    // --- Moduláció string előkészítése ---
    const char* modStr = band.getBandModeDescByIndex(station->modulation);
    int modWidth = tft.textWidth(modStr);

    // --- Név további levágása az elérhető hely alapján ---
    // Elérhető hely = Teljes szélesség - margók - ikon hely - név kezdete - mod szélesség - freq szélesség - rések
    int availableNameWidth = (listX + listW - 5)  // Jobb margó
                             - textStartX         // Név kezdete (ikon után)
                             - modWidth           // Moduláció helye
                             - freqWidth          // Frekvencia helye
                             - NAME_MOD_GAP       // Rés a név és a mod között
                             - MOD_FREQ_GAP;      // Rés a mod és a freq között

    while (tft.textWidth(displayName) > availableNameWidth && strlen(displayName) > 0) {
        displayName[strlen(displayName) - 1] = '\0';
    }
    int nameWidth = tft.textWidth(displayName);  // Végleges név szélesség

    // --- Pozíciók kiszámítása ---
    int nameX = textStartX;
    int modX = nameX + nameWidth + NAME_MOD_GAP;
    int freqX = listX + listW - 5;  // Jobb szélhez igazítva

    // --- Rajzolás ---
    // Név
    tft.drawString(displayName, nameX, textCenterY);

    // Moduláció
    tft.setTextDatum(ML_DATUM);  // Balra igazítva a rés után
    tft.drawString(modStr, modX, textCenterY);

    // Frekvencia
    tft.setTextDatum(MR_DATUM);  // Jobbra igazítás
    tft.drawString(freqStr, freqX, textCenterY);

    // Visszaállítás, ha szükséges
    tft.setTextDatum(TL_DATUM);  // Vagy ami az alapértelmezett
}

/**
 * Állomáslista kirajzolása
 */
void MemoryDisplay::drawStationList() {
    // Lista területének törlése
    tft.fillRect(listX + 1, listY + 1, listW - 2, listH - 2, LIST_ITEM_BG_COLOR);

    uint8_t count = getCurrentStationCount();
    if (count == 0) {
        // Nagyobb font az üres listához is ---
        tft.setFreeFont();
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, LIST_ITEM_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Memory Empty", listX + listW / 2, listY + listH / 2);
        return;
    }

    // Scroll offset és selected index validálása
    if (listScrollOffset < 0) listScrollOffset = 0;
    if (listScrollOffset > max(0, count - visibleLines)) listScrollOffset = max(0, count - visibleLines);
    if (selectedListIndex >= count) selectedListIndex = -1;

    // --- Ciklus a látható elemek kirajzolásához a drawListItem segítségével ---
    for (int i = listScrollOffset; i < count && (i - listScrollOffset) < visibleLines; ++i) {
        drawListItem(i);  // Meghívjuk az új metódust minden látható sorra
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
    // Ha nincs elem a listában, vagy dialógus van nyitva, nem kezeljük
    if (count == 0 || pDialog != nullptr) return false;

    bool selectionChanged = false;             // Jelzi, ha a kiválasztás változott
    int oldSelectedIndex = selectedListIndex;  // Előző index mentése

    // --- Forgatás kezelése ---
    if (encoderState.direction == RotaryEncoder::Direction::Up) {
        int newIndex = (selectedListIndex == -1) ? 0 : min(selectedListIndex + 1, count - 1);
        if (newIndex != selectedListIndex) {
            selectedListIndex = newIndex;
            selectionChanged = true;
        }
    } else if (encoderState.direction == RotaryEncoder::Direction::Down) {
        int newIndex = (selectedListIndex == -1) ? count - 1 : max(0, selectedListIndex - 1);
        if (newIndex != selectedListIndex) {
            selectedListIndex = newIndex;
            selectionChanged = true;
        }
    }
    // --- Gombnyomás kezelése
    else if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        if (selectedListIndex != -1) {
            tuneToSelectedStation();
            return true;
        }
    }
    // --- Dupla gombnyomás kezelése ---
    else if (encoderState.buttonState == RotaryEncoder::ButtonState::DoubleClicked) {
        // Dupla gombnyomás: Szerkesztés
        if (selectedListIndex != -1) {
            editSelectedStation();
            return true;  // Kezeltük, nem kell tovább menni
        }
    }

    // --- Ha a kiválasztás változott a forgatás miatt ---
    if (selectionChanged) {           // Csak akkor rajzolunk, ha tényleg van változás
        bool needsScrolling = false;  // Jelzi, hogy scrollozni kell-e
        // Ellenőrizzük, kell-e görgetni
        if (selectedListIndex < listScrollOffset) {
            listScrollOffset = selectedListIndex;
            needsScrolling = true;
        } else if (selectedListIndex >= listScrollOffset + visibleLines) {
            listScrollOffset = selectedListIndex - visibleLines + 1;
            needsScrolling = true;
        }

        if (needsScrolling) {
            // Ha görgetni kellett, a teljes listát újrarajzoljuk
            drawStationList();
        } else {
            // Ha nem kellett görgetni, csak a két érintett sort rajzoljuk újra
            drawListItem(oldSelectedIndex);   // Régi (ha volt és látható volt)
            drawListItem(selectedListIndex);  // Új
        }

        updateActionButtonsState();  // Gombok frissítése mindenképp
        return true;                 // Kezeltük az eseményt
    }

    // Ha csak gombnyomás volt, vagy nem változott a kiválasztás, de volt tekerés
    if (encoderState.direction != RotaryEncoder::Direction::None) {
        return true;  // Kezeltük, de nem rajzolunk
    }

    return false;  // Nem kezeltük
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool MemoryDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    uint8_t count = getCurrentStationCount();
    if (count == 0 || pDialog != nullptr) return false;  // Dialógus alatt sem kezeljük

    if (touched && tx >= listX && tx < (listX + listW) && ty >= listY && ty < (listY + listH)) {
        // Pontosabb sor index számítás
        int touchedRow = (ty - (listY + 1)) / lineHeight;  // A kereten belüli relatív Y / sor magasság
        int touchedIndex = listScrollOffset + touchedRow;

        if (touchedIndex >= 0 && touchedIndex < count) {
            int oldSelectedIndex = selectedListIndex;  // Régi index mentése

            if (touchedIndex != selectedListIndex) {
                selectedListIndex = touchedIndex;

                // Csak a két érintett sort rajzoljuk újra (itt nem kell görgetni)
                drawListItem(oldSelectedIndex);   // Régi (ha volt és látható volt)
                drawListItem(selectedListIndex);  // Új

                updateActionButtonsState();  // Gombok frissítése
            }

            // Dupla kattintás hangoláshoz (opcionális, egyszerűsített)
            static unsigned long lastTouchTime = 0;
            static int lastTouchedIndex = -1;
            if (touchedIndex == lastTouchedIndex && millis() - lastTouchTime < 500) {  // 500ms-on belül ugyanoda
                tuneToSelectedStation();
                lastTouchTime = 0;  // Reset dupla kattintás után
                lastTouchedIndex = -1;
            } else {
                lastTouchTime = millis();
                lastTouchedIndex = touchedIndex;
            }
            return true;  // Kezeltük az érintést
        }
    } else if (!touched) {
        // Ha az érintés véget ér a listán kívül, reseteljük a dupla kattintás figyelőt
        // lastTouchTime = 0; // Ezt lehet, hogy nem itt kellene
        // lastTouchedIndex = -1;
    }
    return false;  // Nem kezeltük
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

    bool redrawListNeeded = false;
    bool closeDialog = true;

    if (pDialog) {
        // Billentyűzet válasza
        if (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME) {
            if (event.id == DLG_OK_BUTTON_ID) {
                // A stationNameBuffer tartalmazza az eredményt

                if (currentDialogMode == DialogMode::SAVE_NEW_STATION) {
                    // Új állomás mentése
                    Utils::safeStrCpy(pendingStationData.name, stationNameBuffer.c_str());
                    if (addStationInternal(pendingStationData)) {
                        redrawListNeeded = true;
                        // Opcionális: Ugrás az új elemre a listában
                        selectedListIndex = getCurrentStationCount() - 1;
                        // Scroll offset beállítása, hogy látható legyen
                        if (selectedListIndex >= listScrollOffset + visibleLines) {
                            listScrollOffset = selectedListIndex - visibleLines + 1;
                        }
                        listScrollOffset = constrain(listScrollOffset, 0, max(0, (int)getCurrentStationCount() - visibleLines));

                    } else {
                        delete pDialog;  // Billentyűzet törlése
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Memory full or station exists!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;  // Ne zárja be a MessageDialog-ot
                    }
                } else {  // EDIT_STATION_NAME
                    // Meglévő állomás szerkesztése
                    const StationData* currentStation = getStationData(selectedListIndex);
                    if (currentStation) {
                        StationData updatedStation = *currentStation;
                        Utils::safeStrCpy(updatedStation.name, stationNameBuffer.c_str());
                        if (updateStationInternal(selectedListIndex, updatedStation)) {
                            redrawListNeeded = true;
                        } else {
                            delete pDialog;  // Billentyűzet törlése
                            pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Error in modify!"), "OK");
                            currentDialogMode = DialogMode::NONE;
                            closeDialog = false;  // Ne zárja be a MessageDialog-ot
                        }
                    }
                }
            } else {  // Cancel vagy X a billentyűzeten
                // DEBUG("Station name edit cancelled.\n");
            }
        }
        // Törlés megerősítésének válasza
        else if (currentDialogMode == DialogMode::DELETE_CONFIRM) {
            if (event.id == DLG_OK_BUTTON_ID) {  // "Delete" gomb
                if (selectedListIndex != -1) {
                    int indexToDelete = selectedListIndex;  // Mentsük el az indexet
                    if (deleteStationInternal(indexToDelete)) {
                        selectedListIndex = -1;  // Törlés után nincs kiválasztás
                        redrawListNeeded = true;
                    } else {
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Failed to delete Station!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;  // Ne zárja be a MessageDialog-ot
                    }
                }
            } else {  // Cancel vagy X a megerősítésen
                // DEBUG("Station delete cancelled.\n");
            }
        }
    }

    if (closeDialog) {
        DisplayBase::processDialogButtonResponse(event);  // Bezárja és törli a pDialog-ot, újrarajzolja a képernyőt
        currentDialogMode = DialogMode::NONE;

        // A Base::processDialogButtonResponse már újrarajzolta a képernyőt, ami a listát is frissítette. A gombokat is frissíteni kell.
        updateActionButtonsState();
    }
}

/**
 * Esemény nélküli display loop
 */
void MemoryDisplay::displayLoop() {
    // Csak akkor hívjuk a dialógus loopját, ha van dialógus ÉS az egy virtuális billentyűzet
    if (pDialog != nullptr && (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME)) {
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
        currentDialogMode = DialogMode::NONE;  // Nincs aktív cél
        return;
    }

    BandTable& currentBandData = band.getCurrentBand();
    int existingIndex =
        isFmMode ? pFmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx) : pAmStore->findStation(currentBandData.varData.currFreq, config.data.bandIdx);

    if (existingIndex != -1) {
        DEBUG("Station already exists.\n");
        pDialog = new MessageDialog(this, tft, 250, 100, F("Info"), F("Station already saved!"), "OK");
        currentDialogMode = DialogMode::NONE;
        return;
    }

    // Ideiglenes adatok mentése
    pendingStationData.frequency = currentBandData.varData.currFreq;
    pendingStationData.bandIndex = config.data.bandIdx;
    pendingStationData.modulation = currentBandData.varData.currMod;

    // Releváns sávszélesség index mentése az aktuális mód alapján
    uint8_t currentMod = currentBandData.varData.currMod;
    if (currentMod == FM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxFM;
    } else if (currentMod == AM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxAM;
    } else {  // LSB, USB, CW
        pendingStationData.bandwidthIndex = config.data.bwIdxSSB;
    }

    // A nevet a billentyűzet után kapjuk meg

    stationNameBuffer = "";  // Ürítjük a buffert

    currentDialogMode = DialogMode::SAVE_NEW_STATION;
    selectedListIndex = -1;

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
    currentDialogMode = DialogMode::EDIT_STATION_NAME;

    DEBUG("Opening virtual keyboard for editing station: %s\n", station->name);
    pDialog = new VirtualKeyboardDialog(this, tft, F("Edit Station Name"), stationNameBuffer);
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

    currentDialogMode = DialogMode::DELETE_CONFIRM;
    String msg = "Delete '" + String(station->name) + "'?";
    DEBUG("Showing delete confirmation dialog for index %d\n", selectedListIndex);
    pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm Delete"), F(msg.c_str()), "Delete", "Cancel");
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

    // --- Régi hangolt elem indexének megkeresése (a hangolás *előtt*) ---
    int oldTunedIndex = -1;
    uint16_t oldFreq = band.getCurrentBand().varData.currFreq;
    uint8_t oldBandIdx = config.data.bandIdx;
    uint8_t count = getCurrentStationCount();
    for (int i = 0; i < count; ++i) {
        const StationData* s = getStationData(i);
        if (s && s->frequency == oldFreq && s->bandIndex == oldBandIdx) {
            oldTunedIndex = i;
            break;
        }
    }
    // --- Régi index keresés vége ---

    // --- Hangolás beállítása a Band osztályban---
    band.tuneMemoryStation(station->frequency, station->bandIndex, station->modulation, station->bandwidthIndex);

    // --- Lista frissítése ---
    if (oldTunedIndex != -1 && oldTunedIndex != selectedListIndex) {
        drawListItem(oldTunedIndex);
    }
    drawListItem(selectedListIndex);
    // --- Lista frissítés vége ---

    // Jelezzük a fő loopnak, hogy a frekvencia (és mód) változott,
    // hogy a többi kijelző (pl. státuszsor) is frissüljön, ha visszalépünk.
    DisplayBase::frequencyChanged = true;
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
