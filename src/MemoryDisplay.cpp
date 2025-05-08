#include "MemoryDisplay.h"

#include <algorithm>  // std::sort használatához
#include <vector>     // std::vector használatához

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
constexpr int MOD_FREQ_GAP = 10;            // Rés a moduláció és a frekvencia között
constexpr int NAME_MOD_GAP = 10;            // Rés a név és a moduláció között
constexpr char CURRENT_TUNED_ICON[] = ">";  // Ikon a behangolt állomás jelzésére
}  // namespace MemoryListConstants

/**
 * Konstruktor
 */
MemoryDisplay::MemoryDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band) : DisplayBase(tft, si4735, band) {  // Base class konstruktor hívása

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
    listW = tft.width() - (LIST_X_MARGIN * 2);
    listH = tft.height() - listY - bottomButtonsHeight - LIST_Y_MARGIN;

    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextSize(1);
    lineHeight = tft.fontHeight() + MemoryListConstants::ITEM_PADDING_Y * 2;
    tft.setFreeFont();
    tft.setTextSize(ITEM_TEXT_SIZE_NORMAL);

    if (lineHeight > 0) {
        visibleLines = listH / lineHeight;
    } else {
        visibleLines = 0;
    }

    // Képernyő gombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"SaveC", TftButton::ButtonType::Pushable},
        {"Edit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},
        {"Delete", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},
        {"Tune", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},
        {"Back", TftButton::ButtonType::Pushable},
    };
    buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), false);

    TftButton* backButton = findButtonByLabel("Back");
    if (backButton != nullptr) {
        uint16_t backButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;
        uint16_t backButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, ARRAY_ITEM_COUNT(horizontalButtonsData) - 1, false);
        backButton->setPosition(backButtonX, backButtonY);
    }

    selectedListIndex = -1;
    listScrollOffset = 0;

    // Állomások betöltése és rendezése
    loadAndSortStations();  // Ezt a konstruktor végére helyezzük

    // Automatikus kiválasztás a behangolt állomásra a konstruktorban
    // Most a sortedStations alapján keressük meg
    uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
    uint8_t currentTunedBandIdx = config.data.bandIdx;

    if (!sortedStations.empty() && visibleLines > 0) {
        for (uint8_t i = 0; i < sortedStations.size(); ++i) {
            const StationData& station = sortedStations[i];  // sortedStations-ból vesszük
            if (station.frequency == currentTunedFreq && station.bandIndex == currentTunedBandIdx) {
                selectedListIndex = i;
                listScrollOffset = selectedListIndex - (visibleLines / 2);
                if (listScrollOffset < 0) listScrollOffset = 0;
                // A max scroll offsetet a sortedStations méretéhez igazítjuk
                listScrollOffset = min(listScrollOffset, max(0, (int)sortedStations.size() - visibleLines));
                break;
            }
        }
    }
}

/**
 * Destruktor
 */
MemoryDisplay::~MemoryDisplay() {}

/**
 * Betölti az állomásokat a megfelelő store-ból és rendezi őket frekvencia szerint.
 */
void MemoryDisplay::loadAndSortStations() {
    sortedStations.clear();
    uint8_t count = isFmMode ? pFmStore->getStationCount() : pAmStore->getStationCount();
    for (uint8_t i = 0; i < count; ++i) {
        const StationData* station = isFmMode ? pFmStore->getStationByIndex(i) : pAmStore->getStationByIndex(i);
        if (station) {
            sortedStations.push_back(*station);
        }
    }

    // Rendezés frekvencia szerint
    std::sort(sortedStations.begin(), sortedStations.end(), [](const StationData& a, const StationData& b) {
        if (a.frequency != b.frequency) {
            return a.frequency < b.frequency;
        }
        return false;
    });

    if (selectedListIndex != -1 && selectedListIndex < count) {
        // A kiválasztás megmarad, ha érvényes volt, de a drawScreen() újra megkeresi a behangoltat.
    } else {
        selectedListIndex = -1;
        listScrollOffset = 0;
    }
}

/**
 * Képernyő kirajzolása
 */
