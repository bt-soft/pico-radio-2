#include "Rds.h"

#include "defines.h"

#define RDS_GOOD_SNR 3  // Az RDS-re 'jó' vétel SNR értéke

//-----------------------------------------------------------------------------------------------------------------
/**
 * PTY típusok a PROGMEM-be töltve
 */
const char *RDS_PTY_ARRAY[] = {"No defined",
                               "News",
                               "Current affairs",
                               "Information",
                               "Sport",
                               "Education",
                               "Drama",
                               "Culture",
                               "Science",
                               "Varied",
                               "Pop Nusic",
                               "Rock Music",
                               "Easy Listening",
                               "Light Classical",
                               "Serious Classical",
                               "Other Music",
                               "Weather",
                               "Finance",
                               "Children's Programmes",
                               "Social Affairs",
                               "Religion",
                               "Phone-in",
                               "Travel",
                               "Leisure",
                               "Jazz Music",
                               "Country Music",
                               "National Music",
                               "Oldies Music",
                               "Folk Music",
                               "Documentary",
                               "Alarm Test",
                               "Alarm"};
#define RDS_PTY_COUNT ARRAY_ITEM_COUNT(RDS_PTY_ARRAY)

//-----------------------------------------------------------------------------------------------------------------
/**
 * A PTY PROGMEM String pointerének megszerzése a PTY érték alapján
 */
const char *getPtyStrPointer(uint8_t ptyIndex) {
    if (ptyIndex < RDS_PTY_COUNT) {
        return RDS_PTY_ARRAY[ptyIndex];
    }
    return PSTR("Unknown PTY");
}

/**
 * A PTY PROGMEM Stringjei közül a leghoszabb méretének a kikeresése
 */
int getLongestPtyStrLength() {
    uint8_t maxLength = 0;

    for (uint8_t i = 0; i < RDS_PTY_COUNT; i++) {
        const char *ptr = RDS_PTY_ARRAY[i];
        uint8_t length = 0;

        // Karakterenként olvassuk, amíg nullát nem találunk
        while (*(ptr + length) != '\0') {
            length++;
        }

        if (length > maxLength) {
            maxLength = length;
        }
    }
    return maxLength;
}

//-----------------------------------------------------------------------------------------------------------------

/**
 * Konstruktor
 */
Rds::Rds(TFT_eSPI &tft, SI4735 &si4735, uint16_t stationX, uint16_t stationY, uint16_t msgX, uint16_t msgY, uint16_t timeX, uint16_t timeY, uint16_t ptyX, uint16_t ptyY)
    : tft(tft), si4735(si4735), stationX(stationX), stationY(stationY), msgX(msgX), msgY(msgY), timeX(timeX), timeY(timeY), ptyX(ptyX), ptyY(ptyY) {

    // Lekérjük a fontok méreteit (Fontos előtte beállítani a fontot!!)
    tft.setFreeFont();

    // TextSize1
    tft.setTextSize(1);
    font1Height = tft.fontHeight();
    font1Width = tft.textWidth(F("W"));

    // TextSize2
    tft.setTextSize(2);
    font2Height = tft.fontHeight();
    font2Width = tft.textWidth(F("W"));

    // Megállapítjuk a leghosszabb karakterlánc hosszát a PTY PROGMEM tömbben
    ptyArrayMaxLength = getLongestPtyStrLength();
}

/**
 * RDS adatok megjelenítése
 * (Az esetleges dialóg eltünése után a teljes képernyőt újra rajzolásakor kellhet -> forceDisplay = true)
 * @param forceDisplay erőből, ne csak a változáskor jelenítsen meg adatokat
 */
