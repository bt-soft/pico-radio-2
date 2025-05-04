#ifndef __INFODIALOG_H
#define __INFODIALOG_H

#include <SI4735.h>
// #include "DialogBase.h" // Helyette MessageDialog kell
#include "MessageDialog.h"  // MessageDialog-ból származtatunk

/**
 * @brief Dialógus az SI4735 és fordítási információk megjelenítésére.
 */
class InfoDialog : public MessageDialog {  // Öröklés módosítva
   private:
    SI4735& si4735;  // Referencia az SI4735 objektumra

   public:
    /**
     * @brief Konstruktor
     *
     * @param pParent A szülő képernyő
     * @param tft A TFT_eSPI objektum
     * @param si4735 Az SI4735 objektum
     */
    InfoDialog(IDialogParent* pParent, TFT_eSPI& tft, SI4735& si4735)
        // MessageDialog konstruktor hívása csak OK gombbal (cancelText = nullptr)
        : MessageDialog(pParent, tft, 300, 200, F("System Information"), nullptr, "OK", nullptr), si4735(si4735) {

        drawDialog();
    }

    /**
     * @brief Destruktor
     */
    ~InfoDialog() {}

    /**
     * @brief Dialóg kirajzolása
     */
    void drawDialog() override {

        // Az ős osztály (MessageDialog) már kirajzolja a dialóg mindenét (keret, cím, háttér, 'X' gomb, OK gomb)

        // Információk kirajzolása
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_YELLOW, DLG_BACKGROUND_COLOR);
        tft.setTextDatum(TL_DATUM);  // Bal felső igazítás

        uint16_t textX = x + 10;                    // Bal margó
        uint16_t textY = contentY;                  // Kezdő Y pozíció a tartalomhoz
        uint8_t lineHeight = tft.fontHeight() + 3;  // Sor magassága

        // SI4735 Információk
        tft.setTextColor(TFT_CYAN, DLG_BACKGROUND_COLOR);
        tft.drawString("SI4735 Firmware:", textX, textY);
        textY += lineHeight;

        tft.setTextColor(TFT_WHITE, DLG_BACKGROUND_COLOR);
        tft.drawString("  Part Number: 0x" + String(si4735.getFirmwarePN(), HEX), textX, textY);
        textY += lineHeight;
        tft.drawString("  Firmware: " + String(si4735.getFirmwareFWMAJOR()) + "." + String(si4735.getFirmwareFWMINOR()), textX, textY);
        textY += lineHeight;
        tft.drawString("  Patch ID: 0x" + String(si4735.getFirmwarePATCHH(), HEX) + String(si4735.getFirmwarePATCHL(), HEX), textX, textY);
        textY += lineHeight;
        tft.drawString("  Component: " + String(si4735.getFirmwareCMPMAJOR()) + "." + String(si4735.getFirmwareCMPMINOR()), textX, textY);
        textY += lineHeight;
        tft.drawString("  Chip Rev: " + String(si4735.getFirmwareCHIPREV()), textX, textY);
        textY += lineHeight + 5;  // Kis szünet

        // Fordítási Információk
        tft.setTextColor(TFT_CYAN, DLG_BACKGROUND_COLOR);
        tft.drawString("Build Information:", textX, textY);
        textY += lineHeight;
        tft.setTextColor(TFT_WHITE, DLG_BACKGROUND_COLOR);
        tft.drawString("  Date: " __DATE__, textX, textY);
        textY += lineHeight;
        tft.drawString("  Time: " __TIME__, textX, textY);
        textY += lineHeight;
    }
};

#endif  // __INFODIALOG_H