#ifndef __FREQUENCYINPUTDIALOG_H
#define __FREQUENCYINPUTDIALOG_H

#include <cmath>  // round() használatához

#include "Band.h"
#include "DialogBase.h"
#include "TftButton.h"

/**
 * @brief Frekvencia bevitel dialógus
 * A felhasználó beírhat egy frekvenciát, amit a rádióra állítunk be.
 */
class FrequencyInputDialog : public DialogBase {
   private:
    Band& band;
    uint16_t minFreq;  // A sáv belső egységében (kHz vagy 10kHz)
    uint16_t maxFreq;  // A sáv belső egységében (kHz vagy 10kHz)

    // --- Új tagváltozók a string alapú bevitelhez ---
    String currentInputString;
    bool dotPressed = false;
    uint8_t maxIntegerDigits = 5;     // Max számjegy a pont előtt (pl. 30000 kHz)
    uint8_t maxFractionalDigits = 3;  // Max számjegy a pont után (pl. 29.999 MHz)
    uint8_t integerDigits = 0;
    uint8_t fractionalDigits = 0;
    uint16_t inputDisplayX, inputDisplayY;  // String kirajzolásának koordinátái
    uint16_t inputDisplayW, inputDisplayH;  // String területének méretei
    uint16_t unitDisplayX, unitDisplayY;    // Mértékegység koordinátái
    const char* unitStr = "kHz";            // Mértékegység (kHz vagy MHz)

    bool initialValueDisplayed = true;          // Jelzi, hogy az induló érték van-e még a mezőben
    std::function<void(float)> onFrequencySet;  // Callback az OK gombhoz (float értéket ad vissza MHz/kHz-ben)

    // Gombok
    TftButton* okButton;
    TftButton* cancelButton;
    TftButton* digitButtons[10];
    TftButton* clearButton;
    TftButton* backspaceButton;
    TftButton* dotButton;

    /**
     * @brief Ellenőrzi, hogy a beírt frekvencia a megengedett tartományban van-e
     *
     * @return true ha a frekvencia érvényes, false ha nem
     */
    bool isFrequencyValid() {
        // Üres vagy csak pontot tartalmazó string nem érvényes
        if (currentInputString.length() == 0 || (currentInputString.length() == 1 && currentInputString[0] == '.')) {
            return false;
        }
        // Ha a string '.'-ra végződik, az sem teljesen érvényes még
        if (currentInputString.endsWith(".")) {
            return false;
        }

        float freqValue = currentInputString.toFloat();  // Konvertálás float-tá
        // A toFloat() 0.0-t ad vissza, ha a konverzió sikertelen (pl. "."), ezt már fentebb kezeltük

        // Konvertáljuk a beviteli értéket (ami unitStr egységben van) a sáv belső egységére (kHz vagy 10kHz)
        uint16_t freqInBandUnit;
        uint8_t bandType = band.getCurrentBandType();

        if (bandType == FM_BAND_TYPE) {                     // Bevitel MHz, sáv egysége 10kHz
            if (strcmp(unitStr, "MHz") != 0) return false;  // Belső hiba ellenőrzés
            freqInBandUnit = static_cast<uint16_t>(round(freqValue * 100.0));
        } else {                                // Bevitel kHz vagy MHz, sáv egysége kHz
            if (strcmp(unitStr, "MHz") == 0) {  // SW MHz bevitel
                freqInBandUnit = static_cast<uint16_t>(round(freqValue * 1000.0));
            } else {  // AM/LW kHz bevitel
                freqInBandUnit = static_cast<uint16_t>(round(freqValue));
            }
        }
        // DEBUG("Validation: Input='%s', Value=%.3f %s, BandUnit=%d, Min=%d, Max=%d\n",
        //       currentInputString.c_str(), freqValue, unitStr, freqInBandUnit, minFreq, maxFreq);

        // Ellenőrzés a sávhatárokra
        return freqInBandUnit >= minFreq && freqInBandUnit <= maxFreq;
    }

