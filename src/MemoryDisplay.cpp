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
constexpr int ITEM_PADDING_Y = 5;                           // Elem függőleges margója
constexpr int ITEM_TEXT_SIZE_NORMAL = 2;                    // Nem kiválasztott elem betűmérete
constexpr uint16_t ITEM_TEXT_COLOR = TFT_WHITE;             // Nem kiválasztott elem szövegszíne
constexpr uint16_t ITEM_BG_COLOR = TFT_BLACK;               // Vagy TFT_COLOR_BACKGROUND
constexpr uint16_t SELECTED_ITEM_TEXT_COLOR = TFT_BLACK;    // SetupDisplay-hez hasonlóan
constexpr uint16_t SELECTED_ITEM_BG_COLOR = TFT_LIGHTGREY;  // SetupDisplay-hez hasonlóan
constexpr uint16_t LIST_BORDER_COLOR = TFT_DARKGREY;        // SetupDisplay-hez hasonlóan
constexpr uint16_t TITLE_TEXT_COLOR = TFT_YELLOW;           // Cím színe
constexpr uint16_t TUNED_ICON_COLOR = TFT_MAGENTA;          // Behangolt ikon színe
constexpr int ICON_PADDING_RIGHT = 5;                       // Rés a frekvencia és az ikon között
constexpr int MOD_FREQ_GAP = 10;                            // Rés a moduláció és a frekvencia között
constexpr int NAME_MOD_GAP = 10;                            // Rés a név és a moduláció között
constexpr char CURRENT_TUNED_ICON[] = ">";                  // Ikon a behangolt állomás jelzésére
}  // namespace MemoryListConstants

/**
 * Konstruktor
 */
MemoryDisplay::MemoryDisplay(TFT_eSPI& tft_ref, SI4735& si4735_ref, Band& band_ref)
    : DisplayBase(tft_ref, si4735_ref, band_ref),
      scrollListComponent(tft_ref, 0, 0, 0, 0, this),  // Helyőrző, alább lesz beállítva
      dynamicLineHeight(0) {

    // Aktuális mód meghatározása (FM vagy egyéb)
    isFmMode = (band.getCurrentBandType() == FM_BAND_TYPE);

    // Store pointerek beállítása
    pFmStore = &fmStationStore;  // Globális példányra mutat
    pAmStore = &amStationStore;  // Globális példányra mutat

    using namespace MemoryListConstants;
    // Lista területének meghatározása
    uint16_t statusLineHeight = 20;                                         // Becsült magasság
    uint16_t bottomButtonsHeight = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2;  // Vízszintes gombok helye alul

    // Sormagasság kiszámítása az itemHeight-hez
    tft_ref.setFreeFont(&FreeSansBold9pt7b);  // Elemekhez használt font
    tft_ref.setTextSize(1);
    dynamicLineHeight = tft_ref.fontHeight() + MemoryListConstants::ITEM_PADDING_Y * 2;
    tft_ref.setFreeFont();  // Font visszaállítása

    int lX = LIST_X_MARGIN;
    int lY = statusLineHeight + LIST_Y_MARGIN + 15;
    int lW = tft_ref.width() - (LIST_X_MARGIN * 2);
    int lH = tft_ref.height() - lY - bottomButtonsHeight - LIST_Y_MARGIN;

    // ScrollableListComponent inicializálása a kiszámított méretekkel
    new (&scrollListComponent) ScrollableListComponent(tft_ref, lX, lY, lW, lH, this, MemoryListConstants::ITEM_BG_COLOR, MemoryListConstants::LIST_BORDER_COLOR);

    // A selectedListIndex, listScrollOffset, visibleLines a scrollListComponent által kezelt
    // A loadAndSortStations-t a scrollListComponent.refresh() hívja meg a loadData()-n keresztül

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

    // A kezdeti adatbetöltést és kiválasztást a drawScreen -> scrollListComponent.refresh() -> loadData() kezeli
}

/**
 * Destruktor
 */
MemoryDisplay::~MemoryDisplay() {}

/**
 * Adatok betöltése és rendezése.
 * Visszaadja a kiválasztandó elem indexét, vagy -1-et.
 */
