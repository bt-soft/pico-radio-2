#ifndef __MULTIBUTTONDIALOG_H
#define __MULTIBUTTONDIALOG_H

#include <functional>

#define MULTI_BTN_W 80  // Gombok szélessége
#define MULTI_BTN_H 30  // Gombok magassága

class MultiButtonDialog : public DialogBase {
   private:
    TftButton **buttonsArray;  // A gombok mutatóinak tömbje
    uint8_t buttonCount;       // Gombok száma

    // std::function<void(const char *)> onButtonClicked;  // Callback függvény
    std::function<void(TftButton::ButtonTouchEvent event)> onButtonClicked;  // Callback függvény

    /**
     * Gombok elhelyezése a dialógusban
     */
    void placeButtons() {
        if (!buttonsArray || buttonCount == 0) return;

        uint16_t maxRowWidth = w - 20;
        uint8_t buttonsPerRow = 0, rowCount = 0;
        uint16_t totalWidth = 0;

        // Kiszámoljuk a gombok elrendezését
        // Próbáljuk feltölteni egy sort, amíg elférnek a gombok
        for (uint8_t i = 0; i < buttonCount; i++) {
            uint16_t nextWidth = totalWidth + buttonsArray[i]->getWidth() + (buttonsPerRow > 0 ? DLG_BTN_GAP : 0);
            if (nextWidth > maxRowWidth) {
                break;  // Ha már nem fér el, kilépünk
            }

            totalWidth = nextWidth;
            buttonsPerRow++;
        }

        rowCount = (buttonCount + buttonsPerRow - 1) / buttonsPerRow;  // Felkerekítés

        uint16_t buttonHeight = DLG_BTN_H;
        uint16_t startY = contentY;
        uint16_t startX = x + (w - totalWidth) / 2;

        // Gombok pozicionálása
        for (uint8_t i = 0, row = 0, col = 0; i < buttonCount; i++) {
            buttonsArray[i]->setPosition(startX + col * (buttonsArray[i]->getWidth() + DLG_BTN_GAP), startY);
            col++;
            if (col >= buttonsPerRow) {
                col = 0;
                row++;
                startY += buttonHeight + DLG_BTN_GAP;
            }
        }
    }

    /**
     * Gombok tömbjének létrehozása a gomb feliratok alapján
     */
    void buildButtonArray(const char *buttonLabels[], const char *currentActiveButtonLabel) {

        if (!buttonLabels || buttonCount == 0) return;

        buttonsArray = new TftButton *[buttonCount];
        uint8_t id = DLG_MULTI_BTN_ID_START;

        for (uint8_t i = 0; i < buttonCount; i++) {

            // Ha a gomb felirat megegyezik a jelenlegi aktív gomb feliratával, akkor az állapota CurrentActive lesz
            bool currentActiveButton = currentActiveButtonLabel != nullptr and STREQ(buttonLabels[i], currentActiveButtonLabel);

            TftButton::TftButton::ButtonState state = currentActiveButton ? TftButton::ButtonState::CurrentActive : TftButton::ButtonState::Off;
            buttonsArray[i] = new TftButton(id++, tft, MULTI_BTN_W, MULTI_BTN_H, buttonLabels[i], TftButton::ButtonType::Pushable, state);
        }
    }

   public:
    /**
     * MultiButtonDialog létrehozása
     *
     * @param pParent A dialóg szülő képernyője
     * @param pTft Az TFT_eSPI objektumra mutató referencia.
     * @param w A párbeszédpanel szélessége.
     * @param h A párbeszédpanel magassága.
     * @param title A dialógus címe (opcionális).
     * @param buttonLabels A gombok feliratainak a tömbje.
     * @param buttonCount A gombok száma
     */
    MultiButtonDialog(IDialogParent *pParent, TFT_eSPI &tft, uint16_t w, uint16_t h, const __FlashStringHelper *title, const char *buttonLabels[] = nullptr,
                      uint8_t buttonCount = 0, std::function<void(TftButton::ButtonTouchEvent)> onButtonClicked = nullptr, const char *currentActiveButtonLabel = nullptr)
        : DialogBase(pParent, tft, w, h, title), buttonCount(buttonCount), onButtonClicked(onButtonClicked) {

        // Legyártjuk a gombok töbmjét
        buildButtonArray(buttonLabels, currentActiveButtonLabel);

        // Elhelyezzük a dialógon a gombokat
        placeButtons();

        // Ki is rajzoljuk a dialógust
        drawDialog();
    }

    /**
     * Dialóg destruktor
     */
    virtual ~MultiButtonDialog() {
        // Egyenként töröljük a gombokat
        for (uint8_t i = 0; i < buttonCount; i++) {
            delete buttonsArray[i];
        }

        // Töröljük a gombok tömbjét is
        delete[] buttonsArray;
    }

    /**
     * Dialóg kirajzolása
     */
    void drawDialog() override {

        // Dialóg kirajzolása
        DialogBase::drawDialog();

        // Gombok megjelenítése
        for (uint8_t i = 0; i < buttonCount; i++) {
            buttonsArray[i]->draw();
        }
    }

    /**
     * Touch esemény kezelése
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        // Ha az ős lekezelte már az 'X' gomb eseményét, akkor nem megyünk tovább
        if (DialogBase::handleTouch(touched, tx, ty)) {
            return true;
        }

        // Végigmegyünk az összes gombon
        for (uint8_t i = 0; i < buttonCount; i++) {

            // Ha valamelyik reagált a touch eseményre, akkor beállítjuk a visszatérési értéket és kilépünk a ciklusból
            if (buttonsArray[i]->handleTouch(touched, tx, ty)) {
                // Ha van callback, akkor meghívjuk
                if (onButtonClicked) {
                    DialogBase::drawDlgOverlay();  // Kirajzoljuk a dialógus overlay-t, hogy a gomb lenyomásakor ne legyen lefagyás érzés
                    onButtonClicked(buttonsArray[i]->buildButtonTouchEvent());
                }

                // A dialog bezárásához beállítjuk az eseményt
                DialogBase::pParent->setDialogResponse(buttonsArray[i]->buildButtonTouchEvent());
                return true;
            }
        }

        return false;
    }
};

#endif  // __MULTIBUTTONDIALOG_H
