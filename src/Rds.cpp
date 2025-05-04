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
Rds::Rds(TFT_eSPI &tft, SI4735 &si4735, uint16_t stationX, uint16_t stationY, uint16_t msgX, uint16_t msgY, uint16_t timeX, uint16_t timeY, uint16_t ptyX, uint16_t ptyY,
         uint8_t maxScrollWidth)
    : tft(tft), si4735(si4735), stationX(stationX), stationY(stationY), msgX(msgX), msgY(msgY), timeX(timeX), timeY(timeY), ptyX(ptyX), ptyY(ptyY), maxScrollWidth(maxScrollWidth) {

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
 * RDS info felirat megjelenítése/görgetése
 */
void Rds::scrollRdsText() {

    // Ha nincs mit scrollozni akkor kilépünk
    if (rdsInfo[0] == '\0') {
        return;
    }

    // 1. Szövegjellemzők beállítása
    tft.setTextSize(1);                      // Betűméret a görgetett szöveghez
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Fehér szöveg fekete háttéren
    tft.setTextDatum(TL_DATUM);              // Top-Left datum a drawString-hoz

    // 2. Szélességek kiszámítása
    // Lekérdezzük a teljes rdsInfo szöveg pixelben mért szélességét
    uint16_t textWidth = tft.textWidth(rdsInfo);
    // A maximális megjelenítési szélesség pixelben (a konstruktorban megadott karakter * font szélesség)
    uint16_t displayWidth = font1Width * maxScrollWidth;

    // 3. Statikus változók a görgetés állapotának kezeléséhez
    static int currentPixelOffset = 0;          // Aktuális pixel eltolás balra
    const int PIXEL_STEP = 3;                   // Hány pixelt görgessen egy lépésben - sebesség
    const int SLIDE_IN_GAP_PIXELS = textWidth;  // Szóköz a szöveg vége és az újra megjelenés között

    // 4. Görgetési logika végrehajtása (minden híváskor)
    if (textWidth <= displayWidth) {
        // Szöveg elfér, nincs szükség görgetésre ->  Nincs szükség viewportra
        if (rdsInfoChanged) {
            tft.fillRect(msgX, msgY, displayWidth, font1Height, TFT_BLACK);  // Terület törlése
            tft.setCursor(msgX, msgY);
            tft.setTextDatum(TL_DATUM);  // Visszaállítás a printhez
            tft.print(rdsInfo);
            currentPixelOffset = 0;  // Eltolás nullázása

            rdsInfoChanged = false;  // Flag reset
        }

    } else {
        // Szöveg túl hosszú -> görgetés
        // Viewport beállítása és rajzolás relatív koordinátákkal
        tft.fillRect(msgX, msgY, displayWidth, font1Height, TFT_BLACK);  // Terület törlése
        tft.setViewport(msgX, msgY, displayWidth, font1Height);          // Viewport beállítása

        // 1. Fő szöveg rajzolása (ami balra kiúszik)
        // A '-currentPixelOffset' miatt a szöveg balra mozog, ahogy currentPixelOffset nő.
        // A viewport levágja a bal oldalon kilógó részt.
        tft.drawString(rdsInfo, -currentPixelOffset, 0);  // Relatív X, Y=0

        // 2. Az "újra beúszó" rész rajzolása
        // Kiszámoljuk a második szöveg kezdő X pozícióját a viewporton belül.
        // Ez az első szöveg vége után van 'SLIDE_IN_GAP_PIXELS' távolságra.
        int slideInRelativeX = -currentPixelOffset + textWidth + SLIDE_IN_GAP_PIXELS;

        // Csak akkor rajzoljuk ki a második szöveget, ha a kezdő X pozíciója
        // már a látható területen belülre (< displayWidth) kerülne.
        if (slideInRelativeX < displayWidth) {             // Ellenőrzés a viewport szélességéhez képest
            tft.drawString(rdsInfo, slideInRelativeX, 0);  // Relatív X, Y=0
        }

        // Eltolás növelése a következő lépéshez
        currentPixelOffset += PIXEL_STEP;

        // 3. Ciklus újraindítása
        // Amikor az első szöveg (plusz a szóköz) teljesen kiúszott balra...
        if (currentPixelOffset >= textWidth + SLIDE_IN_GAP_PIXELS) {
            // ...akkor a második szöveg eleje pont a viewport bal szélére (X=0) ért.
            // Nullázzuk az eltolást, így az első szöveg újra a viewport elejéről indul,
            // és a ciklus kezdődik elölről.
            currentPixelOffset = 0;  // Kezdjük elölről
        }
        tft.resetViewport();  // Viewport eltávolítása
    }
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
    char *rdsMsg = si4735.getRdsText2A();
    // Van RDS üzenet?
    if (rdsMsg != nullptr and strlen(rdsMsg) > 0) {

        // Csak ha eltérő a tartalma, akkor másolunk (az rdsMsg végén a space-eket figyelmen kívül hagyva)
        if (Utils::strncmpIgnoringTrailingSpaces(rdsInfo, rdsMsg, sizeof(rdsInfo)) != 0) {
            memset(rdsInfo, 0, sizeof(rdsInfo));            // Töröljük a régi szöveget
            strncpy(rdsInfo, rdsMsg, sizeof(rdsInfo) - 1);  // Átmásoljuk az új szöveget
            Utils::trimTrailingSpaces(rdsInfo);             // Trailing szóközök eltávolítása

            rdsInfoChanged = true;  // Flag a szöveg megváltozásának jelzésére
            DEBUG("RDS info: '%s'\n", rdsInfo);
        }
    }

    // Idő
    char dateTime[20];
    uint16_t year, month, day, hour, minute;

    bool rdsDateTimeSuccess = si4735.getRdsDateTime(&year, &month, &day, &hour, &minute);
    if (forceDisplay or rdsDateTimeSuccess) {
        tft.setTextSize(1);
        tft.setTextDatum(BC_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setCursor(timeX, timeY);
        sprintf(dateTime, "%02d:%02d", hour, minute);
        tft.print(dateTime);
    }

    // RDS program type (PTY)
    uint8_t rdsPty = si4735.getRdsProgramType();
    if (rdsPty < RDS_PTY_COUNT) {
        const char *p = getPtyStrPointer(rdsPty);

        if (forceDisplay or rdsProgramType != p) {
            tft.fillRect(ptyX, ptyY, font2Width * ptyArrayMaxLength, font2Height, TFT_BLACK);
            rdsProgramType = p;
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

    DEBUG("Rds::clearRds()\n");

    // clear RDS rdsStationName
    tft.fillRect(stationX, stationY, font2Width * MAX_STATION_NAME_LENGTH, font2Height, TFT_BLACK);
    rdsStationName = NULL;

    // clear RDS rdsMsg
    tft.fillRect(msgX, msgY, font1Width * MAX_MESSAGE_LENGTH, font1Height, TFT_BLACK);
    rdsInfo[0] = '\0';

    // clear RDS rdsTime
    tft.fillRect(timeX, timeY, font1Width * MAX_TIME_LENGTH, font1Height, TFT_BLACK);

    // clear RDS programType
    tft.fillRect(ptyX, ptyY, font2Width * ptyArrayMaxLength, font2Height, TFT_BLACK);
    rdsProgramType = NULL;
}

/**
 * RDS adatok megjelenítése (csak FM módban hívható...nyílván....)
 */
void Rds::showRDS(uint8_t snr) {

    // Ha 'jó' a vétel akkor rámozdulunk az RDS-re
    if (snr >= RDS_GOOD_SNR) {
        checkRds();
    } else if (rdsStationName != NULL or rdsInfo[0] != '\0') {
        clearRds();  // töröljük az esetleges korábbi RDS adatokat
    }
}
