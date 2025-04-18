#ifndef __DIALOGBASE_H
#define __DIALOGBASE_H

#include "IDialogParent.h"
#include "IGuiEvents.h"
#include "TftButton.h"
#include "utils.h"

#define DLG_BACKGROUND_COLOR TFT_DARKGREY

#define DLG_Y_POS_OFFSET 20         // A dialog a középtől ennyivel magasabban kezdődjön
#define DLG_HEADER_H 30             // Fejléc magassága
#define DLG_CLOSE_BTN_SIZE 20       // Az 'X' gomb mérete
#define DLG_CLOSE_BUTTON_ID 254     // Jobb felső sarok bezáró gomb ID-je
#define DLG_CLOSE_BUTTON_LABEL "X"  // Jobb felső sarok bezáró gomb felirata

// Sima dialóg button ID-k
#define DLG_OK_BUTTON_ID 1                        // OK gomb ID-je
#define DLG_CANCEL_BUTTON_ID DLG_CLOSE_BUTTON_ID  // Cancel gomb ID-je (ugyan az mint az 'X' gomb id-je)

// Multi dialóg gombok ID start
#define DLG_MULTI_BTN_ID_START 10  // A multi buttonok kezdő ID-je

#define DLG_BTN_H 30                                   // Gomb(ok) magassága a dialógusban
#define DLG_BUTTON_Y_GAP 10                            // A Dialog gombjai ennyivel legyenek magasabban a dialóg aljától
#define DLG_BTN_GAP 10                                 // A gombok közötti térköz pixelekben
#define DIALOG_DEFAULT_BUTTON_TEXT_PADDING_X (2 * 15)  // 15-15px X padding a gombok szövegépen

/**
 *
 */
class DialogBase : public IGuiEvents {
   private:
    const __FlashStringHelper *title;     // Flash memóriában tárolt title szöveg
    const __FlashStringHelper *message;   // Flash memóriában tárolt dialóg szöveg
    uint16_t messageY;                    // Az üzenet Y koordinátája
    uint16_t closeButtonX, closeButtonY;  // X gomb pozíciója

   protected:
    IDialogParent *pParent;  // A dialógot létrehozó objektum referencia
    TFT_eSPI &tft;           // TFT objektum referencua
    uint16_t x, y, w, h;     // A dialógus méretei
    uint16_t contentY;       // Ehhez az y értékhez igazíthatják a tartalmukat a leszármazottak

   public:
    // static constexpr TftButton::ButtonTouchEvent okButtonTouchEvent = {DLG_CANCEL_BUTTON_ID, "<ok>", TftButton::ButtonState::Pushed};
    // static constexpr TftButton::ButtonTouchEvent cancelButtonTouchEvent = {DLG_CANCEL_BUTTON_ID, "<cancel>", TftButton::ButtonState::Pushed};

    /**
     * Konstruktor
     * @param pParent Szülő képernyő
     *  @param tft TFT objektum
     * @param w dialóg szélesség
     * @param h dialóg magasság
     * @param title dialog fejléc szöveg
     * @param message dialóg bodí felirat
     * @param messageLeftAligh a szöveg balra legyen igazítva
     */
    DialogBase(IDialogParent *pParent, TFT_eSPI &tft, uint16_t w, uint16_t h, const __FlashStringHelper *title, const __FlashStringHelper *message = nullptr)
        : pParent(pParent), tft(tft), w(w), h(h), title(title), message(message) {
        // Dialóg bal felső sarkának kiszámítása a képernyő középre igzaításához
        x = (tft.width() - w) / 2;
        y = ((tft.height() - h) / 2) - DLG_Y_POS_OFFSET;  // Kicsit feljebb húzzuk a tetejét

        messageY = y + (title ? DLG_HEADER_H + 15 : 5);       // Az üzenet a fejléc utánkezdődjön, ha van fejléc
        contentY = messageY + (message != nullptr ? 15 : 0);  // A belső tér az üzenet után kezdődjön, ha van üzenet
    }

    /**
     *
     */
    virtual ~DialogBase() {}

    /**
     * Dialóg kirajzolása
     */
    virtual void drawDialog() {

        // 'Fátyol' kirajzolása
        drawOverlay();

        // Kirajzoljuk a dialógot
        tft.fillRect(x, y, w, h, DLG_BACKGROUND_COLOR);  // háttér

        // Fontváltás a title és a message kiírásához
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(1);
        tft.setTextPadding(0);

        // Title kiírása
        if (title != nullptr) {
            // Fejléc háttér kitöltése
            tft.fillRect(x, y, w, DLG_HEADER_H, TFT_NAVY);

            // Title kiírása
            tft.setTextColor(TFT_WHITE);
            tft.setTextDatum(TL_DATUM);                                                    // Bal felső sarokhoz igazítva
            tft.drawString(title, x + 10, y + 5 + (DLG_HEADER_H - tft.fontHeight()) / 2);  // Bal oldali margó 10px

            // Fejléc vonala
            tft.drawFastHLine(x, y + DLG_HEADER_H, w, TFT_WHITE);
        }

        // Dialógus kerete
        tft.drawRect(x, y, w, h, TFT_WHITE);  // keret

        // A header "X" gomb kirajzolása
        closeButtonX = x + w - DLG_CLOSE_BTN_SIZE - 5;  // Az "X" gomb pozíciója a title jobb oldalán, kis margóval a jobb szélre
        closeButtonY = y + 5;                           // Fejléc tetejéhez igazítva
        tft.setTextColor(TFT_WHITE);
        tft.setTextDatum(MC_DATUM);  // Középre igazítva az "X"-et
        tft.drawString(F(DLG_CLOSE_BUTTON_LABEL), closeButtonX + DLG_CLOSE_BTN_SIZE / 2, closeButtonY + DLG_CLOSE_BTN_SIZE / 2);

        // Üzenet kirajzolása, ha van üzenet
        if (message != nullptr) {
            tft.setTextColor(TFT_WHITE);
            tft.setTextDatum(ML_DATUM);
            tft.drawString(message, x + 10, messageY);  // Belső padding 10px
        }
    }

    /**
     * A dialóg Rotary handlere
     * Alapesetben nem csinálunk semmit, akinek kell az majd felülírja és dolgozik vele
     * @return true ha lekezelte az eseményt
     */
    virtual bool handleRotary(RotaryEncoder::EncoderState encoderState) override {

        // Alapesetben nem csinálunk semmit a rotary-ra
        return false;
    }

    /**
     * Ellenőrizzük az 'X' gomb érintését
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        // Az 'X' bezáró gomb touch vizsgálat
        if (touched) {
            if (tx >= closeButtonX && tx <= closeButtonX + DLG_CLOSE_BTN_SIZE && ty >= closeButtonY && ty <= closeButtonY + DLG_CLOSE_BTN_SIZE) {
                pParent->setDialogResponse({DLG_CLOSE_BUTTON_ID, DLG_CLOSE_BUTTON_LABEL, TftButton::ButtonState::Pushed});
                return true;
            }
        }

        return false;
    }

   private:
    /**
     * Fátyol kirajzolása
     */
    inline void drawOverlay() {

        for (int y = 0; y < tft.height(); y += 2) {
            for (int x = 0; x < tft.width(); x += 2) {
                tft.drawPixel(x, y, TFT_COLOR(90, 90, 90));  // Apró pontokkal csinálunk fátyolt
            }
        }
    }
};

#endif  // __DIALOGBASE_H