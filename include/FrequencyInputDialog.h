#ifndef __FREQUENCYINPUTDIALOG_H
#define __FREQUENCYINPUTDIALOG_H

#include "Band.h"
#include "DialogBase.h"
#include "SevenSegmentFreq.h"
#include "TftButton.h"

class FrequencyInputDialog : public DialogBase {
   private:
    Band& band;
    uint16_t minFreq;
    uint16_t maxFreq;
    uint16_t currentFreq;
    SevenSegmentFreq* pSevenSegmentFreq;
    TftButton* okButton;
    TftButton* cancelButton;

    // számjegy gombok
    TftButton* digitButtons[10];

    // vezérlő gombok
    TftButton* clearButton;
    TftButton* backspaceButton;
    TftButton* dotButton;  // Új: Tizedes pont gomb

    // segéd változók
    uint8_t digitCount;
    uint32_t enteredFreq;   // Most már 32 bites, hogy a tizedes részt is tárolni tudjuk
    bool hasDot;            // Van-e tizedes pont a bevitelben?
    uint8_t decimalPlaces;  // Hány tizedes jegy van?

    /**
     * @brief Frissíti a frekvencia kijelzőt
     */
    void updateFrequencyDisplay() {
        if (hasDot) {
            pSevenSegmentFreq->freqDispl(enteredFreq /*, decimalPlaces*/);
        } else {
            pSevenSegmentFreq->freqDispl(enteredFreq);
        }
    }

    /**
     * @brief Számjegy gomb esemény kezelése
     *
     * @param digit A megnyomott számjegy
     */
    void handleDigitButton(uint8_t digit) {
        if (digitCount < 5) {
            if (hasDot) {
                if (decimalPlaces < 1) {
                    enteredFreq = enteredFreq * 10 + digit;
                    decimalPlaces++;
                    digitCount++;
                    updateFrequencyDisplay();
                }
            } else {
                enteredFreq = enteredFreq * 10 + digit;
                digitCount++;
                updateFrequencyDisplay();
            }
        }
    }

    /**
     * @brief Tizedes pont gomb esemény kezelése
     */
    void handleDotButton() {
        if (!hasDot) {
            hasDot = true;
            updateFrequencyDisplay();
        }
    }

    /**
     * @brief Törli a beírt frekvenciát
     */
    void clearFrequency() {
        enteredFreq = 0;
        digitCount = 0;
        hasDot = false;
        decimalPlaces = 0;
        updateFrequencyDisplay();
    }

    /**
     * @brief Törli az utolsó beírt számjegyet
     */
    void backspaceFrequency() {
        if (digitCount > 0) {
            if (hasDot && decimalPlaces > 0) {
                enteredFreq /= 10;
                decimalPlaces--;
            } else {
                enteredFreq /= 10;
            }
            digitCount--;
            updateFrequencyDisplay();
        }
    }

    /**
     * @brief Ellenőrzi, hogy a beírt frekvencia a megengedett tartományban van-e
     *
     * @return true ha a frekvencia érvényes, false ha nem
     */
    bool isFrequencyValid() {
        if (hasDot) {
            return enteredFreq >= (minFreq * 10) && enteredFreq <= (maxFreq * 10);
        } else {
            return enteredFreq >= minFreq && enteredFreq <= maxFreq;
        }
    }

