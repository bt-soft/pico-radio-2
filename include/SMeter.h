#ifndef __SMETER_H
#define __SMETER_H

#include <TFT_eSPI.h>

#include <algorithm>  // std::min miatt

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

constexpr uint8_t MeterBarRedStartX = 15;  // Piros S0 sáv kezdete
constexpr uint8_t MeterBarRedWidth = 15;   // Piros S0 sáv szélessége

// Az S1 (első narancs) az S0 (piros) után 2px réssel kezdődik
constexpr uint8_t MeterBarOrangeStartX = MeterBarRedStartX + MeterBarRedWidth + 2;  // 15 + 15 + 2 = 32
constexpr uint8_t MeterBarOrangeWidth = 10;                                         // Narancs S1-S8 sávok szélessége
constexpr uint8_t MeterBarOrangeSpacing = 12;                                       // Narancs sávok kezdőpontjai közötti távolság (10px sáv + 2px rés)

// Az S9+10dB (első zöld) az S8 (utolsó narancs) után 2px réssel kezdődik
// S8 vége: MeterBarOrangeStartX + (7 * MeterBarOrangeSpacing) + MeterBarOrangeWidth = 32 + 84 + 10 = 126
constexpr uint8_t MeterBarGreenStartX = MeterBarOrangeStartX + ((8 - 1) * MeterBarOrangeSpacing) + MeterBarOrangeWidth + 2;  // 126 + 2 = 128
constexpr uint8_t MeterBarGreenWidth = 14;                                                                                   // Zöld S9+dB sávok szélessége
constexpr uint8_t MeterBarGreenSpacing = 16;  // Zöld sávok kezdőpontjai közötti távolság (14px sáv + 2px rés)

constexpr uint8_t MeterBarFinalOrangeStartX = MeterBarGreenStartX + ((6 - 1) * MeterBarGreenSpacing) + MeterBarGreenWidth +
                                              2;  // Utolsó narancs sáv (S9+60dB felett)
                                                  // S9+60dB (6. zöld sáv) vége: 128 + (5*16) + 14 = 128 + 80 + 14 = 222. Utána 2px rés. -> 222 + 2 = 224
constexpr uint8_t MeterBarFinalOrangeWidth = 3;   // Ennek a sávnak a szélessége

constexpr uint8_t MeterBarMaxPixelValue = 208;                   // A teljes mérősáv hossza pixelben, az rssiConverter max kimenete
constexpr uint8_t MeterBarSPointLimit = 9;                       // S-pontok száma (S0-S8), azaz 9 sáv (1 piros, 8 narancs)
constexpr uint8_t MeterBarTotalLimit = MeterBarSPointLimit + 6;  // Összes sáv (S0-S8 és 6db S9+dB), azaz 9 + 6 = 15 sáv

// Szöveges címkék rajzolása
constexpr uint8_t RssiLabelXOffset = 10;
constexpr uint8_t SignalLabelYOffsetInFM = 60;  // FM módban a felirat Y pozíciója (ha máshova kerülne)

// Kezdeti állapot a prev_spoint-hoz
constexpr uint8_t InitialPrevSpoint = 0xFF;  // Érvénytelen érték, hogy az első frissítés biztosan megtörténjen
}  // namespace SMeterConstants

/**
 * SMeter osztály az S-Meter kezelésére
 */
class SMeter {
   private:
    TFT_eSPI &tft;
    uint32_t smeterX;     // S-Meter komponens bal felső sarkának X koordinátája
    uint32_t smeterY;     // S-Meter komponens bal felső sarkának Y koordinátája
    uint8_t prev_spoint;  // Előzőleg kirajzolt S-pont érték (pixelben), optimalizáláshoz