void MemoryDisplay::drawScreen() {
    using namespace MemoryListConstants;
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextSize(1);
    tft.setTextColor(TITLE_TEXT_COLOR, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    const char* title = isFmMode ? "FM Memory" : "AM/LW/SW Memory";
    tft.drawString(title, tft.width() / 2, 5);

    tft.drawRect(listX, listY, listW, listH, LIST_BORDER_COLOR);

    loadAndSortStations();

    uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
    uint8_t currentTunedBandIdx = config.data.bandIdx;
    bool foundTunedInSorted = false;
    if (!sortedStations.empty() && visibleLines > 0) {
        for (uint8_t i = 0; i < sortedStations.size(); ++i) {
            const StationData& station = sortedStations[i];
            if (station.frequency == currentTunedFreq && station.bandIndex == currentTunedBandIdx) {
                if (selectedListIndex != i) {
                    selectedListIndex = i;
                    listScrollOffset = selectedListIndex - (visibleLines / 2);
                    if (listScrollOffset < 0) listScrollOffset = 0;
                    listScrollOffset = min(listScrollOffset, max(0, (int)sortedStations.size() - visibleLines));
                }
                foundTunedInSorted = true;
                break;
            }
        }
    }
    if (!foundTunedInSorted && !sortedStations.empty()) {
        if (selectedListIndex < 0 || selectedListIndex >= sortedStations.size()) {
            selectedListIndex = 0;
            listScrollOffset = 0;
        }
    } else if (sortedStations.empty()) {
        selectedListIndex = -1;
        listScrollOffset = 0;
    }

    drawStationList();

    drawScreenButtons();
    updateActionButtonsState();
}

/**
 * Kirajzol egyetlen listaelemet a megadott indexre.
 */
void MemoryDisplay::drawListItem(int index) {  // index itt a sortedStations indexe
    using namespace MemoryListConstants;

    if (index < 0 || index >= sortedStations.size() || index < listScrollOffset || index >= listScrollOffset + visibleLines) {
        return;
    }

    const StationData& station = sortedStations[index];

    uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
    uint8_t currentTunedBandIdx = config.data.bandIdx;

    bool isSelected = (index == selectedListIndex);
    bool isTuned = (station.frequency == currentTunedFreq && station.bandIndex == currentTunedBandIdx);

    uint16_t bgColor = isSelected ? SELECTED_ITEM_BG_COLOR : ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? SELECTED_ITEM_TEXT_COLOR : ITEM_TEXT_COLOR;

    int yPos = listY + 1 + (index - listScrollOffset) * lineHeight;

    tft.fillRect(listX + 1, yPos, listW - 2, lineHeight, bgColor);

    // 1. Fő betűtípus beállítása a listaelemhez (név, ikon, moduláció)
    //    a kiválasztottság alapján.
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
    } else {
        tft.setFreeFont(); // Alapértelmezett/számozott font
        tft.setTextSize(ITEM_TEXT_SIZE_NORMAL);
    }
    tft.setTextPadding(0);
    // A textColor-t közvetlenül a név/moduláció rajzolása előtt állítjuk be.

    int textCenterY = yPos + lineHeight / 2;

    // Ikon kirajzolása, ha az állomás be van hangolva
    int iconStartX = listX + ICON_PADDING_RIGHT;
    if (isTuned) {
        tft.setTextColor(TUNED_ICON_COLOR, bgColor);
        tft.setTextDatum(ML_DATUM); // Középre-balra igazítás
        tft.drawString(CURRENT_TUNED_ICON, iconStartX, textCenterY);
    }

    // Helyfoglalás az ikonnak (mindig), a fent beállított fonttal mérve.
    int iconWidth = tft.textWidth(CURRENT_TUNED_ICON);
    // Biztosítjuk, hogy a textColor legyen aktív a további mérésekhez, ha nem volt ikon.
    // Erre itt nincs közvetlen szükség, mert a név/moduláció előtt újra beállítjuk.
    int iconSpaceWidth = iconWidth + ICON_PADDING_RIGHT;
    int textStartX = iconStartX + iconSpaceWidth;

    // Frekvencia string előkészítése
    String freqStr;
    if (station.modulation == FM) {
        freqStr = String(station.frequency / 100.0f, 2) + " MHz";
    } else if (station.modulation == LSB || station.modulation == USB || station.modulation == CW) {
        uint32_t displayFreqHz = (uint32_t)station.frequency * 1000 - station.bfoOffset;
        long khz_part = displayFreqHz / 1000;
        int hz_tens_part = abs((int)(displayFreqHz % 1000)) / 10;
        char s[12];
        sprintf(s, "%ld.%02d", khz_part, hz_tens_part);
        freqStr = String(s) + " kHz";
    } else {
        freqStr = String(station.frequency) + " kHz";
    }

    // 2. Frekvencia szélességének mérése a dedikált kicsi fonttal
    tft.setFreeFont(); // Alapértelmezett/számozott font
    tft.setTextSize(1); // Kis betű a frekvenciához
    int freqWidth = tft.textWidth(freqStr);
    // A freqX-et később számoljuk, ez a jobb oldali igazítási pont (MR_DATUM)

    // 3. Visszaállítjuk/újra beállítjuk a fő fontot a név/moduláció méréséhez és rajzolásához.
    //    Ez biztosítja, hogy a frekvenciaméréshez használt font ne maradjon aktív.
    if (isSelected) {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
    } else {
        tft.setFreeFont(); // Alapértelmezett/számozott font
        tft.setTextSize(ITEM_TEXT_SIZE_NORMAL);
    }

    // --- Moduláció String és Szélesség (feltételes a layout számításhoz) ---
    int modWidthForLayout = 0;
    int nameModGapForLayout = 0;
    const char* modStrToDisplay = nullptr; // Ezt fogja tartalmazni a kiírandó string, ha van

    if (station.modulation != FM) {
        modStrToDisplay = band.getBandModeDescByIndex(station.modulation);
        modWidthForLayout = tft.textWidth(modStrToDisplay);
        nameModGapForLayout = NAME_MOD_GAP;
    }
    // FM esetén: modStrToDisplay nullptr marad, modWidthForLayout 0, nameModGapForLayout 0.

    // --- Elérhető névhossz számítása (az eredeti képletet használva, feltételes szélességekkel/résekkel) ---
    int availableNameWidth = (listX + listW - ICON_PADDING_RIGHT) /* teljes elérhető szélesség a szöveges elemeknek */
                             - textStartX                          /* kivonjuk a név előtti helyet */
                             - modWidthForLayout                   /* kivonjuk a modulációs string helyét (0, ha FM) */
                             - freqWidth                           /* kivonjuk a frekvencia string helyét */
                             - nameModGapForLayout                 /* kivonjuk a név-moduláció közötti rést (0, ha FM) */
                             - MOD_FREQ_GAP;                       /* kivonjuk a moduláció-frekvencia (vagy név-frekvencia, ha FM) közötti rést */
    // Biztosítjuk, hogy az availableNameWidth ne legyen negatív
    if (availableNameWidth < 0) availableNameWidth = 0;

    // --- Megjelenítendő név előkészítése és csonkolása ---

    char displayName[STATION_NAME_BUFFER_SIZE];
    strncpy(displayName, station.name, STATION_NAME_BUFFER_SIZE - 1);
    displayName[STATION_NAME_BUFFER_SIZE - 1] = '\0';
    if (strlen(displayName) > MAX_STATION_NAME_LEN) {
        displayName[MAX_STATION_NAME_LEN] = '\0';
    }

    // Név csonkolása, ha túl hosszú az elérhető helyhez képest
    while (tft.textWidth(displayName) > availableNameWidth && strlen(displayName) > 0) {
        displayName[strlen(displayName) - 1] = '\0';
    }
    int actualNameWidth = tft.textWidth(displayName); // A (potenciálisan csonkolt) név tényleges szélessége

    // --- Rajzolás ---
    int nameX = textStartX;
    // A modX és freqX pozíciókat közvetlenül a rajzolás előtt határozzuk meg
    int freqX = listX + listW - ICON_PADDING_RIGHT; // Jobbra igazított frekvencia

    // Név kirajzolása
    tft.setTextColor(textColor, bgColor); // Fő szövegszín beállítása
    tft.setTextDatum(ML_DATUM);
    tft.drawString(displayName, nameX, textCenterY);

    // Moduláció kirajzolása (ha nem FM és a string be van állítva)
    if (station.modulation != FM && modStrToDisplay != nullptr) {
        // A textColor (fő szövegszín) és bgColor már be van állítva
        // A font (normál/vastag) már be van állítva
        tft.setTextDatum(ML_DATUM);
        // A moduláció X pozíciója a név és a név-moduláció rés után
        int modDisplayX = nameX + actualNameWidth + nameModGapForLayout;
        tft.drawString(modStrToDisplay, modDisplayX, textCenterY);
    }

    // Frekvencia kirajzolása (jobbra igazítva)
    tft.setFreeFont(); // Alapértelmezett/számozott font
    tft.setTextSize(1); // Mindig kis betűvel
    tft.setTextColor(textColor, bgColor); // Fő szövegszín beállítása
    tft.setTextDatum(MR_DATUM);
    tft.drawString(freqStr, freqX, textCenterY);

    // Alapértelmezett szövegbeállítások visszaállítása (ha szükséges a ciklus további részében)
    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont();
    tft.setTextSize(1);
}


