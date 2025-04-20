#ifndef __VIRTUALKEYBOARDDIALOG_H
#define __VIRTUALKEYBOARDDIALOG_H

#include <vector>  // std::vector használatához

#include "DialogBase.h"
#include "TftButton.h"

class VirtualKeyboardDialog : public DialogBase {
   private:
    String& targetString;   // Referencia a szerkesztendő stringre
    String originalString;  // Eredeti string mentése Cancel esetére
    String currentInput;    // A dialógusban szerkesztett string

    // Gombok tárolása
    std::vector<TftButton*> keyboardButtons;
    TftButton* okButton;
    TftButton* cancelButton;
    TftButton* backspaceButton;
    TftButton* clearButton;
    // TODO: egyéb speciális gombok (Shift, Space, stb.)

    // Billentyűzet kiosztás
    const char* keyLayout[4] = {
        "1234567890",  //
        "QWERTZUIOP",  //
        "ASDFGHJKL",   //
        "YXCVBNM.,",   //
    };
    uint8_t keyRows = 4;

    // Kijelző terület koordinátái
    uint16_t inputDisplayX, inputDisplayY, inputDisplayW, inputDisplayH;

    // Kurzor villogásához szükséges változók
    unsigned long lastCursorToggleTime = 0;
    bool cursorVisible = true;
    static const unsigned long CURSOR_BLINK_INTERVAL = 500;  // Villogás sebessége (ms)

    void buildKeyboard();
    void updateInputDisplay(bool redrawCursorOnly = false);  // Módosítás: Opcionális paraméter
    void handleKeyPress(const char* key);

    // Kurzorral kapcsolatos metódusok
    void drawCursor();    // Csak a kurzort rajzoló/törlő metódus
    void toggleCursor();  // Kurzort váltó metódus

   public:
    VirtualKeyboardDialog(IDialogParent* pParent, TFT_eSPI& tft, const __FlashStringHelper* title, String& target);
    ~VirtualKeyboardDialog();

    void drawDialog() override;
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;

    // Rotary - egyelőre nem csinálunk benne semmit
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override { return false; };

    // Kurzor villogtatása
    void displayLoop() override;
};

#endif  // __VIRTUALKEYBOARDDIALOG_H