int MemoryDisplay::loadData() {
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
        // Frekvencia egyezés esetén BFO szerint is rendezhetünk, ha van
        if ((a.modulation == LSB || a.modulation == USB || a.modulation == CW) && (b.modulation == LSB || b.modulation == USB || b.modulation == CW)) {
            if (a.bfoOffset != b.bfoOffset) {
                return a.bfoOffset < b.bfoOffset;
            }
        }
        // Végül név szerint, ha minden más egyezik
        return strcmp(a.name, b.name) < 0;
    });

    int selectionToReturn = -1;
    if (selectSpecificStationAfterLoad && !sortedStations.empty()) {
        for (size_t i = 0; i < sortedStations.size(); ++i) {
            // Pontos egyezést keresünk a stationToSelectAfterLoad-dal (név alapján is)
            if (sortedStations[i].frequency == stationToSelectAfterLoad.frequency && sortedStations[i].bandIndex == stationToSelectAfterLoad.bandIndex &&
                sortedStations[i].modulation == stationToSelectAfterLoad.modulation && sortedStations[i].bfoOffset == stationToSelectAfterLoad.bfoOffset &&
                strcmp(sortedStations[i].name, stationToSelectAfterLoad.name) == 0) {
                selectionToReturn = i;
                break;
            }
        }
        selectSpecificStationAfterLoad = false;  // Flag resetelése
    } else if (!sortedStations.empty()) {
        // Eredeti logika: aktuálisan behangolt állomás kiválasztása
        uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
        uint8_t currentTunedBandIdx = config.data.bandIdx;
        int16_t currentBfoOffset = band.getCurrentBand().varData.lastBFO;
        uint8_t currentMod = band.getCurrentBand().varData.currMod;

        for (size_t i = 0; i < sortedStations.size(); ++i) {
            if (sortedStations[i].frequency == currentTunedFreq && sortedStations[i].bandIndex == currentTunedBandIdx && sortedStations[i].modulation == currentMod) {
                if (currentMod == LSB || currentMod == USB || currentMod == CW) {
                    if (sortedStations[i].bfoOffset == currentBfoOffset) {
                        selectionToReturn = i;
                        break;
                    }
                } else {  // AM/FM
                    selectionToReturn = i;
                    break;
                }
            }
        }
    }
    return selectionToReturn;
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

    scrollListComponent.refresh();  // Ez meghívja a loadData()-t, majd a draw()-t

    drawScreenButtons();
    updateActionButtonsState();
}

// IScrollableListDataSource implementációk
int MemoryDisplay::getItemCount() const { return sortedStations.size(); }

void MemoryDisplay::activateListItem(int index) {
    tuneToSelectedStation();  // Az alapértelmezett aktiválás a hangolás
}

int MemoryDisplay::getItemHeight() const { return dynamicLineHeight; }

/**
 * Kirajzol egyetlen listaelemet a megadott indexre.
 */