/**
 * Állomáslista kirajzolása
 */
void MemoryDisplay::drawStationList() {
    using namespace MemoryListConstants;
    tft.fillRect(listX + 1, listY + 1, listW - 2, listH - 2, ITEM_BG_COLOR);

    if (sortedStations.empty()) {
        tft.setFreeFont();
        tft.setTextSize(MemoryListConstants::ITEM_TEXT_SIZE_NORMAL);
        tft.setTextColor(MemoryListConstants::ITEM_TEXT_COLOR, MemoryListConstants::ITEM_BG_COLOR);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Memory Empty", listX + listW / 2, listY + listH / 2);
        return;
    }
    uint8_t count = sortedStations.size();

    if (listScrollOffset < 0) listScrollOffset = 0;
    if (listScrollOffset > max(0, count - visibleLines)) listScrollOffset = max(0, count - visibleLines);

    for (int i = 0; i < visibleLines; ++i) {
        int currentItemIndex = listScrollOffset + i;
        if (currentItemIndex < count) {
            drawListItem(currentItemIndex);
        } else {
            int yPos = listY + 1 + i * lineHeight;
            tft.fillRect(listX + 1, yPos, listW - 2, lineHeight, ITEM_BG_COLOR);
        }
    }
}

/**
 * Az Edit, Delete, Tune gombok állapotának frissítése a kiválasztás alapján
 */
