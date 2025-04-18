#ifndef __MESSAGEDIALOG_H
#define __MESSAGEDIALOG_H

#include "DialogBase.h"
#include "TftButton.h"

/**
 *
 */
class MessageDialog : public DialogBase {
   private:
    TftButton *okButton;
    TftButton *cancelButton;

   public:
    /**
     *
     */
    MessageDialog(IDialogParent *pParent, TFT_eSPI &tft, uint16_t w, uint16_t h, const __FlashStringHelper *title, const __FlashStringHelper *message, const char *okText = "OK",
                  const char *cancelText = nullptr)
        : DialogBase(pParent, tft, w, h, title, message), cancelButton(nullptr) {

        // Kiszedjük a legnagyobb gomb felirat szélességét (10-10 pixel a szélén)
        uint8_t okButtonWidth = tft.textWidth(okText) + DIALOG_DEFAULT_BUTTON_TEXT_PADDING_X;                           // OK gomb szöveg szélessége + padding a gomb széleihez
        uint8_t cancelButtonWidth = cancelText ? tft.textWidth(cancelText) + DIALOG_DEFAULT_BUTTON_TEXT_PADDING_X : 0;  // Cancel gomb szöveg szélessége, ha van

        // Ha van Cancel gomb, akkor a két gomb közötti gap-et is figyelembe vesszük
        uint16_t totalButtonWidth = cancelButtonWidth > 0 ? okButtonWidth + cancelButtonWidth + DLG_BTN_GAP : okButtonWidth;
        uint16_t okX = x + (w - totalButtonWidth) / 2;  // Az OK gomb X pozíciója -> a gombok kezdő X pozíciója

        // Gombok Y pozíció
        uint16_t buttonY = y + h - DLG_BTN_H - DLG_BUTTON_Y_GAP;

        // OK gomb
        okButton = new TftButton(DLG_OK_BUTTON_ID, tft, okX, buttonY, okButtonWidth, DLG_BTN_H, okText, TftButton::ButtonType::Pushable);

        // Cancel gomb, ha van
        if (cancelText) {
            uint16_t cancelX = okX + okButtonWidth + DLG_BTN_GAP;  // A Cancel gomb X pozíciója
            cancelButton = new TftButton(DLG_CANCEL_BUTTON_ID, tft, cancelX, buttonY, cancelButtonWidth, DLG_BTN_H, cancelText, TftButton::ButtonType::Pushable);
        }

        // Ki is rajzoljuk a dialógust
        drawDialog();
    }

    /**
     * Dialóg destruktor
     */
    virtual ~MessageDialog() {
        delete okButton;
        if (cancelButton) {
            delete cancelButton;
        }
    }

    /**
     * Dialóg megjelenítése
     * (Virtual így meg kell hívni az ős metódusát!!)
     */
    virtual void drawDialog() override {

        // Mivel ez a metódus virtual, ezért meg kell hívni az ős metódusát is!!
        DialogBase::drawDialog();

        // Kirajzoljuk az OK gombot
        okButton->draw();

        // Ha van Cancel gomb, akkor kirajzoljuk azt is
        if (cancelButton) {
            cancelButton->draw();
        }
    }

    /**
     * Rotary encoder esemény lekezelése
     */
    virtual bool handleRotary(RotaryEncoder::EncoderState encoderState) override {
        // A messageDialognál a click az az OK gomb megnyomásával azonos eseményt vált ki
        if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
            DialogBase::pParent->setDialogResponse(okButton->buildButtonTouchEvent());
            return true;
        }

        return false;
    }

    /**
     * Dialóg Touch esemény lekezelése
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        // Ha az ős lekezelte már az 'X' gomb eseményét, akkor nem megyünk tovább
        if (DialogBase::handleTouch(touched, tx, ty)) {
            return true;
        }

        // OK gomb touch vizsgálat
        if (okButton->handleTouch(touched, tx, ty)) {
            DialogBase::pParent->setDialogResponse(okButton->buildButtonTouchEvent());
            return true;

        } else if (cancelButton != nullptr) {
            if (cancelButton->handleTouch(touched, tx, ty)) {
                DialogBase::pParent->setDialogResponse(cancelButton->buildButtonTouchEvent());
                return true;
            }
        }

        return false;
    }
};

#endif  // __MESSAGEDIALOG_H