void MemoryDisplay::drawListItem(TFT_eSPI& tft_ref, int index, int itemX, int itemY, int itemW, int itemH, bool isSelected) {
    using namespace MemoryListConstants;

    // Az index érvényességének ellenőrzése a listScrollOffset-tel szemben a ScrollableListComponent által kezelt
    // Csak a sortedStations.size()-szal szemben kell ellenőrizni
    if (index < 0 || index >= sortedStations.size()) return;

    const StationData& station = sortedStations[index];

    uint16_t currentTunedFreq = band.getCurrentBand().varData.currFreq;
    uint8_t currentTunedBandIdx = config.data.bandIdx;

    // Az isSelected paraméterként érkezik
    bool isTuned = (station.frequency == currentTunedFreq && station.bandIndex == currentTunedBandIdx);
    if (isTuned && (station.modulation == LSB || station.modulation == USB || station.modulation == CW)) {
        isTuned = (station.bfoOffset == band.getCurrentBand().varData.lastBFO);
    }

    uint16_t bgColor = isSelected ? SELECTED_ITEM_BG_COLOR : ITEM_BG_COLOR;
    uint16_t textColor = isSelected ? SELECTED_ITEM_TEXT_COLOR : ITEM_TEXT_COLOR;

    // Az itemX, itemY, itemW, itemH a ScrollableListComponent által biztosított
    tft_ref.fillRect(itemX, itemY, itemW, itemH, bgColor);

    // Fő betűtípus beállítása a listaelemhez (név, ikon, moduláció).
    // Mindig FreeSansBold9pt7b, méret 1. A szín változik a kiválasztottság alapján.
    tft_ref.setFreeFont(&FreeSansBold9pt7b);
    tft_ref.setTextSize(1);
    tft_ref.setTextPadding(0);
    // A textColor-t közvetlenül a név/moduláció rajzolása előtt állítjuk be.

    int textCenterY = itemY + itemH / 2;

    // Ikon kirajzolása, ha az állomás be van hangolva
    int iconStartX = itemX + ICON_PADDING_RIGHT;
    if (isTuned) {
        tft_ref.setTextColor(TUNED_ICON_COLOR, bgColor);
        tft_ref.setTextDatum(ML_DATUM);  // Középre-balra igazítás
        tft_ref.drawString(CURRENT_TUNED_ICON, iconStartX, textCenterY);
    }

    // Helyfoglalás az ikonnak (mindig), a fent beállított fonttal mérve.
    int iconWidth = tft.textWidth(CURRENT_TUNED_ICON);
    // Biztosítjuk, hogy a textColor legyen aktív a további mérésekhez, ha nem volt ikon.
    // Erre itt nincs közvetlen szükség, mert a név/moduláció előtt újra beállítjuk.
    int iconSpaceWidth = iconWidth + ICON_PADDING_RIGHT;  // Az ikon által foglalt hely + padding
    int textStartX = iconStartX + iconSpaceWidth;         // Mindig foglalunk helyet az ikonnak, függetlenül attól, hogy ki van-e rajzolva

    // Frekvencia string előkészítése
    String freqStr;
    if (station.modulation == FM) {
        freqStr = String(station.frequency / 100.0f, 2) + " MHz";
    } else if (station.modulation == LSB || station.modulation == USB || station.modulation == CW) {  // SSB/CW frekvencia formázása
        uint32_t displayFreqHz = (uint32_t)station.frequency * 1000 - station.bfoOffset;
        long khz_part = displayFreqHz / 1000;
        int hz_tens_part = abs((int)(displayFreqHz % 1000)) / 10;
        char s[12];
        sprintf(s, "%ld.%02d", khz_part, hz_tens_part);
        freqStr = String(s) + " kHz";
    } else {
        freqStr = String(station.frequency) + " kHz";
    }

    // Frekvencia szélességének mérése a dedikált kicsi (alapértelmezett) fonttal
    tft_ref.setFreeFont();   // Alapértelmezett/számozott font
    tft_ref.setTextSize(1);  // Kis betű a frekvenciához
    int freqWidth = tft_ref.textWidth(freqStr);
    // A freqX-et később számoljuk, ez a jobb oldali igazítási pont (MR_DATUM)

    // Visszaállítjuk a fő fontot (FreeSansBold9pt7b) a moduláció méréséhez és a név/moduláció rajzolásához.
    tft_ref.setFreeFont(&FreeSansBold9pt7b);
    tft_ref.setTextSize(1);

    // --- Moduláció String és Szélesség (feltételes a layout számításhoz) ---
    int modWidthForLayout = 0;
    int nameModGapForLayout = 0;
    const char* modStrToDisplay = nullptr;  // Ezt fogja tartalmazni a kiírandó string, ha van

    if (station.modulation != FM) {
        modStrToDisplay = band.getBandModeDescByIndex(station.modulation);
        modWidthForLayout = tft_ref.textWidth(modStrToDisplay);
        nameModGapForLayout = NAME_MOD_GAP;
    }
    // FM esetén: modStrToDisplay nullptr marad, modWidthForLayout 0, nameModGapForLayout 0.

    // --- Elérhető névhossz számítása (az eredeti képletet használva, feltételes szélességekkel/résekkel) ---
    int availableNameWidth = (itemX + itemW - ICON_PADDING_RIGHT) /* teljes elérhető szélesség a szöveges elemeknek */
                             - textStartX                         /* kivonjuk a név előtti helyet */
                             - modWidthForLayout                  /* kivonjuk a modulációs string helyét (0, ha FM) */
                             - freqWidth                          /* kivonjuk a frekvencia string helyét */
                             - nameModGapForLayout                /* kivonjuk a név-moduláció közötti rést (0, ha FM) */
                             - MOD_FREQ_GAP;                      /* kivonjuk a moduláció-frekvencia (vagy név-frekvencia, ha FM) közötti rést */
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
    while (tft_ref.textWidth(displayName) > availableNameWidth && strlen(displayName) > 0) {
        displayName[strlen(displayName) - 1] = '\0';
    }
    int actualNameWidth = tft_ref.textWidth(displayName);  // A (potenciálisan csonkolt) név tényleges szélessége

    // --- Rajzolás ---
    int nameX = textStartX;
    // A modX és freqX pozíciókat közvetlenül a rajzolás előtt határozzuk meg
    int freqX = itemX + itemW - ICON_PADDING_RIGHT;  // Jobbra igazított frekvencia

    // Név kirajzolása
    tft_ref.setTextColor(textColor, bgColor);  // Fő szövegszín beállítása
    tft_ref.setTextDatum(ML_DATUM);
    tft_ref.drawString(displayName, nameX, textCenterY);

    // Moduláció kirajzolása (ha nem FM és a string be van állítva)
    if (station.modulation != FM && modStrToDisplay != nullptr) {
        // A textColor (fő szövegszín) és bgColor már be van állítva
        // A font (normál/vastag) már be van állítva
        tft_ref.setTextDatum(ML_DATUM);  // Középre balra igazítás
        // A moduláció X pozíciója a név és a név-moduláció rés után
        int modDisplayX = nameX + actualNameWidth + nameModGapForLayout;
        tft_ref.drawString(modStrToDisplay, modDisplayX, textCenterY);
    }

    // Frekvencia kirajzolása (jobbra igazítva)
    tft_ref.setFreeFont();                     // Alapértelmezett/számozott font
    tft_ref.setTextSize(1);                    // Mindig kis betűvel
    tft_ref.setTextColor(textColor, bgColor);  // Fő szövegszín beállítása
    tft_ref.setTextDatum(MR_DATUM);
    tft_ref.drawString(freqStr, freqX, textCenterY);
}

