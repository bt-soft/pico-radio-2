#include "Rds.h"

#include "defines.h"

#define RDS_GOOD_SNR 3              // Az RDS-re 'jó' vétel SNR értéke
#define RDS_SCROLL_INTERVAL_MS 100  // Az RDS scroll lépések közötti idő (ms)

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
    : tft(tft),
      si4735(si4735),
      stationX(stationX),
      stationY(stationY),
      msgX(msgX),
      msgY(msgY),
      timeX(timeX),
      timeY(timeY),
      ptyX(ptyX),
      ptyY(ptyY),
      maxScrollWidth(maxScrollWidth),
      scrollPixelOffset(0),
      scrollSprite(&tft)  // Sprite inicializálása a TFT pointerrel
{

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

    // Sprite létrehozása és beállítása
    uint16_t scrollDisplayWidth = font2Width * maxScrollWidth;  // Kiszámoljuk itt is a sprite méretéhez
    scrollSprite.createSprite(scrollDisplayWidth, font2Height);
    scrollSprite.setFreeFont();   // Font beállítása a sprite-hoz
    scrollSprite.setTextSize(2);  // A görgetéshez használt méret
    scrollSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    scrollSprite.setTextDatum(TL_DATUM);

    // RDS info törlése
    memset(rdsInfo, 0, sizeof(rdsInfo));
}

/**
 * RDS info felirat megjelenítése/görgetése
 */
void Rds::scrollRdsText() {

    // Időzítés a görgetéshez
    static uint32_t lastScrollTime = 0;
    if (rdsInfo[0] == '\0' or millis() - lastScrollTime < RDS_SCROLL_INTERVAL_MS) {
        return;  // Vagy nincs mit görgetni, vagy még nem telt le az idő, így nem csinálunk semmit
    }
    lastScrollTime = millis();  // Időbélyeg frissítése

    // 1. A szövegjellemzők beállítása már a sprite-on be van állítva a konstruktorban

    // 2. Szélességek kiszámítása
    // A sprite már ismeri a saját méreteit, de a textWidth kell
    // Lekérdezzük a teljes rdsInfo szöveg pixelben mért szélességét
    uint16_t textWidth = tft.textWidth(rdsInfo);  // Ezt még a tft-n mérjük, mert a sprite nem biztos, hogy elég széles a teljes szöveghez

    // A maximális megjelenítési szélesség pixelben (a konstruktorban megadott karakter * font szélesség)
    uint16_t scrollDisplayWidth = font2Width * maxScrollWidth;  // Ezt a sprite is tudja: scrollSprite.width()

    // 3. Statikus változók a görgetés állapotának kezeléséhez
    const int PIXEL_STEP = 2;                            // Hány pixelt görgessen egy lépésben - sebesség
    const int SLIDE_IN_GAP_PIXELS = scrollDisplayWidth;  // Szóköz a szöveg vége és az újra megjelenés között

    // 4. Görgetési logika végrehajtása (minden híváskor)
    if (textWidth <= scrollDisplayWidth) {
        // Szöveg elfér, nincs szükség görgetésre
        if (rdsInfoChanged) {                    // Csak akkor rajzolunk, ha változott
            scrollSprite.fillScreen(TFT_BLACK);  // Sprite törlése
            // A print nem működik sprite-on, drawString-et használunk
            scrollSprite.drawString(rdsInfo, 0, 0);  // Rajzolás a sprite bal felső sarkába
            scrollPixelOffset = 0;                   // Eltolás nullázása
            rdsInfoChanged = false;                  // Flag reset
            scrollSprite.pushSprite(msgX, msgY);     // Sprite kirakása a képernyőre
        }

    } else {
        // Szöveg túl hosszú -> görgetés
        if (rdsInfoChanged) {
            scrollPixelOffset = 0;   // Eltolás nullázása, ha új szöveg jött
            rdsInfoChanged = false;  // Flag reset
        }

        // Rajzolás a sprite-ra
        scrollSprite.fillScreen(TFT_BLACK);  // Sprite törlése

        // 1. Fő szöveg rajzolása (ami balra kiúszik)
        // A '-scrollPixelOffset' miatt a szöveg balra mozog, ahogy scrollPixelOffset nő.
        // A sprite levágja a kilógó részt.
        scrollSprite.drawString(rdsInfo, -scrollPixelOffset, 0);  // Rajzolás a sprite-ra

        // 2. Az "újra beúszó" rész rajzolása
        // Kiszámoljuk a második szöveg kezdő X pozícióját a sprite-on belül.
        // Ez az első szöveg vége után van 'SLIDE_IN_GAP_PIXELS' távolságra.
        int slideInRelativeX = -scrollPixelOffset + textWidth + SLIDE_IN_GAP_PIXELS;

        // Csak akkor rajzoljuk ki a második szöveget, ha a kezdő X pozíciója
        // már a látható területen belülre (< scrollDisplayWidth) kerülne.
        if (slideInRelativeX < scrollDisplayWidth) {                // Ellenőrzés a sprite szélességéhez képest
            scrollSprite.drawString(rdsInfo, slideInRelativeX, 0);  // Rajzolás a sprite-ra
        }

        // Eltolás növelése a következő lépéshez
        scrollPixelOffset += PIXEL_STEP;

        // 3. Ciklus újraindítása
        // Amikor az első szöveg (plusz a szóköz) teljesen kiúszott balra...
        if (scrollPixelOffset >= textWidth + SLIDE_IN_GAP_PIXELS) {
            // ...akkor a második szöveg eleje pont a sprite bal szélére (X=0) ért.
            // Nullázzuk az eltolást, így az első szöveg újra a sprite elejéről indul,
            // és a ciklus kezdődik elölről.
            scrollPixelOffset = 0;  // Kezdjük elölről
        }
        scrollSprite.pushSprite(msgX, msgY);  // Sprite kirakása a képernyőre
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
            tft.setTextSize(1);
            tft.setTextDatum(BC_DATUM);
            tft.setTextColor(TFT_ORANGE, TFT_BLACK);
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
    rdsStationName = NULL;

    // clear RDS rdsInfo
    rdsInfo[0] = '\0';
    rdsInfoChanged = true;                // Jelezzük, hogy változott (üres lett)
    scrollSprite.fillScreen(TFT_BLACK);   // A sprite-ot is törölni kell
    scrollSprite.pushSprite(msgX, msgY);  // Törölt sprite kirakása

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
