#include "VirtualKeyboardDialog.h"

#include "StationData.h"  // Szükséges a StationData-hoz (MAX_INPUT_LEN miatt)
#include "defines.h"      // Szükséges a TFT színekhez
#include "utils.h"        // Szükséges a Utils::beepError()-hez

// --- Konstansok az elrendezéshez ---
#define KEY_WIDTH 40
#define KEY_HEIGHT 30
#define KEY_GAP_X 3
#define KEY_GAP_Y 3
#define INPUT_DISPLAY_HEIGHT 40
#define INPUT_DISPLAY_MARGIN_Y 5
#define KEYBOARD_START_Y_OFFSET (INPUT_DISPLAY_HEIGHT + INPUT_DISPLAY_MARGIN_Y * 2)  // Billentyűzet kezdő Y pozíciója a dialógus tetejétől

#define MAX_INPUT_LEN STATION_NAME_BUFFER_SIZE - 1  // Maximális bemeneti hossz (a StationData.h-ból)

/**
 * Konstruktor
 */
VirtualKeyboardDialog::VirtualKeyboardDialog(IDialogParent* pParent, TFT_eSPI& tft, const __FlashStringHelper* title, String& target)
    : DialogBase(pParent, tft, 456, 260, title),  // Méretet ellenőrizd/állítsd be!
      targetString(target),
      originalString(target),  // Eredeti string mentése
      currentInput(target)     // Kezdetben a szerkesztendő stringgel inicializáljuk
{
    DEBUG("VirtualKeyboardDialog::VirtualKeyboardDialog\n");

    // Billentyűzet és gombok felépítése
    buildKeyboard();

    // --- ÚJ ---
    lastCursorToggleTime = millis();  // Kezdeti időpont beállítása a villogáshoz
    // --- ÚJ VÉGE ---

    // Dialógus kirajzolása (ez már rajzolja a kezdeti kurzort is)
    drawDialog();
}

/**
 * Destruktor
 */
VirtualKeyboardDialog::~VirtualKeyboardDialog() {
    DEBUG("VirtualKeyboardDialog::~VirtualKeyboardDialog\n");

    // Dinamikusan létrehozott gombok törlése
    delete okButton;
    delete cancelButton;
    delete backspaceButton;
    delete clearButton;
    // TODO: Töröld az esetleges további speciális gombokat (Space, Shift)

    // Karakter gombok törlése a vektorból
    for (TftButton* btn : keyboardButtons) {
        delete btn;
    }
    keyboardButtons.clear();
}

/**
 * Billentyűzet gombjainak és elrendezésének felépítése
 */