   public:
    /**
     * @brief Konstruktor
     *
     * @param pParent A szülő képernyő
     * @param tft A TFT_eSPI objektum
     * @param band A Band objektum
     * @param currentFreq A jelenlegi frekvencia
     */
    FrequencyInputDialog(IDialogParent* pParent, TFT_eSPI& tft, Band& band, uint16_t currentFreq)
        : DialogBase(pParent, tft, 280, 240, F("Enter Frequency")), band(band), currentFreq(currentFreq) {

        minFreq = band.getCurrentBandMinimumFreq();
        maxFreq = band.getCurrentBandMaximumFreq();
        enteredFreq = currentFreq;
        digitCount = 0;
        hasDot = false;
        decimalPlaces = 0;

        // Frekvencia kijelző
        uint16_t sevenSegmentFreqX = x + (w - tft.width() / 2) / 2;  // Dinamikusan számított X pozíció
        pSevenSegmentFreq = new SevenSegmentFreq(tft, sevenSegmentFreqX, y + 20, band);

        // Gombok létrehozása
        okButton = new TftButton(DLG_OK_BUTTON_ID, tft, x + 10, y + h - DLG_BTN_H - DLG_BUTTON_Y_GAP, 80, DLG_BTN_H, "OK", TftButton::ButtonType::Pushable);
        cancelButton = new TftButton(DLG_CANCEL_BUTTON_ID, tft, x + w - 90, y + h - DLG_BTN_H - DLG_BUTTON_Y_GAP, 80, DLG_BTN_H, "Cancel", TftButton::ButtonType::Pushable);

        // számjegy gombok
        uint16_t buttonWidth = 40;
        uint16_t buttonHeight = 30;
        uint16_t buttonGapX = 5;                                                                      // Vízszintes gombok közötti távolság
        uint16_t buttonGapY = 5;                                                                      // Függőleges gombok közötti távolság
        uint8_t buttonsPerRow = 4;                                                                    // Gombok száma egy sorban (3 számjegy + 1 vezérlő)
        uint16_t totalRowWidth = (buttonWidth * buttonsPerRow) + (buttonGapX * (buttonsPerRow - 1));  // Egy sor teljes szélessége
        uint16_t startX = x + (w - totalRowWidth) / 2;                                                // Dinamikusan számított kezdő X pozíció
        uint16_t startY = y + 95;
        uint8_t id = DLG_MULTI_BTN_ID_START;

        for (int i = 0; i < 10; i++) {
            digitButtons[i] = new TftButton(id++, tft, startX + (i % 3) * (buttonWidth + buttonGapX), startY + (i / 3) * (buttonHeight + buttonGapY), buttonWidth, buttonHeight,
                                            String(i).c_str(), TftButton::ButtonType::Pushable);
            // A 9-es gomb új sorba kerüljön
            if (i == 9) {
                digitButtons[i]->setPosition(startX + 1 * (buttonWidth + buttonGapX), startY + 3 * (buttonHeight + buttonGapY));
            }
        }

        // Vezérlő gombok
        clearButton = new TftButton(id++, tft, startX + 3 * (buttonWidth + 5), startY, buttonWidth, buttonHeight, "CLR", TftButton::ButtonType::Pushable);
        backspaceButton = new TftButton(id++, tft, startX + 3 * (buttonWidth + 5), startY + (buttonHeight + 5), buttonWidth, buttonHeight, "<--", TftButton::ButtonType::Pushable);
        dotButton = new TftButton(id++, tft, startX + 2 * (buttonWidth + 5), startY + 3 * (buttonHeight + 5), buttonWidth, buttonHeight, ".", TftButton::ButtonType::Pushable);

        // Dialóg kirajzolása
        drawDialog();
    }

    /**
     * @brief Destruktor
     */
    ~FrequencyInputDialog() {
        delete pSevenSegmentFreq;
        delete okButton;
        delete cancelButton;
        delete clearButton;
        delete backspaceButton;
        delete dotButton;
        for (int i = 0; i < 10; i++) {
            delete digitButtons[i];
        }
    }

    /**
     * @brief Dialóg kirajzolása
     */
    void drawDialog() override {
        DialogBase::drawDialog();
        pSevenSegmentFreq->freqDispl(enteredFreq);
        okButton->draw();
        cancelButton->draw();
        clearButton->draw();
        backspaceButton->draw();
        dotButton->draw();
        for (int i = 0; i < 10; i++) {
            digitButtons[i]->draw();
        }
    }

    /**
     * @brief Touch esemény kezelése
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        //'X' gombot nyomtak?
        if (DialogBase::handleTouch(touched, tx, ty)) {
            return true;
        }

        // OK?
        if (okButton->handleTouch(touched, tx, ty)) {
            if (isFrequencyValid()) {
                TftButton::ButtonTouchEvent event = okButton->buildButtonTouchEvent();
                if (hasDot) {
                    /// event.value = enteredFreq;
                } else {
                    /// event.value = enteredFreq;
                }
                pParent->setDialogResponse(event);
                return true;
            }
        }

        // Cancel?
        if (cancelButton->handleTouch(touched, tx, ty)) {
            pParent->setDialogResponse(cancelButton->buildButtonTouchEvent());
            return true;
        }

        // CLR?
        if (clearButton->handleTouch(touched, tx, ty)) {
            clearFrequency();
            return true;
        }

        // '<--' ?
        if (backspaceButton->handleTouch(touched, tx, ty)) {
            backspaceFrequency();
            return true;
        }

        // '.' ?
        if (dotButton->handleTouch(touched, tx, ty)) {
            handleDotButton();
            return true;
        }

        // Számjegy gombok?
        for (int i = 0; i < 10; i++) {
            if (digitButtons[i]->handleTouch(touched, tx, ty)) {
                handleDigitButton(i);
                return true;
            }
        }

        return false;
    }
};

#endif  // __FREQUENCYINPUTDIALOG_H
