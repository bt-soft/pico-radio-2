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

// Az S1 (első narancs) közvetlenül az S0 (piros) után kezdődik
constexpr uint8_t MeterBarOrangeStartX = MeterBarRedStartX + MeterBarRedWidth + 2;  // 15 + 15 + 2 = 32
constexpr uint8_t MeterBarOrangeWidth = 10;
constexpr uint8_t MeterBarOrangeSpacing = 12;  // S0-S1, S1-S2, ... S8-S9 közötti távolság

// Az S9+10dB (első zöld) közvetlenül az S8 (utolsó narancs) után kezdődik
constexpr uint8_t MeterBarGreenStartX = MeterBarOrangeStartX + ((8 - 1) * MeterBarOrangeSpacing) + MeterBarOrangeWidth + 2;  // 32 + (7*12) + 10 + 2 = 32 + 84 + 10 + 2 = 128
constexpr uint8_t MeterBarGreenWidth = 14;                                                                                   // A zöld sávok szélessége
constexpr uint8_t MeterBarGreenSpacing = 16;                                                                                 // Zöld sávok közötti távolság
constexpr uint8_t MeterBarFinalOrangeStartX = 224;                                                                           // Utolsó narancs sáv (S9+60dB felett)
constexpr uint8_t MeterBarFinalOrangeWidth = 3;
constexpr uint8_t MeterBarMaxPixelValue = 208;  // A teljes mérősáv hossza pixelben (a sample.cpp alapján)
constexpr uint8_t MeterBarSPointLimit = 9;      // S9-ig (0-8 index, tehát 9 elem)
constexpr uint8_t MeterBarTotalLimit = 15;      // S9 + 6 plusz pont (9-14 index, tehát 6 elem)