void VirtualKeyboardDialog::buildKeyboard() {
    // Bemeneti mező koordinátái
    inputDisplayX = x + 10;
    inputDisplayY = y + DLG_HEADER_H + INPUT_DISPLAY_MARGIN_Y;
    inputDisplayW = w - 20;  // Teljes szélesség - margók
    inputDisplayH = INPUT_DISPLAY_HEIGHT;

    // Billentyűzet kezdő pozíciója
    uint16_t keyboardStartY = y + DLG_HEADER_H + KEYBOARD_START_Y_OFFSET;

    // Karakter gombok létrehozása a keyLayout alapján
    uint16_t currentY = keyboardStartY;
    uint8_t btnId = DLG_MULTI_BTN_ID_START;  // Kezdő ID a karakter gombokhoz

    for (uint8_t r = 0; r < keyRows; ++r) {
        const char* rowLayout = keyLayout[r];
        uint8_t rowLen = strlen(rowLayout);
        uint16_t totalRowWidth = (rowLen * KEY_WIDTH) + ((rowLen - 1) * KEY_GAP_X);
        uint16_t startX = x + (w - totalRowWidth) / 2;  // Sor középre igazítása

        for (uint8_t c = 0; c < rowLen; ++c) {
            char keyChar[2] = {rowLayout[c], '\0'};  // Gomb felirata (1 karakter + null terminátor)
            TftButton* keyButton = new TftButton(btnId++, tft, startX + c * (KEY_WIDTH + KEY_GAP_X), currentY, KEY_WIDTH, KEY_HEIGHT, keyChar, TftButton::ButtonType::Pushable);
            keyboardButtons.push_back(keyButton);
        }
        currentY += KEY_HEIGHT + KEY_GAP_Y;
    }

    // Speciális gombok létrehozása (OK, Cancel, Bksp, Clear)
    // Ezeket a karakter gombok alá helyezzük el
    uint16_t specialBtnY = currentY + KEY_GAP_Y;  // Egy kis extra rés
    uint16_t btnWidthSpecial = 60;                // Speciális gombok szélessége
    uint16_t totalSpecialWidth = (btnWidthSpecial * 4) + (KEY_GAP_X * 3);
    uint16_t startXSpecial = x + (w - totalSpecialWidth) / 2;

    clearButton =
        new TftButton(btnId++, tft, startXSpecial + 0 * (btnWidthSpecial + KEY_GAP_X), specialBtnY, btnWidthSpecial, KEY_HEIGHT, "Clear", TftButton::ButtonType::Pushable);
    backspaceButton =
        new TftButton(btnId++, tft, startXSpecial + 1 * (btnWidthSpecial + KEY_GAP_X), specialBtnY, btnWidthSpecial, KEY_HEIGHT, "Bksp", TftButton::ButtonType::Pushable);
    cancelButton = new TftButton(DLG_CANCEL_BUTTON_ID, tft, startXSpecial + 2 * (btnWidthSpecial + KEY_GAP_X), specialBtnY, btnWidthSpecial, KEY_HEIGHT, "Cancel",
                                 TftButton::ButtonType::Pushable);
    okButton =
        new TftButton(DLG_OK_BUTTON_ID, tft, startXSpecial + 3 * (btnWidthSpecial + KEY_GAP_X), specialBtnY, btnWidthSpecial, KEY_HEIGHT, "OK", TftButton::ButtonType::Pushable);

    // OK gomb kezdetben tiltva, ha az input érvénytelen (pl. üres)
    okButton->setState(currentInput.length() > 0 ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
}

/**
 * Dialógus kirajzolása
 */
void VirtualKeyboardDialog::drawDialog() {
    // Ős osztály rajzolása (keret, cím, háttér, 'X' gomb)
    DialogBase::drawDialog();

    // Bemeneti mező hátterének és keretének rajzolása
    tft.fillRect(inputDisplayX, inputDisplayY, inputDisplayW, inputDisplayH, TFT_BLACK);  // Háttér
    tft.drawRect(inputDisplayX, inputDisplayY, inputDisplayW, inputDisplayH, TFT_WHITE);  // Keret

    // Aktuális bemenet kirajzolása (kurzorral együtt)
    updateInputDisplay();

    // Billentyűzet gombjainak kirajzolása
    for (TftButton* btn : keyboardButtons) {
        btn->draw();
    }

    // Speciális gombok kirajzolása
    clearButton->draw();
    backspaceButton->draw();
    cancelButton->draw();
    okButton->draw();  // Az állapotát a buildKeyboard/updateInputDisplay már beállította
}

/**
 * Bemeneti mező tartalmának frissítése
 * @param redrawCursorOnly Csak a kurzort kell újrarajzolni?
 */
void VirtualKeyboardDialog::updateInputDisplay(bool redrawCursorOnly) {
    if (!redrawCursorOnly) {  // Csak akkor rajzoljuk újra a szöveget, ha nem csak a kurzor változott
        // Terület törlése
        tft.fillRect(inputDisplayX + 1, inputDisplayY + 1, inputDisplayW - 2, inputDisplayH - 2, TFT_BLACK);

        // Szöveg beállítások
        tft.setFreeFont();   // Válassz egy megfelelő fontot
        tft.setTextSize(2);  // Nagyobb betűmret az editorban
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(ML_DATUM);  // Középre-balra igazítás

        // String kirajzolása (levágva, ha túl hosszú)
        String displayString = currentInput;
        if (displayString.length() > MAX_INPUT_LEN) {
            displayString = displayString.substring(0, MAX_INPUT_LEN);
        }
        // TODO: Szélesség alapján történő levágás vagy görgetés implementálása, ha szükséges
        tft.drawString(displayString, inputDisplayX + 5, inputDisplayY + inputDisplayH / 2);

        // OK gomb állapotának frissítése az input hossza alapján
        TftButton::ButtonState newState = (currentInput.length() > 0) ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled;
        if (okButton->getState() != newState) {  // Csak akkor rajzoljuk újra, ha változott az állapota
            okButton->setState(newState);
            // okButton->draw(); // A setState már rajzol
        }
    }

    // Kurzort mindig újrarajzoljuk/töröljük az aktuális állapot szerint
    drawCursor();
}

/**
 * Csak a kurzort rajzolja vagy törli az aktuális `cursorVisible` állapot alapján.
 */
void VirtualKeyboardDialog::drawCursor() {
    // Kurzor pozíciójának kiszámítása
    tft.setFreeFont();   // Font beállítása a textWidth-hoz
    tft.setTextSize(2);  // Ugyanaz a méret, mint a szövegnél
    String displayString = currentInput;
    if (displayString.length() > MAX_INPUT_LEN) {
        displayString = displayString.substring(0, MAX_INPUT_LEN);
    }
    int cursorX = inputDisplayX + 5 + tft.textWidth(displayString);

    // Rajzolás vagy törlés (háttérszínnel rajzolás)
    uint16_t cursorColor = cursorVisible ? TFT_WHITE : TFT_BLACK;  // Fehér vagy háttérszín

    // Igazítsd ezeket az értékeket, ha kell
    uint16_t cursorTopY = inputDisplayY + 6;     // Pl. kicsit lejjebb
    uint16_t cursorHeight = inputDisplayH - 12;  // Pl. kicsit rövidebb/hosszabb
    tft.drawFastVLine(cursorX, cursorTopY, cursorHeight, cursorColor);
}

/**
 * Váltja a kurzor láthatóságát és újrarajzolja csak a kurzort.
 */
void VirtualKeyboardDialog::toggleCursor() {
    cursorVisible = !cursorVisible;   // Állapot váltása
    drawCursor();                     // Csak a kurzor újrarajzolása
    lastCursorToggleTime = millis();  // Időzítő resetelése
}

/**
 * Billentyű lenyomásának kezelése
 */
void VirtualKeyboardDialog::handleKeyPress(const char* key) {
    bool inputChanged = false;  // Jelzi, ha a szöveg változott

    if (strcmp(key, "OK") == 0) {
        if (currentInput.length() > 0) {
            targetString = currentInput;
            pParent->setDialogResponse(okButton->buildButtonTouchEvent());
        } else {
            Utils::beepError();
        }
    } else if (strcmp(key, "Cancel") == 0) {
        pParent->setDialogResponse(cancelButton->buildButtonTouchEvent());
    } else if (strcmp(key, "Bksp") == 0) {
        if (currentInput.length() > 0) {
            currentInput.remove(currentInput.length() - 1);
            inputChanged = true;
        }
    } else if (strcmp(key, "Clear") == 0) {
        if (currentInput.length() > 0) {
            currentInput = "";
            inputChanged = true;
        }
    }
    // TODO: Kezeld a Space, Shift stb. gombokat
    // else if (strcmp(key, "Space") == 0) { ... }
    else if (strlen(key) == 1) {  // Feltételezzük, hogy a többi az karakter gomb
        if (currentInput.length() < MAX_INPUT_LEN) {
            currentInput += key;
            inputChanged = true;
        } else {
            Utils::beepError();
        }
    }

    if (inputChanged) {
        cursorVisible = true;             // Legyen látható a kurzor változás után
        updateInputDisplay(false);        // Teljes mező frissítése (szöveg + kurzor)
        lastCursorToggleTime = millis();  // Időzítő reset
    }
}

/**
 * Touch esemény kezelése
 */
bool VirtualKeyboardDialog::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    // Ős osztály 'X' gombjának kezelése
    if (DialogBase::handleTouch(touched, tx, ty)) {
        return true;
    }

    // Speciális gombok ellenőrzése
    if (okButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("OK");
        return true;
    }
    if (cancelButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Cancel");
        return true;
    }
    if (backspaceButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Bksp");
        return true;
    }
    if (clearButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Clear");
        return true;
    }
    // TODO: Ellenőrizd a többi speciális gombot (Space, Shift)

    // Karakter gombok ellenőrzése
    for (TftButton* btn : keyboardButtons) {
        if (btn->handleTouch(touched, tx, ty)) {
            handleKeyPress(btn->getLabel());
            return true;
        }
    }

    // Ha egyik gomb sem kezelte
    return false;
}

/**
 * Kurzor villogtatása
 */
void VirtualKeyboardDialog::displayLoop() {
    if (millis() - lastCursorToggleTime >= CURSOR_BLINK_INTERVAL) {
        toggleCursor();
    }
}