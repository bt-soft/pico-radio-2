#ifndef __SMETER_H
#define __SMETER_H

#include <TFT_eSPI.h>

#include "defines.h"  // Színekhez (TFT_BLACK, stb.)

namespace SMeterConstants {
// Skála méretei és pozíciója
constexpr uint8_t ScaleWidth = 236;
constexpr uint8_t ScaleHeight = 46;
constexpr uint8_t ScaleStartXOffset = 2;
constexpr uint8_t ScaleStartYOffset = 6;
constexpr uint8_t ScaleEndXOffset = ScaleStartXOffset + ScaleWidth;
constexpr uint8_t ScaleEndYOffset = ScaleStartYOffset + ScaleHeight;

// S-Pont skála rajzolása
constexpr uint8_t SPointStartX = 15;
constexpr uint8_t SPointY = 24;
constexpr uint8_t SPointTickWidth = 2;
constexpr uint8_t SPointTickHeight = 8;
constexpr uint8_t SPointNumberY = 13;
constexpr uint8_t SPointSpacing = 12;
constexpr uint8_t SPointCount = 10;  // 0-9

// Plusz skála rajzolása
constexpr uint8_t PlusScaleStartX = 123;
constexpr uint8_t PlusScaleY = 24;
constexpr uint8_t PlusScaleTickWidth = 3;
constexpr uint8_t PlusScaleTickHeight = 8;
constexpr uint8_t PlusScaleNumberY = 13;
constexpr uint8_t PlusScaleSpacing = 16;
constexpr uint8_t PlusScaleCount = 6;  // +10-től +60-ig

// Skála sávok rajzolása
constexpr uint8_t SBarY = 32;
constexpr uint8_t SBarHeight = 3;
constexpr uint8_t SBarSPointWidth = 112;
constexpr uint8_t SBarPlusStartX = 127;
constexpr uint8_t SBarPlusWidth = 100;

// Mérősáv rajzolása
constexpr uint8_t MeterBarY = 38;
constexpr uint8_t MeterBarHeight = 6;
constexpr uint8_t MeterBarRedStartX = 15;
constexpr uint8_t MeterBarRedWidth = 15;
constexpr uint8_t MeterBarOrangeStartX = 20;
constexpr uint8_t MeterBarOrangeWidth = 10;
constexpr uint8_t MeterBarOrangeSpacing = 12;
constexpr uint8_t MeterBarGreenStartX = 128;
constexpr uint8_t MeterBarGreenWidth = 14;
constexpr uint8_t MeterBarGreenSpacing = 16;
constexpr uint8_t MeterBarFinalOrangeStartX = 224;
constexpr uint8_t MeterBarFinalOrangeWidth = 3;
constexpr uint8_t MeterBarMaxPixelValue = 208;
constexpr uint8_t MeterBarBlackFillOffset = 22;
constexpr uint8_t MeterBarBlackFillBaseWidth = 207;
constexpr uint8_t MeterBarSPointLimit = 9;  // S9-ig
constexpr uint8_t MeterBarTotalLimit = 15;  // S9 + 6 plusz pont

// Szöveges címkék rajzolása
constexpr uint8_t RssiLabelXOffset = 20;
constexpr uint8_t SnrLabelXOffset = 180;
constexpr uint8_t SignalLabelYOffset = 60;  // Közös Y eltolás az RSSI és SNR címkékhez (Lejjebb hozva)

// RSSI -> S-Pont konverziós konstansok (HF)
constexpr uint8_t HF_RSSI_S1 = 1, HF_RSSI_S2 = 2, HF_RSSI_S3 = 3, HF_RSSI_S4 = 4;
constexpr uint8_t HF_RSSI_S5_START = 10, HF_RSSI_S6_START = 16, HF_RSSI_S7_START = 22, HF_RSSI_S8_START = 28, HF_RSSI_S9_START = 34;
constexpr uint8_t HF_RSSI_P10_START = 44, HF_RSSI_P20_START = 54, HF_RSSI_P30_START = 64, HF_RSSI_P40_START = 74, HF_RSSI_P50_START = 84, HF_RSSI_P60_START = 94;
constexpr uint8_t HF_PIXEL_S0 = 12, HF_PIXEL_S1 = 12, HF_PIXEL_S2 = 24, HF_PIXEL_S3 = 36, HF_PIXEL_S4 = 48;
constexpr uint8_t HF_PIXEL_S5 = 60, HF_PIXEL_S6 = 72, HF_PIXEL_S7 = 84, HF_PIXEL_S8 = 96, HF_PIXEL_S9 = 108;
constexpr uint8_t HF_PIXEL_P10 = 124, HF_PIXEL_P20 = 140, HF_PIXEL_P30 = 156, HF_PIXEL_P40 = 172, HF_PIXEL_P50 = 188, HF_PIXEL_P60 = 204;
constexpr uint8_t HF_PIXEL_MAX = 208;
constexpr uint8_t HF_PIXEL_STEP = 2;

// RSSI -> S-Pont konverziós konstansok (FM)
constexpr uint8_t FM_RSSI_S3 = 1, FM_RSSI_S5 = 2;
constexpr uint8_t FM_RSSI_S7_START = 8, FM_RSSI_S8_START = 14, FM_RSSI_S9_START = 24;
constexpr uint8_t FM_RSSI_P10_START = 34, FM_RSSI_P20_START = 44, FM_RSSI_P30_START = 54, FM_RSSI_P40_START = 64, FM_RSSI_P50_START = 74;
constexpr uint8_t FM_PIXEL_S0 = 36, FM_PIXEL_S3 = 36, FM_PIXEL_S5 = 60;
constexpr uint8_t FM_PIXEL_S7 = 84, FM_PIXEL_S8 = 96, FM_PIXEL_S9 = 108;
constexpr uint8_t FM_PIXEL_P10 = 124, FM_PIXEL_P20 = 140, FM_PIXEL_P30 = 156, FM_PIXEL_P40 = 172, FM_PIXEL_P50 = 188, FM_PIXEL_P60 = 204;
constexpr uint8_t FM_PIXEL_MAX = 208;
constexpr uint8_t FM_PIXEL_STEP = 2;

// Kezdeti állapot a prev_spoint-hoz
constexpr uint8_t InitialPrevSpoint = 0xFF;  // uint8_t-hoz 0xFF használata -1 helyett
}  // namespace SMeterConstants