// Szöveges címkék rajzolása
constexpr uint8_t RssiLabelXOffset = 10;
constexpr uint8_t SignalLabelYOffsetInFM = 60;

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
    uint8_t prev_spoint;

    /**
     * RSSI érték konvertálása S-pont értékre
     *
     * RSSI értékek 0-127 dBuV között
     * S-Pont értékek 0-MeterBarMaxPixelValue (pl. 208) között
     */
    uint8_t rssiConverter(uint8_t rssi, bool isFMMode) {
        int spoint;
        if (isFMMode) {
            // dBuV to S point konverzió
            if (rssi < 1)
                spoint = 36;
            else if ((rssi >= 1) and (rssi <= 2))
                spoint = 60;  // S6
            else if ((rssi > 2) and (rssi <= 8))
                spoint = 84 + (rssi - 2) * 2;  // S7
            else if ((rssi > 8) and (rssi <= 14))
                spoint = 96 + (rssi - 8) * 2;  // S8
            else if ((rssi > 14) and (rssi <= 24))
                spoint = 108 + (rssi - 14) * 2;  // S9
            else if ((rssi > 24) and (rssi <= 34))
                spoint = 124 + (rssi - 24) * 2;  // S9 +10
            else if ((rssi > 34) and (rssi <= 44))
                spoint = 140 + (rssi - 34) * 2;  // S9 +20
            else if ((rssi > 44) and (rssi <= 54))
                spoint = 156 + (rssi - 44) * 2;  // S9 +30
            else if ((rssi > 54) and (rssi <= 64))
                spoint = 172 + (rssi - 54) * 2;  // S9 +40
            else if ((rssi > 64) and (rssi <= 74))
                spoint = 188 + (rssi - 64) * 2;  // S9 +50
            else if (rssi > 74 && rssi <= 76)
                spoint = 204;  // S9 +60
            else if (rssi > 76)
                spoint = SMeterConstants::MeterBarMaxPixelValue;  // >S9 +60
            else
                spoint = 36;  // Default ha egyik sem illik

        } else {  // AM/SSB/CW

            // dBuV to S point konverzió
            if ((rssi >= 0) and (rssi <= 1))
                spoint = 12;  // S0
            else if ((rssi > 1) and (rssi <= 2))
                spoint = 24;  // S1
            else if ((rssi > 2) and (rssi <= 3))
                spoint = 36;  // S2
            else if ((rssi > 3) and (rssi <= 4))
                spoint = 48;  // S3
            else if ((rssi > 4) and (rssi <= 10))
                spoint = 48 + (rssi - 4) * 2;  // S4
            else if ((rssi > 10) and (rssi <= 16))
                spoint = 60 + (rssi - 10) * 2;  // S5
            else if ((rssi > 16) and (rssi <= 22))
                spoint = 72 + (rssi - 16) * 2;  // S6
            else if ((rssi > 22) and (rssi <= 28))
                spoint = 84 + (rssi - 22) * 2;  // S7
            else if ((rssi > 28) and (rssi <= 34))
                spoint = 96 + (rssi - 28) * 2;  // S8
            else if ((rssi > 34) and (rssi <= 44))
                spoint = 108 + (rssi - 34) * 2;  // S9
            else if ((rssi > 44) and (rssi <= 54))
                spoint = 124 + (rssi - 44) * 2;  // S9 +10
            else if ((rssi > 54) and (rssi <= 64))
                spoint = 140 + (rssi - 54) * 2;  // S9 +20
            else if ((rssi > 64) and (rssi <= 74))
                spoint = 156 + (rssi - 64) * 2;  // S9 +30
            else if ((rssi > 74) and (rssi <= 84))
                spoint = 172 + (rssi - 74) * 2;  // S9 +40
            else if ((rssi > 84) and (rssi <= 94))
                spoint = 188 + (rssi - 84) * 2;  // S9 +50
            else if (rssi > 94 && rssi <= 95)
                spoint = 204;  // S9 +60
            else if (rssi > 95)
                spoint = SMeterConstants::MeterBarMaxPixelValue;  // >S9 +60
            else
                spoint = 0;  // Default ha egyik sem illik
        }
        // Biztosítjuk, hogy az érték a megengedett tartományban maradjon
        if (spoint < 0) spoint = 0;
        if (spoint > SMeterConstants::MeterBarMaxPixelValue) spoint = SMeterConstants::MeterBarMaxPixelValue;
        return static_cast<uint8_t>(spoint);
    }

    /**
     * S-Meter érték kirajzolása
     */
    void smeter(uint8_t rssi, bool isFMMode) {
        using namespace SMeterConstants;
        uint8_t spoint = rssiConverter(rssi, isFMMode);

        if (spoint == prev_spoint) return;  // Ha nem változott, nem frissítünk
        prev_spoint = spoint;

        int tik = 0;
        // A 'met' itt a rendelkezésre álló "energia" a sávok kirajzolásához.
        int met = spoint;

        // Annak az X koordinátája, ahol az utolsó színes sáv véget ért (abszolút a tft-hez képest)
        // Kezdetben a piros sáv elejére állítjuk, ha spoint=0, akkor ez marad.
        int end_of_colored_x_abs = smeterX + MeterBarRedStartX;

        // Narancs és piros sávok
        // Amíg van "energia" (met > 0) ÉS még a piros/narancs szegmensen belül vagyunk (tik < MeterBarSPointLimit)
        while (met > 0 && tik < MeterBarSPointLimit) {
            if (tik == 0) {                                        // S0 - Piros
                int draw_width = min(met, (int)MeterBarRedWidth);  // Ne rajzoljunk többet, mint amennyi "energia" van, vagy a sáv szélessége
                if (draw_width > 0) {
                    tft.fillRect(smeterX + MeterBarRedStartX, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_RED);
                    end_of_colored_x_abs = smeterX + MeterBarRedStartX + draw_width;
                }
                met -= MeterBarRedWidth;  // Teljes sáv "költségét" vonjuk le
            } else {                      // S1-S8 - Narancs
                // Az X pozíció számítása: a piros sáv után kezdődik, és (tik-1) narancs sávnyi távolságra.
                int current_bar_x = smeterX + MeterBarOrangeStartX + ((tik - 1) * MeterBarOrangeSpacing);
                int draw_width = min(met, (int)MeterBarOrangeWidth);
                if (draw_width > 0) {
                    tft.fillRect(current_bar_x, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_ORANGE);
                    end_of_colored_x_abs = current_bar_x + draw_width;
                }
                met -= MeterBarOrangeWidth;  // Teljes sáv "költségét" vonjuk le
            }
            tik++;
        }

        // Zöld sávok
        // Amíg van "energia" (met > 0) ÉS még a zöld szegmensen belül vagyunk (tik < MeterBarTotalLimit)
        while (met > 0 && tik < MeterBarTotalLimit) {
            // tik itt MeterBarSPointLimit-től MeterBarTotalLimit-1 -ig megy
            // Az X pozíció: a zöld sávok kezdete + (aktuális zöld sáv indexe) * zöld sávok távolsága
            int current_bar_x = smeterX + MeterBarGreenStartX + ((tik - MeterBarSPointLimit) * MeterBarGreenSpacing);
            int draw_width = min(met, (int)MeterBarGreenWidth);
            if (draw_width > 0) {
                tft.fillRect(current_bar_x, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_GREEN);
                end_of_colored_x_abs = current_bar_x + draw_width;
            }
            met -= MeterBarGreenWidth;  // Teljes sáv "költségét" vonjuk le
            tik++;
        }

        // Utolsó sáv (S9+60dB felett)
        // Ha elértük a zöld szegmens végét ÉS még van "energia"
        if (tik == MeterBarTotalLimit && met > 0) {
            int draw_width = min(met, (int)MeterBarFinalOrangeWidth);
            if (draw_width > 0) {
                tft.fillRect(smeterX + MeterBarFinalOrangeStartX, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_ORANGE);
                end_of_colored_x_abs = smeterX + MeterBarFinalOrangeStartX + draw_width;
            }
            // met -= MeterBarFinalOrangeWidth; // Itt már nem kell csökkenteni, mert ez az utolsó
        }

        // A mérősáv teljes látható végének X koordinátája (a skála definíciója alapján)
        // A MeterBarMaxPixelValue a teljes hosszt jelenti a MeterBarRedStartX-től.
        int meter_display_area_end_x_abs = smeterX + MeterBarRedStartX + MeterBarMaxPixelValue;

        // Biztosítjuk, hogy end_of_colored_x_abs ne lépje túl a maximális értéket
        if (end_of_colored_x_abs > meter_display_area_end_x_abs) {
            end_of_colored_x_abs = meter_display_area_end_x_abs;
        }
        // És ne legyen kisebb, mint a kezdete (ha pl. spoint=0 és semmi sem rajzolódott)
        if (spoint == 0) {  // Ha spoint 0, semmi nem rajzolódik, end_of_colored_x_abs marad a kezdeti értéken.
            end_of_colored_x_abs = smeterX + MeterBarRedStartX;
        }

        // Fekete kitöltés az utolsó színes sáv végétől a skála végéig
        if (end_of_colored_x_abs < meter_display_area_end_x_abs) {
            tft.fillRect(end_of_colored_x_abs, smeterY + MeterBarY, meter_display_area_end_x_abs - end_of_colored_x_abs, MeterBarHeight, TFT_BLACK);
        }
    }

   public:
    /**
     * Konstruktor
     */
    SMeter(TFT_eSPI &tft, uint8_t smeterX, uint8_t smeterY) : tft(tft), smeterX(smeterX), smeterY(smeterY), prev_spoint(SMeterConstants::InitialPrevSpoint) {}

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
            if (i % 2 == 0) {                                                                                   // Csak minden másodiknál írjuk ki a +számot
                tft.print("+");
                tft.print(i * 10);
            }
        }

        // Skála vízszintes sávok
        tft.fillRect(smeterX + SPointStartX, smeterY + SBarY, SBarSPointWidth, SBarHeight, TFT_WHITE);
        tft.fillRect(smeterX + SBarPlusStartX, smeterY + SBarY, SBarPlusWidth, SBarHeight, TFT_RED);
    }

    /**
     * S-Meter + RSSI/SNR kiírás
     *  @param rssi the current receive signal strength (0–127 dBμV)
     *  @param snr the current SNR metric (0–127 dB)
     *  @param isFMMode true, ha FM módban vagyunk, false egyébként (AM/SSB/CW)
     */
    void showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode) {

        // A skála kirajzolása csak egyszer történik, a drawSmeterScale() hívásakor
        smeter(rssi, isFMMode);

        using namespace SMeterConstants;

        // --- Szöveg kirajzolása a skála ALATT (snprintf-fel és explicit törléssel) ---
        tft.setFreeFont();
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);  // Szín a sample.cpp alapján

        // Formázott string létrehozása snprintf segítségével, fix szélességgel az SNR-nek
        char signalBuffer[40];  // Buffer a teljes szövegnek
        snprintf(signalBuffer, sizeof(signalBuffer), "RSSI: %3d dBuV   SNR: %3d dB", rssi, snr);

        // Először töröljük a területet, ahova írni fogunk, hogy ne maradjon ott a régi érték
        // A terület szélessége kb. a ScaleWidth, magassága egy sornyi.
        uint16_t text_y_pos = smeterY + ScaleEndYOffset;
        tft.fillRect(smeterX + RssiLabelXOffset, text_y_pos - tft.fontHeight(1), ScaleWidth - RssiLabelXOffset, tft.fontHeight(1) + 2, TFT_COLOR_BACKGROUND);
        tft.setTextDatum(TL_DATUM);  // Top Left igazítás
        tft.drawString(signalBuffer, smeterX + RssiLabelXOffset, text_y_pos);
    }
};

#endif