/**
 * Az Edit, Delete, Tune gombok állapotának frissítése a kiválasztás alapján
 */
void MemoryDisplay::updateActionButtonsState() {
    bool itemSelected = (scrollListComponent.getSelectedItemIndex() != -1);
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
void MemoryDisplay::updateListAfterTuning(int previouslyTunedSortedIdx) {
    int newlyTunedSortedIdx = scrollListComponent.getSelectedItemIndex();

    // Előzőleg behangolt elem újrarajzolása, ha érvényes volt, látható, és nem ugyanaz, mint az új
    // A redrawItem metódus ellenőrzi a láthatóságot.
    if (previouslyTunedSortedIdx != -1 && previouslyTunedSortedIdx != newlyTunedSortedIdx) {
        scrollListComponent.redrawItem(previouslyTunedSortedIdx);
    }

    // Újonnan behangolt elem újrarajzolása, ha érvényes és látható
    // A redrawItem metódus ellenőrzi a láthatóságot.
    if (newlyTunedSortedIdx != -1) {
        scrollListComponent.redrawItem(newlyTunedSortedIdx);
    }
    // Ha a scrollbar látható, érdemes lehet azt is frissíteni, bár ebben az esetben valószínűleg nem változik.
    // A drawScrollbar() saját maga törli a területét, így biztonságosan hívható.
    if (scrollListComponent.isScrollbarVisible()) {
        scrollListComponent.drawScrollbar();
    }

    updateActionButtonsState();
}

/**
 * Rotary encoder esemény lekezelése
 */
bool MemoryDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    if (sortedStations.empty() || pDialog != nullptr) return false;

    bool scrolled = scrollListComponent.handleRotaryScroll(encoderState);

    if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        if (scrollListComponent.getSelectedItemIndex() != -1) {
            uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
            uint8_t bandIdxBeforeTune = config.data.bandIdx;
            int16_t bfoBeforeTune = band.getCurrentBand().varData.lastBFO;
            int previouslyTunedSortedIndex = -1;
            for (size_t i = 0; i < sortedStations.size(); ++i) {
                if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune &&
                    (sortedStations[i].modulation != LSB && sortedStations[i].modulation != USB && sortedStations[i].modulation != CW ||
                     sortedStations[i].bfoOffset == bfoBeforeTune)) {
                    previouslyTunedSortedIndex = i;
                    break;
                }
            }
            scrollListComponent.activateSelectedItem();  // Ez meghívja a tuneToSelectedStation-t
            updateListAfterTuning(previouslyTunedSortedIndex);
            return true;
        }
    } else if (encoderState.buttonState == RotaryEncoder::ButtonState::DoubleClicked) {
        if (scrollListComponent.getSelectedItemIndex() != -1) {
            editSelectedStation();
            return true;
        }
    }

    if (scrolled) {
        updateActionButtonsState();
    }

    return scrolled || (encoderState.buttonState != RotaryEncoder::ButtonState::Open);
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool MemoryDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    if (pDialog) return false;

    // Az activateOnTouch = false használata a komponenshez, az aktiválást itt kezeljük a dupla koppintáshoz
    bool listHandled = scrollListComponent.handleTouch(touched, tx, ty, false);

    if (listHandled && touched) {
        int currentSelection = scrollListComponent.getSelectedItemIndex();
        if (currentSelection != -1) {
            // Dupla koppintás logika
            if (scrollListComponent.getSelectedItemIndex() == currentSelection) {  // Ellenőrizzük, hogy a kiválasztás még mindig ugyanaz-e
                static unsigned long lastTouchTime = 0;
                static int lastTouchedIndex = -1;
                if (currentSelection == lastTouchedIndex && (millis() - lastTouchTime) < 500) {  // Dupla koppintás
                    uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
                    uint8_t bandIdxBeforeTune = config.data.bandIdx;
                    int16_t bfoBeforeTune = band.getCurrentBand().varData.lastBFO;
                    int previouslyTunedSortedIndex = -1;
                    for (size_t i = 0; i < sortedStations.size(); ++i) {
                        if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune &&
                            (sortedStations[i].modulation != LSB && sortedStations[i].modulation != USB && sortedStations[i].modulation != CW ||
                             sortedStations[i].bfoOffset == bfoBeforeTune)) {
                            previouslyTunedSortedIndex = i;
                            break;
                        }
                    }
                    scrollListComponent.activateSelectedItem();  // Ez meghívja a tuneToSelectedStation-t
                    updateListAfterTuning(previouslyTunedSortedIndex);
                    lastTouchTime = 0;
                    lastTouchedIndex = -1;
                } else {
                    lastTouchTime = millis();
                    lastTouchedIndex = currentSelection;
                }
            }
            updateActionButtonsState();  // Gombok frissítése az új kiválasztás alapján
        }
    }
    return listHandled;
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
        if (scrollListComponent.getSelectedItemIndex() != -1) {
            uint16_t freqBeforeTune = band.getCurrentBand().varData.currFreq;
            uint8_t bandIdxBeforeTune = config.data.bandIdx;
            int16_t bfoBeforeTune = band.getCurrentBand().varData.lastBFO;
            int previouslyTunedSortedIndex = -1;
            for (size_t i = 0; i < sortedStations.size(); ++i) {
                if (sortedStations[i].frequency == freqBeforeTune && sortedStations[i].bandIndex == bandIdxBeforeTune &&
                    (sortedStations[i].modulation != LSB && sortedStations[i].modulation != USB && sortedStations[i].modulation != CW ||
                     sortedStations[i].bfoOffset == bfoBeforeTune)) {
                    previouslyTunedSortedIndex = i;
                    break;
                }
            }
            scrollListComponent.activateSelectedItem();  // Ez meghívja a tuneToSelectedStation-t
            updateListAfterTuning(previouslyTunedSortedIndex);
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

    if (pDialog) {
        if (currentDialogMode == DialogMode::SAVE_NEW_STATION || currentDialogMode == DialogMode::EDIT_STATION_NAME) {
            if (event.id == DLG_OK_BUTTON_ID) {
                Utils::safeStrCpy(pendingStationData.name, stationNameBuffer.c_str());  // Név beállítása a pendingStationData-ba
                if (currentDialogMode == DialogMode::SAVE_NEW_STATION) {
                    if (addStationInternal(pendingStationData)) {
                        stationToSelectAfterLoad = pendingStationData;  // Ezt kell kiválasztani
                        selectSpecificStationAfterLoad = true;
                    } else {
                        delete pDialog;
                        pDialog = new MessageDialog(this, tft, 280, 100, F("Error"), F("Memory full or station exists!"), "OK");
                        currentDialogMode = DialogMode::NONE;
                        closeDialog = false;
                    }
                } else {  // EDIT_STATION_NAME
                    if (scrollListComponent.getSelectedItemIndex() != -1 && scrollListComponent.getSelectedItemIndex() < sortedStations.size()) {
                        const StationData& stationToEdit = sortedStations[scrollListComponent.getSelectedItemIndex()];
                        int originalIndex = isFmMode ? pFmStore->findStation(stationToEdit.frequency, stationToEdit.bandIndex, stationToEdit.bfoOffset)
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
                            // A pendingStationData.name már frissítve van a Utils::safeStrCpy által feljebb
                            // A többi adat a stationToEdit-ből (ami pendingStationData volt a dialógus előtt) jön
                            pendingStationData.frequency = stationToEdit.frequency;  // Biztosítjuk, hogy a pendingStationData a helyes adatokat tartalmazza
                            pendingStationData.bandIndex = stationToEdit.bandIndex;
                            pendingStationData.modulation = stationToEdit.modulation;
                            pendingStationData.bfoOffset = stationToEdit.bfoOffset;
                            pendingStationData.bandwidthIndex = stationToEdit.bandwidthIndex;

                            if (updateStationInternal(originalIndex, pendingStationData)) {
                                stationToSelectAfterLoad = pendingStationData;  // Ezt kell kiválasztani
                                selectSpecificStationAfterLoad = true;
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
                if (scrollListComponent.getSelectedItemIndex() != -1 && scrollListComponent.getSelectedItemIndex() < sortedStations.size()) {
                    const StationData& stationToDelete = sortedStations[scrollListComponent.getSelectedItemIndex()];
                    int originalIndex = isFmMode ? pFmStore->findStation(stationToDelete.frequency, stationToDelete.bandIndex, stationToDelete.bfoOffset)
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
                        if (!deleteStationInternal(originalIndex)) {
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
        if (s && s->frequency == pendingStationData.frequency && s->bandIndex == pendingStationData.bandIndex && s->modulation == pendingStationData.modulation) {
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
        String rdsName = getCurrentRdsProgramService();  // Az Si4735Utils metódus hívása
        if (rdsName.length() > 0) {
            stationNameBuffer = rdsName;  // Beállítjuk a billentyűzet bufferébe
        }
    }

    currentDialogMode = DialogMode::SAVE_NEW_STATION;
    // A selectSpecificStationAfterLoad flag-et a processDialogButtonResponse állítja be az OK után.
    pDialog = new VirtualKeyboardDialog(this, tft, F("Enter Station Name"), stationNameBuffer);
}

/**
 * KIválasztott elem javítása
 */
void MemoryDisplay::editSelectedStation() {
    int currentSelection = scrollListComponent.getSelectedItemIndex();
    if (currentSelection < 0 || currentSelection >= sortedStations.size()) return;
    const StationData& station = sortedStations[currentSelection];

    stationNameBuffer = station.name;
    currentDialogMode = DialogMode::EDIT_STATION_NAME;
    pDialog = new VirtualKeyboardDialog(this, tft, F("Edit Station Name"), stationNameBuffer);
}

/**
 * Kiválasztott elem törlése
 */
void MemoryDisplay::deleteSelectedStation() {
    int currentSelection = scrollListComponent.getSelectedItemIndex();
    if (currentSelection < 0 || currentSelection >= sortedStations.size()) return;
    const StationData& station = sortedStations[currentSelection];

    currentDialogMode = DialogMode::DELETE_CONFIRM;
    String msg = "Delete '" + String(station.name) + "'?";
    pDialog = new MessageDialog(this, tft, 250, 120, F("Confirm Delete"), F(msg.c_str()), "Delete", "Cancel");
}

/**
 * Kiválasztott állomás hangolása
 */
void MemoryDisplay::tuneToSelectedStation() {
    int currentSelection = scrollListComponent.getSelectedItemIndex();
    if (currentSelection < 0 || currentSelection >= sortedStations.size()) return;
    const StationData& station = sortedStations[currentSelection];

    band.tuneMemoryStation(station.frequency, station.bfoOffset, station.bandIndex, station.modulation, station.bandwidthIndex);
    Si4735Utils::checkAGC();

    DisplayBase::frequencyChanged = true;
}

uint8_t MemoryDisplay::getCurrentStationCount() const { return isFmMode ? (pFmStore ? pFmStore->getStationCount() : 0) : (pAmStore ? pAmStore->getStationCount() : 0); }

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