    /**
     * RSSI érték konvertálása S-pont értékre (pixelben).
     * @param rssi Bemenő RSSI érték (0-127 dBuV).
     * @param isFMMode Igaz, ha FM módban vagyunk, hamis AM/SSB/CW esetén.
     * @return A jelerősség pixelben (0-MeterBarMaxPixelValue).
     */
    uint8_t rssiConverter(uint8_t rssi, bool isFMMode) {
        int spoint_calc;  // Ideiglenes változó a számításhoz
        if (isFMMode) {
            // dBuV to S point konverzió FM módra
            if (rssi < 1)
                spoint_calc = 36;
            else if ((rssi >= 1) and (rssi <= 2))
                spoint_calc = 60;  // S6
            else if ((rssi > 2) and (rssi <= 8))
                spoint_calc = 84 + (rssi - 2) * 2;  // S7
            else if ((rssi > 8) and (rssi <= 14))
                spoint_calc = 96 + (rssi - 8) * 2;  // S8
            else if ((rssi > 14) and (rssi <= 24))
                spoint_calc = 108 + (rssi - 14) * 2;  // S9
            else if ((rssi > 24) and (rssi <= 34))
                spoint_calc = 124 + (rssi - 24) * 2;  // S9 +10dB
            else if ((rssi > 34) and (rssi <= 44))
                spoint_calc = 140 + (rssi - 34) * 2;  // S9 +20dB
            else if ((rssi > 44) and (rssi <= 54))
                spoint_calc = 156 + (rssi - 44) * 2;  // S9 +30dB
            else if ((rssi > 54) and (rssi <= 64))
                spoint_calc = 172 + (rssi - 54) * 2;  // S9 +40dB
            else if ((rssi > 64) and (rssi <= 74))
                spoint_calc = 188 + (rssi - 64) * 2;  // S9 +50dB
            else if (rssi > 74 && rssi <= 76)
                spoint_calc = 204;  // S9 +60dB
            else if (rssi > 76)
                spoint_calc = SMeterConstants::MeterBarMaxPixelValue;  // Max érték
            else
                spoint_calc = 36;  // Alapértelmezett minimum FM-re, ha egyik tartomány sem illik
        } else {                   // AM/SSB/CW
            // dBuV to S point konverzió AM/SSB/CW módra
            if ((rssi >= 0) and (rssi <= 1))
                spoint_calc = 12;  // S0
            else if ((rssi > 1) and (rssi <= 2))
                spoint_calc = 24;  // S1
            else if ((rssi > 2) and (rssi <= 3))
                spoint_calc = 36;  // S2
            else if ((rssi > 3) and (rssi <= 4))
                spoint_calc = 48;  // S3
            else if ((rssi > 4) and (rssi <= 10))
                spoint_calc = 48 + (rssi - 4) * 2;  // S4
            else if ((rssi > 10) and (rssi <= 16))
                spoint_calc = 60 + (rssi - 10) * 2;  // S5
            else if ((rssi > 16) and (rssi <= 22))
                spoint_calc = 72 + (rssi - 16) * 2;  // S6
            else if ((rssi > 22) and (rssi <= 28))
                spoint_calc = 84 + (rssi - 22) * 2;  // S7
            else if ((rssi > 28) and (rssi <= 34))
                spoint_calc = 96 + (rssi - 28) * 2;  // S8
            else if ((rssi > 34) and (rssi <= 44))
                spoint_calc = 108 + (rssi - 34) * 2;  // S9
            else if ((rssi > 44) and (rssi <= 54))
                spoint_calc = 124 + (rssi - 44) * 2;  // S9 +10dB
            else if ((rssi > 54) and (rssi <= 64))
                spoint_calc = 140 + (rssi - 54) * 2;  // S9 +20dB
            else if ((rssi > 64) and (rssi <= 74))
                spoint_calc = 156 + (rssi - 64) * 2;  // S9 +30dB
            else if ((rssi > 74) and (rssi <= 84))
                spoint_calc = 172 + (rssi - 74) * 2;  // S9 +40dB
            else if ((rssi > 84) and (rssi <= 94))
                spoint_calc = 188 + (rssi - 84) * 2;  // S9 +50dB
            else if (rssi > 94 && rssi <= 95)
                spoint_calc = 204;  // S9 +60dB
            else if (rssi > 95)
                spoint_calc = SMeterConstants::MeterBarMaxPixelValue;  // Max érték
            else
                spoint_calc = 0;  // Alapértelmezett minimum AM/SSB/CW-re
        }
        // Biztosítjuk, hogy az érték a megengedett tartományban maradjon
        if (spoint_calc < 0) spoint_calc = 0;
        if (spoint_calc > SMeterConstants::MeterBarMaxPixelValue) spoint_calc = SMeterConstants::MeterBarMaxPixelValue;
        return static_cast<uint8_t>(spoint_calc);
    }