    /**
     * @brief Frissíti a frekvencia kijelzőt (string kirajzolása)
     */
    void updateFrequencyDisplay() {
        // Kijelző terület törlése (háttérszínnel)
        tft.fillRect(inputDisplayX, inputDisplayY, inputDisplayW, inputDisplayH, DLG_BACKGROUND_COLOR);
        // Keret rajzolása a kijelző terület köré
        tft.drawRect(inputDisplayX, inputDisplayY, inputDisplayW, inputDisplayH, TFT_WHITE);  // Vagy TFT_SILVER
        // Mértékegység terület törlése (legyen elég széles)
        tft.fillRect(unitDisplayX, inputDisplayY, 40, inputDisplayH, DLG_BACKGROUND_COLOR);

        // Szöveg tulajdonságok beállítása a frekvenciához
        tft.setFreeFont(&FreeSansBold18pt7b);  // Használjunk egy jól olvasható fontot
        tft.setTextSize(1);
        tft.setTextPadding(0);
        tft.setTextDatum(MC_DATUM);  // Középre igazítás (vertikálisan és horizontálisan)

        // Szín meghatározása érvényesség alapján (piros, ha érvénytelen)
        // Az üres stringet is érvényesnek tekintjük a kezdeti állapotban (nem piros)
        bool currentlyValid = isFrequencyValid() || currentInputString.length() == 0;
        tft.setTextColor(currentlyValid ? TFT_WHITE : TFT_RED, DLG_BACKGROUND_COLOR);

        // Bevitt string kirajzolása
        tft.drawString(currentInputString, inputDisplayX + inputDisplayW / 2, inputDisplayY + inputDisplayH / 2);

        // Mértékegység kirajzolása (kisebb, normál fonttal)
        tft.setFreeFont();           // Vissza alapértelmezett fontra
        tft.setTextSize(2);          // Kisebb méret
        tft.setTextDatum(ML_DATUM);  // Középre-balra igazítás
        tft.setTextColor(TFT_YELLOW, DLG_BACKGROUND_COLOR);
        // A unitDisplayY-t igazítsuk a frekvencia közepéhez
        tft.drawString(unitStr, unitDisplayX, inputDisplayY + inputDisplayH / 2);

        // OK gomb állapotának frissítése
        okButton->setState(isFrequencyValid() ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    }

    /**
     * @brief Számjegy gomb esemény kezelése
     *
     * @param digit A megnyomott számjegy
     */
    void handleDigitButton(uint8_t digit) {

        // Ha az iniciális érték van még kint, töröljük az első gombnyomásra
        if (initialValueDisplayed) {
            clearFrequency();  // Ez false-ra állítja az initialValueDisplayed-et is
        }

        char digitChar = digit + '0';  // Számjegy karakterré alakítása

        if (dotPressed) {
            // Tört rész bevitele
            if (fractionalDigits < maxFractionalDigits) {
                currentInputString += digitChar;
                fractionalDigits++;
                updateFrequencyDisplay();
            }
        } else {
            // Egész rész bevitele
            // Ne engedjünk 0-val kezdeni, hacsak nem ez az egyetlen számjegy
            if (integerDigits == 0 && digit == 0 && maxIntegerDigits > 1) {
                // Nem csinálunk semmit, ha 0-val akarnak kezdeni (pl. 093)
                // De ha csak 0-t akarnak beírni (pl. 0 kHz), azt engedjük, ha maxIntegerDigits=1
            } else if (integerDigits < maxIntegerDigits) {
                currentInputString += digitChar;
                integerDigits++;
                updateFrequencyDisplay();
            }
        }
    }

    /**
     * @brief Tizedes pont gomb esemény kezelése
     */
    void handleDotButton() {
        // Ha az iniciális érték van még kint, töröljük az első gombnyomásra
        if (initialValueDisplayed) {
            clearFrequency();
        }

        // Csak akkor tehetünk pontot, ha még nincs, van már egész számjegy, és a sáv enged tizedeseket
        if (!dotPressed && integerDigits > 0 && maxFractionalDigits > 0) {
            currentInputString += '.';
            dotPressed = true;
            updateFrequencyDisplay();  // Frissítjük a kijelzőt, hogy a pont megjelenjen
        }
    }

    /**
     * @brief Törli a beírt frekvenciát és reseteli az állapotokat
     */
    void clearFrequency() {
        currentInputString = "";
        dotPressed = false;
        integerDigits = 0;
        fractionalDigits = 0;
        initialValueDisplayed = false;  // Törlés után már nem az iniciális érték van ott
        updateFrequencyDisplay();
    }

    /**
     * @brief Törli az utolsó beírt karaktert
     */
    void backspaceFrequency() {
        // Backspace után már biztosan nem az iniciális érték van ott
        initialValueDisplayed = false;

        if (currentInputString.length() > 0) {
            char lastChar = currentInputString.charAt(currentInputString.length() - 1);
            currentInputString.remove(currentInputString.length() - 1);  // Utolsó karakter törlése

            if (lastChar == '.') {
                dotPressed = false;
            } else if (dotPressed) {
                fractionalDigits--;
            } else {
                integerDigits--;
            }
            updateFrequencyDisplay();
        }
    }

   public:
    /**
     * @brief Konstruktor
     *
     * @param pParent A szülő képernyő
     * @param tft A TFT_eSPI objektum
     * @param band A Band objektum
     * @param currentFreq A jelenlegi frekvencia (a rádió belső formátumában: kHz vagy 10kHz)
     */
    FrequencyInputDialog(IDialogParent* pParent, TFT_eSPI& tft, Band& band, uint16_t currentFreq, std::function<void(float)> onFrequencySet = nullptr)
        : DialogBase(pParent, tft, 280, 240, F("Enter Frequency")), band(band), onFrequencySet(onFrequencySet) {  // currentFreq nem kell a tagváltozókhoz

        BandTable& currentBand = band.getCurrentBand();
        minFreq = currentBand.pConstData->minimumFreq;
        maxFreq = currentBand.pConstData->maximumFreq;
        uint8_t bandType = currentBand.pConstData->bandType;

        // Mértékegység, max tizedesjegyek és kezdeti string formázása
        if (bandType == FM_BAND_TYPE) {
            unitStr = "MHz";
            maxFractionalDigits = 2;
            currentInputString = String(currentFreq / 100.0f, 2);
        } else if (bandType == SW_BAND_TYPE) {
            // Rövidhullámnál MHz-ben várjuk a bevitelt 3 tizedesig
            unitStr = "MHz";
            maxFractionalDigits = 3;
            currentInputString = String(currentFreq / 1000.0f, 3);
        } else {  // MW, LW
            unitStr = "kHz";
            maxFractionalDigits = 0;  // Nincs tizedesjegy kHz-nél
            currentInputString = String(currentFreq);
        }

        // Kezdeti string elemzése (dotPressed, digit count)
        int dotIndex = currentInputString.indexOf('.');
        if (dotIndex != -1) {
            dotPressed = true;
            integerDigits = dotIndex;
            // Biztosítjuk, hogy a fractionalDigits ne legyen negatív
            fractionalDigits = max(0, (int)currentInputString.length() - dotIndex - 1);
        } else {
            dotPressed = false;
            integerDigits = currentInputString.length();
            fractionalDigits = 0;
        }
        // Max integer digits (példa, finomítható)
        maxIntegerDigits = (bandType == FM_BAND_TYPE) ? 3 : 5;

        initialValueDisplayed = true;  // Kezdetben az iniciális érték van kint

        // Kijelző területének koordinátái (példa)
        inputDisplayX = x + 10;
        inputDisplayY = y + DLG_HEADER_H + 5;  // Feljebb hozzuk
        inputDisplayW = w - 60;                // Keskenyebb, hogy a unit elférjen
        inputDisplayH = 45;
        unitDisplayX = inputDisplayX + inputDisplayW + 5;  // Közvetlenül a számok után
        unitDisplayY = inputDisplayY;                      // Ugyanaz az Y, mint a számnak

        // Gombok létrehozása (OK, Cancel)
        okButton = new TftButton(DLG_OK_BUTTON_ID, tft, x + 10, y + h - DLG_BTN_H - DLG_BUTTON_Y_GAP, 80, DLG_BTN_H, "OK", TftButton::ButtonType::Pushable);
        cancelButton = new TftButton(DLG_CANCEL_BUTTON_ID, tft, x + w - 90, y + h - DLG_BTN_H - DLG_BUTTON_Y_GAP, 80, DLG_BTN_H, "Cancel", TftButton::ButtonType::Pushable);

        // Számjegy és vezérlő gombok elrendezése (maradhat a régi logika)
        uint16_t buttonWidth = 40;
        uint16_t buttonHeight = 30;
        uint16_t buttonGapX = 5;
        uint16_t buttonGapY = 5;
        uint8_t buttonsPerRow = 4;  // 0-9 + CLR, <-- , .
        uint16_t totalRowWidth = (buttonWidth * buttonsPerRow) + (buttonGapX * (buttonsPerRow - 1));
        uint16_t startX = x + (w - totalRowWidth) / 2;
        // Y pozíció a frekvencia kijelző alá
        uint16_t startY = inputDisplayY + inputDisplayH + 15;  // Nagyobb hely a kijelzőnek
        uint8_t id = DLG_MULTI_BTN_ID_START;

        // 1-9 gombok (3x3 rács)
        for (int i = 1; i <= 9; i++) {
            digitButtons[i] = new TftButton(id++, tft, startX + ((i - 1) % 3) * (buttonWidth + buttonGapX), startY + ((i - 1) / 3) * (buttonHeight + buttonGapY), buttonWidth,
                                            buttonHeight, String(i).c_str(), TftButton::ButtonType::Pushable);
        }
        // 0 gomb (középre az utolsó sorba)
        digitButtons[0] = new TftButton(id++, tft, startX + 1 * (buttonWidth + buttonGapX), startY + 3 * (buttonHeight + buttonGapY), buttonWidth, buttonHeight, "0",
                                        TftButton::ButtonType::Pushable);

        // Vezérlő gombok a 4. oszlopba és a 0 mellé
        clearButton = new TftButton(id++, tft, startX + 3 * (buttonWidth + buttonGapX), startY + 0 * (buttonHeight + buttonGapY), buttonWidth, buttonHeight, "CLR",
                                    TftButton::ButtonType::Pushable);
        backspaceButton = new TftButton(id++, tft, startX + 3 * (buttonWidth + buttonGapX), startY + 1 * (buttonHeight + buttonGapY), buttonWidth, buttonHeight, "<--",
                                        TftButton::ButtonType::Pushable);
        // Pont gomb a 0 mellé jobbra
        dotButton = new TftButton(id++, tft, startX + 2 * (buttonWidth + buttonGapX), startY + 3 * (buttonHeight + buttonGapY), buttonWidth, buttonHeight, ".",
                                  TftButton::ButtonType::Pushable);
        // Ha nincs tizedesjegy (kHz mód), akkor a pont gombot letiltjuk
        if (maxFractionalDigits == 0) {
            dotButton->setState(TftButton::ButtonState::Disabled);
        }

        // Dialógus kirajzolása (ez hívja az updateFrequencyDisplay-t is)
        drawDialog();
        // Kezdeti OK gomb állapot beállítása (lehet, hogy a kezdeti érték érvénytelen?)
        okButton->setState(isFrequencyValid() ? TftButton::ButtonState::Off : TftButton::ButtonState::Disabled);
    }

    /**
     * @brief Destruktor
     */
    ~FrequencyInputDialog() {
        delete okButton;
        delete cancelButton;
        delete clearButton;
        delete backspaceButton;
        delete dotButton;
        for (int i = 0; i < 10; i++) {
            if (digitButtons[i]) delete digitButtons[i];  // Biztonsági ellenőrzés
        }
    }

    /**
     * @brief Dialóg kirajzolása
     */
    void drawDialog() override {
        DialogBase::drawDialog();  // Alap dialógus (keret, cím, háttér, X gomb)
        updateFrequencyDisplay();  // Kezdeti frekvencia string kirajzolása
        // Gombok kirajzolása
        okButton->draw();
        cancelButton->draw();
        clearButton->draw();
        backspaceButton->draw();
        dotButton->draw();
        for (int i = 0; i < 10; i++) {
            if (digitButtons[i]) digitButtons[i]->draw();
        }
    }

    /**
     * @brief Touch esemény kezelése
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        // 'X' gomb (ős osztály kezeli)
        if (DialogBase::handleTouch(touched, tx, ty)) {
            return true;  // Az ős már beállította a response-t (Cancel/Close)
        }

        // OK gomb
        if (okButton->handleTouch(touched, tx, ty)) {
            // Csak akkor zárjuk be és állítjuk be, ha érvényes a frekvencia
            if (isFrequencyValid()) {
                float freqValue = currentInputString.toFloat();  // A beírt érték MHz/kHz-ben

                // --- Callback hívása ---
                if (onFrequencySet) {
                    onFrequencySet(freqValue);  // Átadjuk a float értéket a hívónak
                }

                // uint16_t targetFreq;  // Ez lesz kHz vagy 10kHz a rádiónak
                // uint8_t bandType = band.getCurrentBandType();
                //
                // // Konverzió a rádió egységére
                // if (bandType == FM_BAND_TYPE) {  // Bevitel MHz, cél 10kHz
                //     targetFreq = static_cast<uint16_t>(round(freqValue * 100.0));
                // } else {                                // Bevitel kHz vagy MHz, cél kHz
                //     if (strcmp(unitStr, "MHz") == 0) {  // SW MHz bevitel
                //         targetFreq = static_cast<uint16_t>(round(freqValue * 1000.0));
                //     } else {  // AM/LW kHz bevitel
                //         targetFreq = static_cast<uint16_t>(round(freqValue));
                //     }
                // }
                //
                // Band adat frissítése és rádió hangolása
                //                 BandTable& currentBand = band.getCurrentBand();
                //                 currentBand.varData.currFreq = targetFreq;
                // /////////////////////////////////////////////////////////////
                // ////                si4735.setFrequency(targetFreq);  // Itt használjuk a Si4735Utils::si4735-öt
                // DEBUG("Frequency set to: %d (%s)\n", targetFreq, (bandType == FM_BAND_TYPE) ? "10kHz" : "kHz");

                // Opcionális: BFO nullázása SSB/CW módban frekvenciaváltáskor
                // if (currentBand.varData.currMod == LSB || currentBand.varData.currMod == USB || currentBand.varData.currMod == CW) {
                //    currentBand.varData.lastBFO = 0;
                //    config.data.currentBFO = 0;
                //    // Itt kellene a BFO-t is beállítani az si4735-ben, ha szükséges
                //    // si4735.setSSBBfo(isCW ? CW_SHIFT_FREQUENCY : 0);
                // }

                // Szülő értesítése és bezárás
                pParent->setDialogResponse(okButton->buildButtonTouchEvent());
                return true;
            } else {
                // Ha OK-t nyomtak, de nem érvényes, csak adjunk hangjelzést
                Utils::beepError();
                // A gombnyomást kezeltük, de nem csináltunk semmit
                return true;  // Vagy false? Maradjon true, mert a gombot kezeltük.
            }
        }

        // Cancel gomb
        if (cancelButton->handleTouch(touched, tx, ty)) {
            pParent->setDialogResponse(cancelButton->buildButtonTouchEvent());
            return true;
        }

        // CLR gomb
        if (clearButton->handleTouch(touched, tx, ty)) {
            clearFrequency();
            return true;
        }

        // <-- (Backspace) gomb
        if (backspaceButton->handleTouch(touched, tx, ty)) {
            backspaceFrequency();
            return true;
        }

        // . (Dot) gomb
        // Csak akkor kezeljük, ha nincs letiltva (azaz maxFractionalDigits > 0)
        if (maxFractionalDigits > 0 && dotButton->handleTouch(touched, tx, ty)) {
            handleDotButton();
            return true;
        }

        // Számjegy gombok (0-9)
        for (int i = 0; i < 10; i++) {
            if (digitButtons[i] && digitButtons[i]->handleTouch(touched, tx, ty)) {
                handleDigitButton(i);
                return true;
            }
        }

        // Ha egyik gomb sem kezelte
        return false;
    }

    /**
     * @brief Rotary encoder esemény lekezelése - Nincs implementálva ebben a dialógusban
     */
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override {
        // Ebben a dialógusban a rotary encoderrel nem csinálunk semmit
        return false;
    }
};

#endif  // __FREQUENCYINPUTDIALOG_H