/**
 * SMeter osztály az S-Meter kezelésére
 */
class SMeter {
   private:
    TFT_eSPI &tft;
    uint32_t smeterX;
    uint32_t smeterY;

    /**
     * RSSI érték konvertálása S-pont értékre
     */
    uint8_t rssiConverter(uint8_t rssi, bool isFM) {
        using namespace SMeterConstants;
        if (!isFM) {
            // HF konverzió
            if (rssi <= HF_RSSI_S1) return HF_PIXEL_S0;
            if (rssi <= HF_RSSI_S2) return HF_PIXEL_S1 + (rssi - HF_RSSI_S1) * (HF_PIXEL_S2 - HF_PIXEL_S1);  // Pontosabb interpoláció
            if (rssi <= HF_RSSI_S3) return HF_PIXEL_S2 + (rssi - HF_RSSI_S2) * (HF_PIXEL_S3 - HF_PIXEL_S2);
            if (rssi <= HF_RSSI_S4) return HF_PIXEL_S3 + (rssi - HF_RSSI_S3) * (HF_PIXEL_S4 - HF_PIXEL_S3);
            if (rssi <= HF_RSSI_S5_START) return HF_PIXEL_S4 + (rssi - HF_RSSI_S4) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_S6_START) return HF_PIXEL_S5 + (rssi - HF_RSSI_S5_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_S7_START) return HF_PIXEL_S6 + (rssi - HF_RSSI_S6_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_S8_START) return HF_PIXEL_S7 + (rssi - HF_RSSI_S7_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_S9_START) return HF_PIXEL_S8 + (rssi - HF_RSSI_S8_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P10_START) return HF_PIXEL_S9 + (rssi - HF_RSSI_S9_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P20_START) return HF_PIXEL_P10 + (rssi - HF_RSSI_P10_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P30_START) return HF_PIXEL_P20 + (rssi - HF_RSSI_P20_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P40_START) return HF_PIXEL_P30 + (rssi - HF_RSSI_P30_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P50_START) return HF_PIXEL_P40 + (rssi - HF_RSSI_P40_START) * HF_PIXEL_STEP;
            if (rssi <= HF_RSSI_P60_START) return HF_PIXEL_P50 + (rssi - HF_RSSI_P50_START) * HF_PIXEL_STEP;
            return (rssi > HF_RSSI_P60_START) ? HF_PIXEL_P60 : HF_PIXEL_MAX;  // Vagy csak HF_PIXEL_P60? A kód 204/208-at adott.
        } else {
            // FM konverzió
            if (rssi < FM_RSSI_S3) return FM_PIXEL_S0;
            if (rssi <= FM_RSSI_S5) return FM_PIXEL_S3 + (rssi - FM_RSSI_S3) * (FM_PIXEL_S5 - FM_PIXEL_S3);  // Interpoláció
            if (rssi <= FM_RSSI_S7_START) return FM_PIXEL_S5 + (rssi - FM_RSSI_S5) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_S8_START) return FM_PIXEL_S7 + (rssi - FM_RSSI_S7_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_S9_START) return FM_PIXEL_S8 + (rssi - FM_RSSI_S8_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_P10_START) return FM_PIXEL_S9 + (rssi - FM_RSSI_S9_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_P20_START) return FM_PIXEL_P10 + (rssi - FM_RSSI_P10_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_P30_START) return FM_PIXEL_P20 + (rssi - FM_RSSI_P20_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_P40_START) return FM_PIXEL_P30 + (rssi - FM_RSSI_P30_START) * FM_PIXEL_STEP;
            if (rssi <= FM_RSSI_P50_START) return FM_PIXEL_P40 + (rssi - FM_RSSI_P40_START) * FM_PIXEL_STEP;
            return (rssi > FM_RSSI_P50_START) ? FM_PIXEL_P60 : FM_PIXEL_MAX;  // Vagy csak FM_PIXEL_P50? A kód 204/208-at adott.
        }
    }

    /**
     * S-Meter érték kirajzolása
     */
    void smeter(uint8_t rssi, bool isFMMode) {
        using namespace SMeterConstants;
        static uint8_t prev_spoint = InitialPrevSpoint;
        uint8_t spoint = rssiConverter(rssi, isFMMode);

        if (spoint == prev_spoint) return;  // Ha nem változott, nem frissítünk
        prev_spoint = spoint;

        int tik = 0;
        int met = spoint + SPointTickWidth;  // +2 helyett

        // Narancs és piros sávok
        while (met > (SPointSpacing - SPointTickWidth / 2) && tik < MeterBarSPointLimit) {  // 11 helyett (12-1) vagy (12-2/2)
            if (tik == 0)
                tft.fillRect(smeterX + MeterBarRedStartX, smeterY + MeterBarY, MeterBarRedWidth, MeterBarHeight, TFT_RED);
            else
                tft.fillRect(smeterX + MeterBarOrangeStartX + (tik * MeterBarOrangeSpacing), smeterY + MeterBarY, MeterBarOrangeWidth, MeterBarHeight, TFT_ORANGE);
            met -= MeterBarOrangeSpacing;
            tik++;
        }

        // Zöld sávok
        while (met > (MeterBarGreenSpacing - SPointTickWidth / 2) && tik < MeterBarTotalLimit) {  // 15 helyett (16-1)
            tft.fillRect(smeterX + MeterBarGreenStartX + ((tik - MeterBarSPointLimit) * MeterBarGreenSpacing), smeterY + MeterBarY, MeterBarGreenWidth, MeterBarHeight, TFT_GREEN);
            met -= MeterBarGreenSpacing;
            tik++;
        }

        // Utolsó sáv
        if (tik == MeterBarTotalLimit && met > (MeterBarFinalOrangeWidth / 2 + 1)) {  // 4 helyett
            tft.fillRect(smeterX + MeterBarFinalOrangeStartX, smeterY + MeterBarY, MeterBarFinalOrangeWidth, MeterBarHeight, TFT_ORANGE);
        } else {
            // Fekete kitöltés a maradék területre
            tft.fillRect(smeterX + MeterBarBlackFillOffset + spoint - met, smeterY + MeterBarY, MeterBarBlackFillBaseWidth - (SPointTickWidth + spoint) + met, MeterBarHeight,
                         TFT_BLACK);
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
        using namespace SMeterConstants;
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.fillRect(smeterX + ScaleStartXOffset, smeterY + ScaleStartYOffset, ScaleWidth, ScaleHeight, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(BC_DATUM);

        // Skála vonalak és számok
        for (int i = 0; i < SPointCount; i++) {
            tft.fillRect(smeterX + SPointStartX + (i * SPointSpacing), smeterY + SPointY, SPointTickWidth, SPointTickHeight, TFT_WHITE);
            tft.setCursor(smeterX + SPointStartX - 1 + (i * SPointSpacing), smeterY + SPointNumberY);  // -1 korrekció
            tft.print(i);
        }
        for (int i = 1; i <= PlusScaleCount; i++) {
            tft.fillRect(smeterX + PlusScaleStartX + (i * PlusScaleSpacing), smeterY + PlusScaleY, PlusScaleTickWidth, PlusScaleTickHeight, TFT_RED);
            tft.setCursor(smeterX + PlusScaleStartX - 6 + (i * PlusScaleSpacing), smeterY + PlusScaleNumberY);  // -6 korrekció
            if (i % 2 == 0) {
                tft.print("+");
                tft.print(i * 10);
            }
        }

        // Skála vízszintes sávok
        tft.fillRect(smeterX + SPointStartX, smeterY + SBarY, SBarSPointWidth, SBarHeight, TFT_WHITE);
        tft.fillRect(smeterX + SBarPlusStartX, smeterY + SBarY, SBarPlusWidth, SBarHeight, TFT_RED);
    }

    /**
     * S-Meter + RSSI/SNR kiírás (FM esetén nincs SNR/RSSI kijelzés)
     */
    void showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode) {
        smeter(rssi, isFMMode);

        if (isFMMode) {
            return;  // Exit early for FM mode
        }

        using namespace SMeterConstants;

        // --- Szöveg kirajzolása a skála ALATT (snprintf-fel és explicit törléssel) ---
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);

        // Statikus változó a maximális szélesség tárolására (törléshez), csak egyszer számoljuk ki.
        static uint16_t combinedMaxWidth = 0;
        if (combinedMaxWidth == 0) {  // Csak az első híváskor számoljuk ki
            // Helyőrző értékekkel számoljuk a maximális szélességet
            char tempBuffer[40];
            snprintf(tempBuffer, sizeof(tempBuffer), "RSSI: %3d dBuV  SNR: %3d dB", 100, 100);  // Max értékekkel
            combinedMaxWidth = tft.textWidth(tempBuffer);
            if (combinedMaxWidth == 0) combinedMaxWidth = 160;  // Biztonsági érték, ha a textWidth 0-t adna
        }

        // Formázott string létrehozása snprintf segítségével, fix szélességgel az SNR-nek
        char signalBuffer[40];  // Buffer a teljes szövegnek
        snprintf(signalBuffer, sizeof(signalBuffer), "RSSI: %d dBuV  SNR: %3d dB", rssi, snr);

        // Teljes string kirajzolása egyszerre (Bottom Left igazítás) padding NÉLKÜL, ez törli a korábbi felirat értékeket is
        tft.setTextDatum(BL_DATUM);
        tft.setTextPadding(0);  // NINCS padding
        tft.drawString(signalBuffer, smeterX + RssiLabelXOffset, smeterY + SignalLabelYOffset);
    }
};

#endif
