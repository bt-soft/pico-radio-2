#ifndef __RDS_H
#define __RDS_H

#include <SI4735.h>
#include <TFT_eSPI.h>

#include "utils.h"

#define DEFAULT_MAX_SCROLL_WIDTH 20  // Maximális görgetési szélesség (karakterekben)

/**
 *
 */
class Rds {
   private:
    TFT_eSPI &tft;
    SI4735 &si4735;

#define MAX_STATION_NAME_LENGTH 8
    // Az SI4735 bufferét használjuk,
    // ez csak ez jelző pointer a képernyő törlésének szükségességének jelzésére
    char *rdsStationName = NULL;

#define MAX_MESSAGE_LENGTH 64
    char rdsInfo[MAX_MESSAGE_LENGTH];
    bool rdsInfoChanged = false;  // Flag a szöveg megváltozásának jelzésére

#define MAX_TIME_LENGTH 8 //hh:mm:ss

    // Program Type
    uint8_t ptyArrayMaxLength;   // A RDS_PTY_ARRAY leghosszabb stringjének hossza, a képernyő törléshez
    const char *rdsProgramType;  // RDS_PTY_ARRAY PROGMEM pointer a kiíráshoz

    // 1. font méretek
    uint8_t font1Height;
    uint8_t font1Width;

    // 2. font méretek
    uint8_t font2Height;
    uint8_t font2Width;

    // RDS adatok kiírásának X,Y TFT koordinátái
    uint16_t stationX;
    uint16_t stationY;
    uint16_t msgX;
    uint16_t msgY;
    uint16_t timeX;
    uint16_t timeY;
    uint16_t ptyX;
    uint16_t ptyY;

    // Sprite a villogásmentes görgetéshez
    TFT_eSprite scrollSprite;

    // Görgetés szélessége (karakterekben)
    uint8_t maxScrollWidth;

    /**
     * RDS adatok megszerzése és megjelenítése
     */
    void checkRds();

   public:
    /**
     * Konstruktor
     */
    Rds(TFT_eSPI &Tft, SI4735 &si4735, uint16_t stationX, uint16_t stationY, uint16_t msgX, uint16_t msgY, uint16_t timeX, uint16_t timeY, uint16_t ptyX, uint16_t ptyY,
        uint8_t maxScrollWidth = DEFAULT_MAX_SCROLL_WIDTH);

    /**
     *  RDS adatok törlése (csak FM módban)
     */
    void clearRds();

    /**
     * RDS adatok megjelenítése (csak FM módban)
     */
    void showRDS(uint8_t snr);

    /**
     * RDS adatok megjelenítése
     * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzolásakor kellhet)
     */
    void displayRds(bool force = false);

    /**
     * RDS info felirat megjelenítése/görgetése
     */
    void scrollRdsText();
};

#endif