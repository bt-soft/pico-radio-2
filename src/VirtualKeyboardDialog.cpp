#include "VirtualKeyboardDialog.h"

#include "StationData.h"  // Szükséges a StationData-hoz (MAX_INPUT_LEN miatt)
#include "defines.h"      // Szükséges a TFT színekhez
#include "utils.h"        // Szükséges a Utils::beepError()-hez

namespace VirtualKeyboardConstants {
    // Dialógus méretei
    constexpr uint16_t DIALOG_WIDTH = 456;
    constexpr uint16_t DIALOG_HEIGHT = 260;

    // Billentyűzet elrendezés
    constexpr uint16_t KEY_WIDTH = 40;
    constexpr uint16_t KEY_HEIGHT = 30;
    constexpr uint16_t KEY_GAP_X = 3;
    constexpr uint16_t KEY_GAP_Y = 3;
    constexpr uint16_t INPUT_DISPLAY_HEIGHT = 40;
    constexpr uint16_t INPUT_DISPLAY_MARGIN_X = 10;
    constexpr uint16_t INPUT_DISPLAY_MARGIN_Y = 5;
    constexpr uint16_t INPUT_BORDER_WIDTH = 1;
    constexpr uint16_t INPUT_TEXT_PADDING_X = 5;
    constexpr uint16_t KEYBOARD_START_Y_OFFSET = INPUT_DISPLAY_HEIGHT + INPUT_DISPLAY_MARGIN_Y * 2;
    constexpr uint16_t CURSOR_Y_OFFSET = 6; // Felső/alsó margó a kurzornak a beviteli mezőn belül
    constexpr uint16_t CURSOR_HEIGHT_REDUCTION = CURSOR_Y_OFFSET * 2; // Mennyivel rövidebb a kurzor, mint a mező

} // namespace VirtualKeyboardConstants
#define MAX_INPUT_LEN STATION_NAME_BUFFER_SIZE - 1  // Maximális bemeneti hossz (a StationData.h-ból)
#define SPACE_BTN_WIDTH 120                         // Space gomb szélessége (ha szükséges)

/**
 * Konstruktor
 */