    /**
     * S-Meter érték kirajzolása a mért RSSI alapján.
     * @param rssi Aktuális RSSI érték.
     * @param isFMMode Igaz, ha FM módban vagyunk.
     */
    void smeter(uint8_t rssi, bool isFMMode) {
        using namespace SMeterConstants;
        uint8_t spoint = rssiConverter(rssi, isFMMode);  // Jelerősség pixelben

        if (spoint == prev_spoint) return;  // Optimalizáció: ne rajzoljunk feleslegesen
        prev_spoint = spoint;

        int tik = 0;       // 'tik': aktuálisan rajzolt sáv indexe (S0, S1, ..., S9+10dB, ...)
        int met = spoint;  // 'met': hátralévő "jelerősség energia" pixelben, amit még ki kell rajzolni

        // Az utolsó színes sáv abszolút X koordinátája a kijelzőn.
        // Kezdetben a piros sáv (S0) elejére mutat. Ha spoint=0, ez marad.
        int end_of_colored_x_abs = smeterX + MeterBarRedStartX;

        // Piros (S0) és narancs (S1-S8) sávok rajzolása
        // Ciklus amíg van 'met' (energia) ÉS még az S-pont tartományon (S0-S8) belül vagyunk.
        while (met > 0 && tik < MeterBarSPointLimit) {
            if (tik == 0) {                                             // Első sáv: S0 (piros)
                int draw_width = std::min(met, (int)MeterBarRedWidth);  // Max. a sáv szélessége, vagy amennyi 'met' van
                if (draw_width > 0) {
                    tft.fillRect(smeterX + MeterBarRedStartX, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_RED);
                    end_of_colored_x_abs = smeterX + MeterBarRedStartX + draw_width;  // Frissítjük a színes rész végét
                }
                met -= MeterBarRedWidth;  // Teljes S0 sáv "költségét" levonjuk a 'met'-ből
            } else {                      // Következő sávok: S1-S8 (narancs)
                // X pozíció: MeterBarOrangeStartX + (aktuális narancs sáv indexe) * (narancs sáv szélessége + rés)
                int current_bar_x = smeterX + MeterBarOrangeStartX + ((tik - 1) * MeterBarOrangeSpacing);
                int draw_width = std::min(met, (int)MeterBarOrangeWidth);
                if (draw_width > 0) {
                    tft.fillRect(current_bar_x, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_ORANGE);
                    end_of_colored_x_abs = current_bar_x + draw_width;
                }
                met -= MeterBarOrangeWidth;  // Teljes narancs sáv "költségét" levonjuk
            }
            tik++;  // Lépünk a következő sávra
        }

        // Zöld (S9+10dB - S9+60dB) sávok rajzolása
        // Ciklus amíg van 'met' ÉS még az S9+dB tartományon belül vagyunk.
        while (met > 0 && tik < MeterBarTotalLimit) {
            // X pozíció: MeterBarGreenStartX + (aktuális zöld sáv indexe az S9+dB tartományon belül) * (zöld sáv szélessége + rés)
            int current_bar_x = smeterX + MeterBarGreenStartX + ((tik - MeterBarSPointLimit) * MeterBarGreenSpacing);
            int draw_width = std::min(met, (int)MeterBarGreenWidth);
            if (draw_width > 0) {
                tft.fillRect(current_bar_x, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_GREEN);
                end_of_colored_x_abs = current_bar_x + draw_width;
            }
            met -= MeterBarGreenWidth;  // Teljes zöld sáv "költségét" levonjuk
            tik++;                      // Lépünk a következő sávra
        }

        // Utolsó, S9+60dB feletti narancs sáv rajzolása
        // Ha elértük az összes S és S9+dB sáv végét (tik == MeterBarTotalLimit) ÉS még mindig van 'met' (energia).
        if (tik == MeterBarTotalLimit && met > 0) {
            int draw_width = std::min(met, (int)MeterBarFinalOrangeWidth);
            if (draw_width > 0) {
                tft.fillRect(smeterX + MeterBarFinalOrangeStartX, smeterY + MeterBarY, draw_width, MeterBarHeight, TFT_ORANGE);
                end_of_colored_x_abs = smeterX + MeterBarFinalOrangeStartX + draw_width;
            }
            // met -= MeterBarFinalOrangeWidth; // Itt már nem kell csökkenteni, mert ez az utolsó lehetséges színes sáv.
        }

        // A mérősáv teljes definiált végének X koordinátája (ahol a fekete kitöltésnek véget kell érnie).
        int meter_display_area_end_x_abs = smeterX + MeterBarRedStartX + MeterBarMaxPixelValue;

        // Biztosítjuk, hogy a kirajzolt színes rész ne lógjon túl a definiált maximális értéken.
        if (end_of_colored_x_abs > meter_display_area_end_x_abs) {
            end_of_colored_x_abs = meter_display_area_end_x_abs;
        }
        // Ha spoint=0 volt, akkor semmi sem rajzolódott, end_of_colored_x_abs a skála elején maradt.
        if (spoint == 0) {
            end_of_colored_x_abs = smeterX + MeterBarRedStartX;
        }

        // Fekete kitöltés: az utolsó színes sáv végétől a skála definiált végéig.
        // Csak akkor rajzolunk feketét, ha a színes sáv nem érte el a skála végét.
        if (end_of_colored_x_abs < meter_display_area_end_x_abs) {
            tft.fillRect(end_of_colored_x_abs, smeterY + MeterBarY, meter_display_area_end_x_abs - end_of_colored_x_abs, MeterBarHeight, TFT_BLACK);
        }
    }