void MemoryDisplay::updateActionButtonsState() {
    bool itemSelected = (selectedListIndex != -1 && selectedListIndex < sortedStations.size());
    TftButton* editButton = findButtonByLabel("Edit");
    if (editButton) editButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    TftButton* deleteButton = findButtonByLabel("Delete");
    if (deleteButton) deleteButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    TftButton* tuneButton = findButtonByLabel("Tune");
    if (tuneButton) tuneButton->setState(itemSelected ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
}

/**
 * Frissíti a lista nézetét hangolás után.
 * Újrarajzolja a korábban és az újonnan behangolt állomást a listában.
 * @param previouslyTunedSortedIdx A korábban behangolt állomás indexe a 'sortedStations' listában.
 */
void MemoryDisplay::updateListAfterTuning(uint8_t previouslyTunedSortedIdx) {
    // Újrarajzoljuk a korábban behangolt állomást (ha látható és eltér az újtól)
    // Ez eltávolítja róla a '>' jelet, mivel már nem ez van behangolva.
    // Figyelem: Ha previouslyTunedSortedIdx == (uint8_t)-1, az 255 lesz.
    // Ezért a feltételnek helyesnek kell lennie. Ha -1-et akarunk jelezni, akkor int típust kellene használni.
    // Jelenlegi implementációban a 255-ös indexet is érvényesnek veheti, ha a lista olyan nagy.
    // De mivel a MAX_FM/AM_STATIONS < 255, ez nem okozhat problémát, a 255. index sosem lesz valid.
    if (previouslyTunedSortedIdx != (uint8_t)-1 && // Explicit kasztolás a -1 összehasonlításhoz, ha uint8_t a paraméter
        previouslyTunedSortedIdx != selectedListIndex &&
        previouslyTunedSortedIdx >= listScrollOffset &&
        previouslyTunedSortedIdx < listScrollOffset + visibleLines) {
        drawListItem(previouslyTunedSortedIdx);
    }

    // Újrarajzoljuk az újonnan behangolt (aktuálisan kiválasztott) állomást (ha látható)
    // Ez megjeleníti mellette a '>' jelet.
    if (selectedListIndex != -1 && selectedListIndex >= listScrollOffset && selectedListIndex < listScrollOffset + visibleLines) {
        drawListItem(selectedListIndex);
    }
    updateActionButtonsState();
}

/**
 * Rotary encoder esemény lekezelése
 */
bool MemoryDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    if (sortedStations.empty() || pDialog != nullptr) return false;
    uint8_t count = sortedStations.size();

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
        if (selectedListIndex != -1 && selectedListIndex < count) {
            uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
            uint8_t bandIdxBeforeTune = config.data.bandIdx;
            int previouslyTunedSortedIndex = -1; // int, mert -1 lehet
            for (int i = 0; i < count; ++i) {
                if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune) {
                    previouslyTunedSortedIndex = i;
                    break;
                }
            }
            tuneToSelectedStation();
            updateListAfterTuning(static_cast<uint8_t>(previouslyTunedSortedIndex)); // Kasztolás uint8_t-ra
            return true;
        }
    } else if (encoderState.buttonState == RotaryEncoder::ButtonState::DoubleClicked) {
        if (selectedListIndex != -1 && selectedListIndex < count) {
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
        listScrollOffset = min(listScrollOffset, max(0, count - visibleLines));

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
    if (sortedStations.empty() || pDialog != nullptr) return false;
    uint8_t count = sortedStations.size();

    if (touched && tx >= listX && tx < (listX + listW) && ty >= listY && ty < (listY + listH)) {
        int touchedRow = (ty - (listY + 1)) / lineHeight;
        int touchedIndex = listScrollOffset + touchedRow;

        if (touchedIndex >= 0 && touchedIndex < count) {
            int oldSelectedItemIndex = selectedListIndex;
            int currentVisibleTop = listScrollOffset;
            int currentVisibleBottom = listScrollOffset + visibleLines;

            if (touchedIndex != selectedListIndex) {
                selectedListIndex = touchedIndex;
                if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < count && oldSelectedItemIndex >= currentVisibleTop && oldSelectedItemIndex < currentVisibleBottom) {
                    drawListItem(oldSelectedItemIndex);
                }
                if (selectedListIndex >= 0 && selectedListIndex < count && selectedListIndex >= currentVisibleTop && selectedListIndex < currentVisibleBottom) {
                    drawListItem(selectedListIndex);
                }
                updateActionButtonsState();
            } else {
                static unsigned long lastTouchTime = 0;
                static int lastTouchedIndex = -1;
                if (selectedListIndex == lastTouchedIndex && millis() - lastTouchTime < 500) { // Double tap
                    uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
                    uint8_t bandIdxBeforeTune = config.data.bandIdx;
                    int previouslyTunedSortedIndex = -1;
                    for (int i = 0; i < count; ++i) {
                        if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune) {
                            previouslyTunedSortedIndex = i;
                            break;
                        }
                    }
                    tuneToSelectedStation();
                    updateListAfterTuning(static_cast<uint8_t>(previouslyTunedSortedIndex));
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
        if (selectedListIndex != -1 && selectedListIndex < sortedStations.size()) {
            uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
            uint8_t bandIdxBeforeTune = config.data.bandIdx;
            int previouslyTunedSortedIndex = -1;
            for (int i = 0; i < sortedStations.size(); ++i) {
                if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune) {
                    previouslyTunedSortedIndex = i;
                    break;
                }
            }
            tuneToSelectedStation();
            updateListAfterTuning(static_cast<uint8_t>(previouslyTunedSortedIndex));
        }
    } else if (STREQ("Back", event.label)) {
        ::newDisplay = prevDisplay;
    }
}

/**
 * Dialóg Button touch esemény feldolgozása
 */
void MemoryDisplay::processDialogButtonResponse(TftButton::ButtonTouchEvent& event) {
    bool closeDialog = true;
    bool refreshListAndButtons = false;

    if (pDialog) {
        if (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME) {
            if (event.id == DLG_OK_BUTTON_ID) {
                if (currentDialogMode == DialogMode::SAVE_NEW_STATION) {
                    Utils::safeStrCpy(pendingStationData.name, stationNameBuffer.c_str());
                    if (addStationInternal(pendingStationData)) {
                        loadAndSortStations();
                        bool foundNew = false;
                        for (size_t i = 0; i < sortedStations.size(); ++i) {
                            if (sortedStations[i].frequency == pendingStationData.frequency && sortedStations[i].bandIndex == pendingStationData.bandIndex &&
                                sortedStations[i].bfoOffset == pendingStationData.bfoOffset) {
                                selectedListIndex = i;
                                foundNew = true;
                                break;
                            }
                        }
                        if (foundNew && visibleLines > 0) {
                            listScrollOffset = selectedListIndex - (visibleLines / 2);
                            if (listScrollOffset < 0) listScrollOffset = 0;
                            listScrollOffset = min(listScrollOffset, max(0, (int)sortedStations.size() - visibleLines));
                        } else if (!foundNew) {
                            selectedListIndex = -1;
                        }
                        refreshListAndButtons = true;
                    } else {
                        delete pDialog;
                        pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Memory full or station exists!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;
                    }
                } else {  // EDIT_STATION_NAME
                    if (selectedListIndex != -1 && selectedListIndex < sortedStations.size()) {
                        const StationData& stationToEdit = sortedStations[selectedListIndex];
                        int originalIndex = isFmMode ? pFmStore->findStation(stationToEdit.frequency, stationToEdit.bandIndex)
                                                     : pAmStore->findStation(stationToEdit.frequency, stationToEdit.bandIndex);
                        if (originalIndex != -1 && (stationToEdit.modulation == LSB || stationToEdit.modulation == USB || stationToEdit.modulation == CW)) {
                            const StationData* originalStation = isFmMode ? pFmStore->getStationByIndex(originalIndex) : pAmStore->getStationByIndex(originalIndex);
                            if (originalStation && originalStation->bfoOffset != stationToEdit.bfoOffset) {
                                originalIndex = -1;
                                for (uint8_t i = 0; i < (isFmMode ? pFmStore->getStationCount() : pAmStore->getStationCount()); ++i) {
                                    const StationData* s = isFmMode ? pFmStore->getStationByIndex(i) : pAmStore->getStationByIndex(i);
                                    if (s && s->frequency == stationToEdit.frequency && s->bandIndex == stationToEdit.bandIndex && s->bfoOffset == stationToEdit.bfoOffset) {
                                        originalIndex = i;
                                        break;
                                    }
                                }
                            }
                        }

                        if (originalIndex != -1) {
                            StationData updatedStation = stationToEdit;
                            Utils::safeStrCpy(updatedStation.name, stationNameBuffer.c_str());
                            if (updateStationInternal(originalIndex, updatedStation)) {
                                loadAndSortStations();
                                bool foundEdited = false;
                                for (size_t i = 0; i < sortedStations.size(); ++i) {
                                    if (sortedStations[i].frequency == updatedStation.frequency && sortedStations[i].bandIndex == updatedStation.bandIndex &&
                                        sortedStations[i].bfoOffset == updatedStation.bfoOffset &&
                                        strcmp(sortedStations[i].name, updatedStation.name) == 0) {
                                        selectedListIndex = i;
                                        foundEdited = true;
                                        break;
                                    }
                                }
                                if (!foundEdited) selectedListIndex = -1;
                                refreshListAndButtons = true;
                            } else {
                                delete pDialog;
                                pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Error in modify!"), "OK");
                                currentDialogMode = DialogMode::NONE;
                                closeDialog = false;
                            }
                        } else {
                            DEBUG("Error: Original station not found for edit.\n");
                        }
                    }
                }
            }
        } else if (currentDialogMode == DialogMode::DELETE_CONFIRM) {
            if (event.id == DLG_OK_BUTTON_ID) {
                if (selectedListIndex != -1 && selectedListIndex < sortedStations.size()) {
                    const StationData& stationToDelete = sortedStations[selectedListIndex];
                    int originalIndex = isFmMode ? pFmStore->findStation(stationToDelete.frequency, stationToDelete.bandIndex)
                                                 : pAmStore->findStation(stationToDelete.frequency, stationToDelete.bandIndex);
                    if (originalIndex != -1 && (stationToDelete.modulation == LSB || stationToDelete.modulation == USB || stationToDelete.modulation == CW)) {
                        const StationData* originalStation = isFmMode ? pFmStore->getStationByIndex(originalIndex) : pAmStore->getStationByIndex(originalIndex);
                        if (originalStation && originalStation->bfoOffset != stationToDelete.bfoOffset) {
                            originalIndex = -1;
                            for (uint8_t i = 0; i < (isFmMode ? pFmStore->getStationCount() : pAmStore->getStationCount()); ++i) {
                                const StationData* s = isFmMode ? pFmStore->getStationByIndex(i) : pAmStore->getStationByIndex(i);
                                if (s && s->frequency == stationToDelete.frequency && s->bandIndex == stationToDelete.bandIndex && s->bfoOffset == stationToDelete.bfoOffset) {
                                    originalIndex = i;
                                    break;
                                }
                            }
                        }
                    }

                    if (originalIndex != -1) {
                        if (deleteStationInternal(originalIndex)) {
                            loadAndSortStations();
                            selectedListIndex = -1;
                            refreshListAndButtons = true;
                        } else {
                            delete pDialog;
                            pDialog = new MessageDialog(this, tft, 250, 100, F("Error"), F("Failed to delete Station!"), "OK");
                            currentDialogMode = DialogMode::NONE;
                            closeDialog = false;
                        }
                    } else {
                        DEBUG("Error: Original station not found for delete.\n");
                    }
                }
            }
        }
    }

    if (closeDialog) {
        DisplayBase::processDialogButtonResponse(event);
        currentDialogMode = DialogMode::NONE;
        if (refreshListAndButtons) {
            updateActionButtonsState();
        }
    } else if (pDialog) {
        pDialog->drawDialog();
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
    uint8_t count = isFmMode ? pFmStore->getStationCount() : pAmStore->getStationCount();
    uint8_t maxCount = isFmMode ? MAX_FM_STATIONS : MAX_AM_STATIONS;

    if (count >= maxCount) {
        pDialog = new MessageDialog(this, tft, 200, 100, F("Error"), F("Memory Full!"), "OK");
        currentDialogMode = DialogMode::NONE;
        return;
    }

    BandTable& currentBandData = band.getCurrentBand();
    pendingStationData.frequency = currentBandData.varData.currFreq;
    if (currentBandData.varData.currMod == LSB || currentBandData.varData.currMod == USB || currentBandData.varData.currMod == CW) {
        pendingStationData.bfoOffset = currentBandData.varData.lastBFO;
    } else {
        pendingStationData.bfoOffset = 0;
    }
    pendingStationData.bandIndex = config.data.bandIdx;
    pendingStationData.modulation = currentBandData.varData.currMod;
    strncpy(pendingStationData.name, "", STATION_NAME_BUFFER_SIZE);

    bool alreadyExists = false;
    uint8_t storeCount = isFmMode ? pFmStore->getStationCount() : pAmStore->getStationCount();
    for (uint8_t i = 0; i < storeCount; ++i) {
        const StationData* s = isFmMode ? pFmStore->getStationByIndex(i) : pAmStore->getStationByIndex(i);
        if (s && s->frequency == pendingStationData.frequency && s->bandIndex == pendingStationData.bandIndex &&
            s->modulation == pendingStationData.modulation) {
            if (pendingStationData.modulation == LSB || pendingStationData.modulation == USB || pendingStationData.modulation == CW) {
                if (s->bfoOffset == pendingStationData.bfoOffset) {
                    alreadyExists = true;
                    break;
                }
            } else {
                alreadyExists = true;
                break;
            }
        }
    }

    if (alreadyExists) {
        pDialog = new MessageDialog(this, tft, 250, 100, F("Info"), F("Station already saved!"), "OK");
        currentDialogMode = DialogMode::NONE;
        return;
    }

    uint8_t currentMod = currentBandData.varData.currMod;
    if (currentMod == FM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxFM;
    } else if (currentMod == AM) {
        pendingStationData.bandwidthIndex = config.data.bwIdxAM;
    } else {
        pendingStationData.bandwidthIndex = config.data.bwIdxSSB;
    }

    // Kezdetben töröljük a buffert
    stationNameBuffer = ""; 

    // Ha FM módban vagyunk, próbáljuk meg lekérni az RDS állomásnevet az Si4735Utils segítségével
    if (isFmMode) {
        String rdsName = getCurrentRdsProgramService(); // Az Si4735Utils metódus hívása
        if (rdsName.length() > 0) {
            stationNameBuffer = rdsName; // Beállítjuk a billentyűzet bufferébe
        }
    }

    currentDialogMode = DialogMode::SAVE_NEW_STATION;
    pDialog = new VirtualKeyboardDialog(this, tft, F("Enter Station Name"), stationNameBuffer);
}

void MemoryDisplay::editSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= sortedStations.size()) return;
    const StationData& station = sortedStations[selectedListIndex];

    stationNameBuffer = station.name;
    currentDialogMode = DialogMode::EDIT_STATION_NAME;
    pDialog = new VirtualKeyboardDialog(this, tft, F("Edit Station Name"), stationNameBuffer);
}