VirtualKeyboardDialog::VirtualKeyboardDialog(IDialogParent* pParent, TFT_eSPI& tft, const __FlashStringHelper* title, String& target)
    : DialogBase(pParent, tft, VirtualKeyboardConstants::DIALOG_WIDTH, VirtualKeyboardConstants::DIALOG_HEIGHT, title),
      targetString(target),
      // originalString(target), // Ezt nem használjuk jelenleg, törölhető?
      originalString(target),  // Eredeti string mentése
      currentInput(target),    // Kezdetben a szerkesztendő stringgel inicializáljuk
      shiftActive(false)       // Shift kezdetben inaktív
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
    delete spaceButton;
    delete shiftButton;  // Shift gomb törlése

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
    using namespace VirtualKeyboardConstants;
    // Bemeneti mező koordinátái
    inputDisplayX = x + INPUT_DISPLAY_MARGIN_X;
    inputDisplayY = y + DLG_HEADER_H + INPUT_DISPLAY_MARGIN_Y;
    inputDisplayW = w - (INPUT_DISPLAY_MARGIN_X * 2);  // Teljes szélesség - margók
    inputDisplayH = INPUT_DISPLAY_HEIGHT;

    // Billentyűzet kezdő pozíciója
    uint16_t keyboardStartY = inputDisplayY + INPUT_DISPLAY_HEIGHT + INPUT_DISPLAY_MARGIN_Y; // Közvetlenül az input mező alá

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
            // Nagybetűs kezdőállapot, ha a shift aktív (bár a konstruktorban még nem az)
            // A redrawKeyboardKeys majd beállítja a helyeset a drawDialog előtt/alatt
            char displayChar = shiftActive ? toupper(rowLayout[c]) : rowLayout[c];
            char displayKeyStr[2] = {displayChar, '\0'};
            TftButton* keyButton =
                new TftButton(btnId++, tft, startX + c * (KEY_WIDTH + KEY_GAP_X), currentY, KEY_WIDTH, KEY_HEIGHT, displayKeyStr, TftButton::ButtonType::Pushable);
            keyboardButtons.push_back(keyButton);
        }
        currentY += KEY_HEIGHT + KEY_GAP_Y;
    }

    // Speciális gombok létrehozása (OK, Cancel, <--, Clear)
    // Ezeket a karakter gombok alá helyezzük el
    uint16_t specialBtnY = currentY + KEY_GAP_Y;           // Egy kis extra rés
    uint16_t btnWidthSpecialBase = 60;                     // Alap speciális gomb szélesség
    uint16_t backspaceBtnWidth = btnWidthSpecialBase - 15;  // <-- gomb keskenyebb
    uint16_t cancelBtnWidth = btnWidthSpecialBase + 15;     // Cancel gomb szélesebb
    // Teljes szélesség: Shift + Clear + <-- + Space + Cancel + OK + 5 rés
    uint16_t totalSpecialWidth = (btnWidthSpecialBase * 3) + backspaceBtnWidth + cancelBtnWidth + SPACE_BTN_WIDTH + (KEY_GAP_X * 5);  // Újraszámolt teljes szélesség
    uint16_t startXSpecial = x + (w - totalSpecialWidth) / 2;                                                                         // Újraszámolt kezdő X

    // Gombok létrehozása sorban
    uint16_t currentXSpecial = startXSpecial;

    shiftButton =
        new TftButton(btnId++, tft, currentXSpecial, specialBtnY, btnWidthSpecialBase, KEY_HEIGHT, "Shift", TftButton::ButtonType::Toggleable, TftButton::ButtonState::Off);
    currentXSpecial += btnWidthSpecialBase + KEY_GAP_X;

    clearButton = new TftButton(btnId++, tft, currentXSpecial, specialBtnY, btnWidthSpecialBase, KEY_HEIGHT, "Clear", TftButton::ButtonType::Pushable);
    currentXSpecial += btnWidthSpecialBase + KEY_GAP_X;

    backspaceButton = new TftButton(btnId++, tft, currentXSpecial, specialBtnY, backspaceBtnWidth, KEY_HEIGHT, "<--", TftButton::ButtonType::Pushable);  // Keskenyebb <--
    currentXSpecial += backspaceBtnWidth + KEY_GAP_X;

    spaceButton = new TftButton(btnId++, tft, currentXSpecial, specialBtnY, SPACE_BTN_WIDTH, KEY_HEIGHT, "Space", TftButton::ButtonType::Pushable);
    currentXSpecial += SPACE_BTN_WIDTH + KEY_GAP_X;

    cancelButton =
        new TftButton(DLG_CANCEL_BUTTON_ID, tft, currentXSpecial, specialBtnY, cancelBtnWidth, KEY_HEIGHT, "Cancel", TftButton::ButtonType::Pushable);  // Szélesebb Cancel
    currentXSpecial += cancelBtnWidth + KEY_GAP_X;

    okButton = new TftButton(DLG_OK_BUTTON_ID, tft, currentXSpecial, specialBtnY, btnWidthSpecialBase, KEY_HEIGHT, "OK", TftButton::ButtonType::Pushable);

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
    shiftButton->draw();
    clearButton->draw();
    backspaceButton->draw();
    spaceButton->draw();
    cancelButton->draw();
    okButton->draw();  // Az állapotát a buildKeyboard/updateInputDisplay már beállította
}

/**
 * Bemeneti mező tartalmának frissítése
 * @param redrawCursorOnly Csak a kurzort kell újrarajzolni?
 */