   public:
    /**
     * Konstruktor.
     * @param tft Referencia a TFT kijelző objektumra.
     * @param smeterX Az S-Meter komponens bal felső sarkának X koordinátája.
     * @param smeterY Az S-Meter komponens bal felső sarkának Y koordinátája.
     */
    SMeter(TFT_eSPI &tft, uint8_t smeterX, uint8_t smeterY) : tft(tft), smeterX(smeterX), smeterY(smeterY), prev_spoint(SMeterConstants::InitialPrevSpoint) {}

    /**
     * S-Meter skála kirajzolása (a statikus részek: vonalak, számok).
     * Ezt általában egyszer kell meghívni a képernyő inicializálásakor.
     */
    void drawSmeterScale() {
        using namespace SMeterConstants;
        tft.setFreeFont();  // Alapértelmezett font használata
        tft.setTextSize(1);

        // A skála teljes területének törlése feketével
        tft.fillRect(smeterX + ScaleStartXOffset, smeterY + ScaleStartYOffset, ScaleWidth, ScaleHeight, TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Szövegszín: fehér, Háttér: fekete
        tft.setTextDatum(BC_DATUM);              // Igazítás: Bottom-Center (számokhoz)

        // S-pont skála vonalak és számok (0-9)
        for (int i = 0; i < SPointCount; i++) {
            tft.fillRect(smeterX + SPointStartX + (i * SPointSpacing), smeterY + SPointY, SPointTickWidth, SPointTickHeight, TFT_WHITE);
            tft.setCursor(smeterX + SPointStartX - 1 + (i * SPointSpacing), smeterY + SPointNumberY);  // Pozicionálás a számhoz (-1 korrekció)
            tft.print(i);
        }
        // S9+dB skála vonalak és számok (+10, +20, ..., +60)
        for (int i = 1; i <= PlusScaleCount; i++) {
            tft.fillRect(smeterX + PlusScaleStartX + (i * PlusScaleSpacing), smeterY + PlusScaleY, PlusScaleTickWidth, PlusScaleTickHeight, TFT_RED);
            tft.setCursor(smeterX + PlusScaleStartX - 6 + (i * PlusScaleSpacing), smeterY + PlusScaleNumberY);  // Pozicionálás (-6 korrekció)
            if (i % 2 == 0) {                                                                                   // Csak minden másodiknál írjuk ki a "+számot" (pl. +20, +40, +60)
                tft.print("+");
                tft.print(i * 10);
            }
        }

        // Skála alatti vízszintes sávok
        tft.fillRect(smeterX + SPointStartX, smeterY + SBarY, SBarSPointWidth, SBarHeight, TFT_WHITE);  // S0-S9 sáv
        tft.fillRect(smeterX + SBarPlusStartX, smeterY + SBarY, SBarPlusWidth, SBarHeight, TFT_RED);    // S9+dB sáv
    }

    /**
     * S-Meter érték és RSSI/SNR szöveg megjelenítése.
     * @param rssi Aktuális RSSI érték (0–127 dBμV).
     * @param snr Aktuális SNR érték (0–127 dB).
     * @param isFMMode Igaz, ha FM módban vagyunk, hamis egyébként (AM/SSB/CW).
     */
    void showRSSI(uint8_t rssi, uint8_t snr, bool isFMMode) {
        // 1. Dinamikus S-Meter sávok kirajzolása az aktuális RSSI alapján
        smeter(rssi, isFMMode);

        using namespace SMeterConstants;

        // 2. RSSI és SNR értékek szöveges kiírása a skála ALÁ
        tft.setFreeFont();  // Alapértelmezett font
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);

        // Formázott string létrehozása (pl. "RSSI:  32 dBuV   SNR:  15 dB")
        char signalBuffer[40];
        snprintf(signalBuffer, sizeof(signalBuffer), "RSSI: %3d dBuV   SNR: %3d dB", rssi, snr);

        // Szöveg pozíciójának meghatározása
        uint16_t text_y_pos = smeterY + ScaleEndYOffset + 2;  // Pár pixel margó a skála alatt
        uint16_t text_x_pos = smeterX + RssiLabelXOffset;

        // Először töröljük a területet, ahova írni fogunk, hogy ne maradjon ott a régi érték.
        // A törlési szélesség lefedi a maximálisan várható szöveghosszt.
        tft.fillRect(text_x_pos, text_y_pos, ScaleWidth - RssiLabelXOffset, tft.fontHeight(1) + 2, TFT_COLOR_BACKGROUND);

        tft.setTextDatum(TL_DATUM);  // Igazítás: Top-Left
        tft.drawString(signalBuffer, text_x_pos, text_y_pos);
    }
};

#endif