void MemoryDisplay::deleteSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= sortedStations.size()) return;
    const StationData& station = sortedStations[selectedListIndex];

    currentDialogMode = DialogMode::DELETE_CONFIRM;
    String msg = "Delete '" + String(station.name) + "'?";
    pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm Delete"), F(msg.c_str()), "Delete", "Cancel");
}

void MemoryDisplay::tuneToSelectedStation() {
    if (selectedListIndex < 0 || selectedListIndex >= sortedStations.size()) return;
    const StationData& station = sortedStations[selectedListIndex];

    band.tuneMemoryStation(station.frequency, station.bfoOffset, station.bandIndex, station.modulation, station.bandwidthIndex);
    Si4735Utils::checkAGC();

    DisplayBase::frequencyChanged = true;
}

uint8_t MemoryDisplay::getCurrentStationCount() const {
    return isFmMode ? (pFmStore ? pFmStore->getStationCount() : 0) : (pAmStore ? pAmStore->getStationCount() : 0);
}

const StationData* MemoryDisplay::getStationData(uint8_t index) const {
    return isFmMode ? (pFmStore ? pFmStore->getStationByIndex(index) : nullptr) : (pAmStore ? pAmStore->getStationByIndex(index) : nullptr);
}

bool MemoryDisplay::addStationInternal(const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->addStation(station) : false) : (pAmStore ? pAmStore->addStation(station) : false);
}

bool MemoryDisplay::updateStationInternal(uint8_t originalIndex, const StationData& station) {
    return isFmMode ? (pFmStore ? pFmStore->updateStation(originalIndex, station) : false) : (pAmStore ? pAmStore->updateStation(originalIndex, station) : false);
}

bool MemoryDisplay::deleteStationInternal(uint8_t originalIndex) {
    return isFmMode ? (pFmStore ? pFmStore->deleteStation(originalIndex) : false) : (pAmStore ? pAmStore->deleteStation(originalIndex) : false);
}
