#ifndef __SMETER_H
#define __SMETER_H

#include <TFT_eSPI.h>

// Konstansok a kód olvashatóságának javítására
constexpr uint8_t S_METER_MAX = 208;
constexpr uint8_t S_METER_MIN = 12;
constexpr uint8_t S_METER_STEP = 12;
constexpr uint8_t S_METER_SCALE_WIDTH = 236;
constexpr uint8_t S_METER_SCALE_HEIGHT = 46;

/**
 * SMeter osztály az S-Meter kezelésére
 */
class SMeter {
   private:
    TFT_eSPI &tft;
    uint8_t smeterX;
    uint8_t smeterY;

    /**
     * RSSI érték konvertálása S-pont értékre
     */
    uint8_t rssiConverter(uint8_t rssi, bool isFM) {
        if (!isFM) {
            // HF konverzió
            if (rssi <= 1) return 12;
            if (rssi <= 2) return 24;
            if (rssi <= 3) return 36;
            if (rssi <= 4) return 48;
            if (rssi <= 10) return 48 + (rssi - 4) * 2;
            if (rssi <= 16) return 60 + (rssi - 10) * 2;
            if (rssi <= 22) return 72 + (rssi - 16) * 2;
            if (rssi <= 28) return 84 + (rssi - 22) * 2;
            if (rssi <= 34) return 96 + (rssi - 28) * 2;
            if (rssi <= 44) return 108 + (rssi - 34) * 2;
            if (rssi <= 54) return 124 + (rssi - 44) * 2;
            if (rssi <= 64) return 140 + (rssi - 54) * 2;
            if (rssi <= 74) return 156 + (rssi - 64) * 2;
            if (rssi <= 84) return 172 + (rssi - 74) * 2;
            if (rssi <= 94) return 188 + (rssi - 84) * 2;
            return (rssi > 94) ? 204 : 208;
        } else {
            // FM konverzió
            if (rssi < 1) return 36;
            if (rssi <= 2) return 60;
            if (rssi <= 8) return 84 + (rssi - 2) * 2;
            if (rssi <= 14) return 96 + (rssi - 8) * 2;
            if (rssi <= 24) return 108 + (rssi - 14) * 2;
            if (rssi <= 34) return 124 + (rssi - 24) * 2;
            if (rssi <= 44) return 140 + (rssi - 34) * 2;
            if (rssi <= 54) return 156 + (rssi - 44) * 2;
            if (rssi <= 64) return 172 + (rssi - 54) * 2;
            if (rssi <= 74) return 188 + (rssi - 64) * 2;
            return (rssi > 74) ? 204 : 208;
        }
    }

    /**
     * S-Meter érték kirajzolása
     */
    void smeter(uint8_t rssi, bool isFMMode) {
        static uint8_t prev_spoint = -1;
        uint8_t spoint = rssiConverter(rssi, isFMMode);

        if (spoint == prev_spoint) return;  // Ha nem változott, nem frissítünk
        prev_spoint = spoint;

        int tik = 0;
        int met = spoint + 2;

        // Narancs és piros sávok
        while (met > 11 && tik < 9) {
            if (tik == 0)
                tft.fillRect(smeterX + 15, smeterY + 38, 15, 6, TFT_RED);
            else
                tft.fillRect(smeterX + 20 + (tik * 12), smeterY + 38, 10, 6, TFT_ORANGE);
            met -= 12;
            tik++;
        }

        // Zöld sávok
        while (met > 15 && tik < 15) {
            tft.fillRect(smeterX + 20 + ((tik - 9) * 16) + 108, smeterY + 38, 14, 6, TFT_GREEN);
            met -= 16;
            tik++;
        }

        // Utolsó sáv
        if (tik == 15 && met > 4) {
            tft.fillRect(smeterX + 20 + 204, smeterY + 38, 3, 6, TFT_ORANGE);
        } else {
            tft.fillRect(smeterX + 22 + spoint - met, smeterY + 38, 207 - (2 + spoint) + met, 6, TFT_BLACK);
        }
    }

   public:
    /**
     * Konstruktor
     */
    SMeter(TFT_eSPI &tft, uint8_t smeterX, uint8_t smeterY) : tft(tft), smeterX(smeterX), smeterY(smeterY) {}

    /**
     * S-Meter skála kirajzolása
     */
    void drawSmeterScale() {
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.fillRect(smeterX + 2, smeterY + 6, S_METER_SCALE_WIDTH, S_METER_SCALE_HEIGHT, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);

        // Skála vonalak és számok
        for (int i = 0; i < 10; i++) {
            tft.fillRect(smeterX + 15 + (i * 12), smeterY + 24, 2, 8, TFT_WHITE);
            tft.setCursor(smeterX + 14 + (i * 12), smeterY + 13);
            tft.print(i);
        }
        for (int i = 1; i < 7; i++) {
            tft.fillRect(smeterX + 123 + (i * 16), smeterY + 24, 3, 8, TFT_RED);
            tft.setCursor(smeterX + 117 + (i * 16), smeterY + 13);
            if (i % 2 == 0) {
                tft.print("+");
                tft.print(i * 10);
            }
        }

        // Skála sávok
        tft.fillRect(smeterX + 15, smeterY + 32, 112, 3, TFT_WHITE);
        tft.fillRect(smeterX + 127, smeterY + 32, 100, 3, TFT_RED);
    }

    /**
     * S-Meter + RSSI/SNR kiírás (csak nem FM esetén)
     */
    void showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode) {
        smeter(rssi, isFMMode);

        if (isFMMode) return;

        // RSSI + SNR szöveges megjelenítése
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);

        tft.setTextDatum(TL_DATUM);
        tft.drawString("RSSI " + String(rssi) + " dBuV ", smeterX + 20, smeterY + 50);
        tft.setTextDatum(TR_DATUM);
        tft.drawString(" SNR " + String(snr) + " dB", smeterX + 180, smeterY + 50);
    }
};

#endif