void VirtualKeyboardDialog::updateInputDisplay(bool redrawCursorOnly) {
    using namespace VirtualKeyboardConstants;
    if (!redrawCursorOnly) { // Csak akkor rajzoljuk újra a szöveget, ha nem csak a kurzor változott
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
        tft.drawString(displayString, inputDisplayX + INPUT_TEXT_PADDING_X, inputDisplayY + inputDisplayH / 2);

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
    using namespace VirtualKeyboardConstants;
    // Kurzor pozíciójának kiszámítása
    tft.setFreeFont();   // Font beállítása a textWidth-hoz
    tft.setTextSize(2);  // Ugyanaz a méret, mint a szövegnél
    String displayString = currentInput;
    if (displayString.length() > MAX_INPUT_LEN) {
        displayString = displayString.substring(0, MAX_INPUT_LEN);
    }
    int cursorX = inputDisplayX + INPUT_TEXT_PADDING_X + tft.textWidth(displayString);

    // Rajzolás vagy törlés (háttérszínnel rajzolás)
    uint16_t cursorColor = cursorVisible ? TFT_WHITE : TFT_BLACK;  // Fehér vagy háttérszín

    // Igazítsd ezeket az értékeket, ha kell
    uint16_t cursorTopY = inputDisplayY + CURSOR_Y_OFFSET;
    uint16_t cursorHeight = inputDisplayH - CURSOR_HEIGHT_REDUCTION;
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
 * Újrarajzolja a billentyűzet karaktergombjait a shift állapotnak megfelelően.
 */
void VirtualKeyboardDialog::redrawKeyboardKeys() {
    for (TftButton* btn : keyboardButtons) {
        const char* currentLabel = btn->getLabel();
        if (strlen(currentLabel) == 1) {  // Csak karaktergombokkal foglalkozunk
            char originalChar = currentLabel[0];
            // Mindig az eredeti kisbetűs karakterből indulunk ki (feltételezve, hogy az van a layoutban)
            char lowerChar = tolower(originalChar);
            char newChar = shiftActive ? toupper(lowerChar) : lowerChar;
            char newLabel[2] = {newChar, '\0'};
            btn->setLabel(newLabel);  // Ez újrarajzolja a gombot
        }
    }
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
    } else if (strcmp(key, "<--") == 0) {
        if (currentInput.length() > 0) {
            currentInput.remove(currentInput.length() - 1);
            inputChanged = true;
        }
    } else if (strcmp(key, "Clear") == 0) {
        if (currentInput.length() > 0) {
            currentInput = "";
            inputChanged = true;
        }
    } else if (strcmp(key, "Space") == 0) {
        if (currentInput.length() < MAX_INPUT_LEN) {
            currentInput += " ";  // Szóköz hozzáadása
            inputChanged = true;
        } else {
            Utils::beepError();
        }
    } else if (strcmp(key, "Shift") == 0) {
        shiftActive = !shiftActive;  // Shift állapot váltása
        // Shift gomb állapotának frissítése (On, ha aktív, Off, ha nem)
        shiftButton->setState(shiftActive ? TftButton::ButtonState::On : TftButton::ButtonState::Off);
        // Billentyűk újrarajzolása a megfelelő kis/nagybetűvel
        redrawKeyboardKeys();
        // Nem kell inputChanged, mert a szöveg nem változott
    } else if (strlen(key) == 1) {  // Karakter gomb
        char pressedChar = key[0];
        // Ha a Shift aktív, nagybetűsítünk
        if (shiftActive) {
            pressedChar = toupper(pressedChar);
        }

        if (currentInput.length() < MAX_INPUT_LEN) {
            currentInput += pressedChar;  // Javítás: a módosított karaktert fűzzük hozzá
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
    if (backspaceButton->handleTouch(touched, tx, ty)) {  // <-- Javítva itt is
        handleKeyPress("<--");
        return true;
    }
    if (clearButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Clear");
        return true;
    }
    if (spaceButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Space");
        return true;
    }
    if (shiftButton->handleTouch(touched, tx, ty)) {
        handleKeyPress("Shift");
        return true;
    }

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