void Rds::displayRds(bool forceDisplay) {

    tft.setFreeFont();
    tft.setTextDatum(BC_DATUM);

    // Állomásnév
    rdsStationName = si4735.getRdsText0A();
    if (rdsStationName != NULL) {
        tft.setTextSize(2);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(stationX, stationY);
        tft.print(rdsStationName);
    }

    // Info
    rdsMsg = si4735.getRdsText2A();
    if (rdsMsg != NULL) {
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(msgX, msgY);
        tft.print(rdsMsg);
    }

    // Idő
    char dateTime[20];
    uint16_t year, month, day, hour, minute;

    bool rdsDateTimeSuccess = si4735.getRdsDateTime(&year, &month, &day, &hour, &minute);
    // Ha izomból kell megjeleníteni vagy érvényes idő jött
    if (forceDisplay or rdsDateTimeSuccess) {
        // sprintf(dateTime, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
        //  DEBUG("RDS full datetime : %s  ", dateTime);

        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(timeX, timeY);
        sprintf(dateTime, "%02d:%02d", hour, minute);
        // DEBUG("RDS time : %s\n", dateTime);
        tft.print(dateTime);
    }

    // RDS program type (PTY)
    uint8_t rdsPty = si4735.getRdsProgramType();
    if (rdsPty < RDS_PTY_COUNT) {
        const char *p = getPtyStrPointer(rdsPty);  

        // Ha izomból kell megjeleníteni vagy változás van
        if (forceDisplay or rdsProgramType != p) {
            // clear RDS programType
            tft.fillRect(ptyX, ptyY, font2Width * ptyArrayMaxLength, font2Height, TFT_BLACK);

            // Elmentjük az új pointert
            rdsProgramType = p;

            // Kiírjuk a String-et a PROGMEM-ből
            tft.setTextSize(2);
            tft.setTextDatum(BC_DATUM);
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.setCursor(ptyX, ptyY);
            tft.print((const __FlashStringHelper *)rdsProgramType);
        }
    }
}

/**
 * RDS adatok megszerzése és megjelenítése
 */
void Rds::checkRds() {

    // Ha nincs RDS akkor nem megyünk tovább
    si4735.getRdsStatus();
    if (!si4735.getRdsReceived() or !si4735.getRdsSync() or !si4735.getRdsSyncFound()) {
        return;
    }

    displayRds();
}

/**
 *  RDS adatok törlése (csak FM módban hívható...nyílván....)
 */
void Rds::clearRds() {

    // clear RDS rdsStationName
    tft.fillRect(stationX, stationY, font2Width * MAX_STATION_NAME_LENGTH, font2Height, TFT_BLACK);
    // tft.drawRect(stationX, stationY, font2Width * MAX_STATION_NAME_LENGTH, font2Height, TFT_YELLOW);
    rdsStationName = NULL;

    // clear RDS rdsMsg
    tft.fillRect(msgX, msgY, font1Width * MAX_MESSAGE_LENGTH, font1Height, TFT_BLACK);
    // tft.drawRect(msgX, msgY, font1Width * MAX_MESSAGE_LENGTH, font1Height, TFT_YELLOW);
    rdsMsg = NULL;

    // clear RDS rdsTime
    tft.fillRect(timeX, timeY, font1Width * MAX_TIME_LENGTH, font1Height, TFT_BLACK);
    // tft.drawRect(timeX, timeY, font1Width * MAX_TIME_LENGTH, font1Height, TFT_YELLOW);
    // this->rdsTime = NULL;

    // clear RDS programType
    tft.fillRect(ptyX, ptyY, font2Width * ptyArrayMaxLength, font2Height, TFT_BLACK);
    // tft.drawRect(ptyX, ptyY, font2Width * ptyArrayMaxLength, font2Height, TFT_YELLOW);
    rdsProgramType = NULL;
}

/**
 * RDS adatok megjelenítése (csak FM módban hívható...nyílván....)
 */
void Rds::showRDS(uint8_t snr) {

    // Ha 'jó' a vétel akkor rámozdulunk az RDS-re
    if (snr >= RDS_GOOD_SNR) {
        checkRds();
    } else if (rdsStationName != NULL or rdsMsg != NULL) {
        clearRds();  // töröljük az esetleges korábbi RDS adatokat
    }
}
