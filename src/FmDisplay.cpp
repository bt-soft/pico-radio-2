#include "FmDisplay.h"

#include <Arduino.h>

// --- Külső Globális Változók Deklarálása ---
#include "RotaryEncoder.h"           // Szükséges a RotaryEncoder típushoz
extern RotaryEncoder rotaryEncoder;  // Jelezzük, hogy ez máshol van definiálva

// --- Seek Callback függvények (statikusak, mert a C library ezt várja) ---
// Hozzáférés a globális/statikus tft és rotaryEncoder objektumokhoz szükséges lehet.
// Alternatív megoldás lehetne lambda függvények használata, ha a library támogatja a kontextus átadását.

static TFT_eSPI *pSeekTft = nullptr;  // Pointer a TFT objektumra a callbackekhez
// static RotaryEncoder* pSeekRotary = nullptr; // Ezt nem használjuk közvetlenül
static SevenSegmentFreq *pSeekFreqDisplay = nullptr;  // Pointer a frekvenciakijelzőre
static volatile bool seekStoppedByUser = false;       // Flag a keresés megszakításához

/**
 * Konstruktor
 */
FmDisplay::FmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band), pSMeter(nullptr), pRds(nullptr), pSevenSegmentFreq(nullptr) {

    DEBUG("FmDisplay::FmDisplay\n");

    // Statikus pointerek beállítása a callbackekhez
    pSeekTft = &tft;
    // pSeekRotary = &rotaryEncoder; // Nem tároljuk el a pointert

    // SMeter példányosítása
    pSMeter = new SMeter(tft, 0, 110);

    // RDS példányosítása
    pRds = new Rds(tft, si4735, 80, 62,  // Station x,y
                   0, 80,                // Message x,y
                   2, 42,                // Time x,y
                   0, 105                // program type x,y
    );

    // Frekvencia kijelzés pédányosítása
    pSevenSegmentFreq = new SevenSegmentFreq(tft, rtv::freqDispX, rtv::freqDispY, band);
    pSeekFreqDisplay = pSevenSegmentFreq;  // Pointer beállítása a callbackhez

    // Függőleges gombok legyártása, nincs saját függőleges gombsor
    DisplayBase::buildVerticalScreenButtons(nullptr, 0);

    // Horizontális Képernyőgombok definiálása
    // Csak az FM-specifikus gombokat adjuk hozzá.
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"RDS", TftButton::ButtonType::Toggleable, TFT_TOGGLE_BUTTON_STATE(config.data.rdsEnabled)},  //
        {"SeekD", TftButton::ButtonType::Pushable},                                                   // Seek Down
        {"SeekU", TftButton::ButtonType::Pushable},                                                   // Seek Up
    };

    // Horizontális képernyőgombok legyártása:
    // Összefűzzük a kötelező gombokat (amiből kivettük a AFWdt, BFO-t) az FM-specifikus gombokkal.
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), true);  // isMandatoryNeed = true
}

/**
 *
 */
FmDisplay::~FmDisplay() {

    // SMeter trölése
    if (pSMeter) {
        delete pSMeter;
    }

    // RDS trölése
    if (pRds) {
        delete pRds;
    }

    // Frekvencia kijelző törlése
    if (pSevenSegmentFreq) {
        delete pSevenSegmentFreq;
    }
}

// --- Seek Callback Implementációk ---

/**
 * @brief Callback a seek folyamat közbeni frekvencia kijelzéshez.
 * @param freq Az aktuálisan talált frekvencia (a rádió formátumában, pl. 10kHz FM esetén).
 */
static void seekFreqCallback(uint16_t freq) {
    if (pSeekFreqDisplay) {
        // Itt csak a frekvenciát frissítjük, a teljes kijelzőt nem rajzoljuk újra.
        pSeekFreqDisplay->freqDispl(freq);
    }
    // Opcionális: Vizuális visszajelzés a keresésről (pl. "SEEK..." szöveg)
    // Ezt a gombnyomáskor is meg lehet jeleníteni és a keresés végén levenni.
}

/**
 * @brief Callback a keresés felhasználói megszakításának ellenőrzéséhez.
 * @return true, ha a felhasználó megszakította a keresést, egyébként false.
 */
static bool checkStopSeekingCallback() {
    // Közvetlenül a globális rotaryEncoder objektumot használjuk itt,
    // mivel a seekStationProgress valószínűleg blokkol, és a handleRotary nem fut.
    if (!pSeekTft) return false;  // TFT pointer ellenőrzése

    uint16_t tx, ty;
    bool touched = pSeekTft->getTouch(&tx, &ty);                     // Érintés ellenőrzése
    RotaryEncoder::EncoderState rotaryState = rotaryEncoder.read();  // Közvetlen olvasás a globális objektumból

    seekStoppedByUser = touched or (rotaryState.direction != RotaryEncoder::Direction::None) or (rotaryState.buttonState != RotaryEncoder::ButtonState::Open);

    return seekStoppedByUser;
}

/**
 * Képernyő kirajzolása
 * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
 */
void FmDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    DisplayBase::dawStatusLine();

    // RSSI skála kirajzoltatása
    pSMeter->drawSmeterScale();

    BandTable &currentBand = band.getCurrentBand();

    // RSSI aktuális érték
    si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = si4735.getCurrentRSSI();
    uint8_t snr = si4735.getCurrentSNR();
    pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

    // RDS (erőből a 'valamilyen' adatok megjelenítése)
    if (config.data.rdsEnabled) {
        pRds->displayRds(true);
    }

    // Mono/Stereo aktuális érték
    this->showMonoStereo(si4735.getCurrentPilot());

    // Frekvencia
    float currFreq = currentBand.varData.currFreq;  // A Rotary változtatásakor már eltettük a Band táblába
    pSevenSegmentFreq->freqDispl(currFreq);

    // Gombok kirajzolása
    DisplayBase::drawScreenButtons();
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void FmDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    if (STREQ("RDS", event.label)) {
        // Radio Data System
        config.data.rdsEnabled = event.state == TftButton::ButtonState::On;
        if (config.data.rdsEnabled) {
            pRds->displayRds(true);
        } else {
            pRds->clearRds();
        }

    } else if (STREQ("SeekD", event.label) or STREQ("SeekU", event.label)) {

        // RDS adatok törlése
        pRds->clearRds();

        // Állomáskeresés irányának meghatározása
        bool seekUp = STREQ("SeekU", event.label);

        // Vizuális visszajelzés indítása
        tft.setFreeFont();
        tft.setTextSize(2);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setTextDatum(TL_DATUM);  // Top-Left datum (szöveg bal felső sarka)

        constexpr uint16_t seekTextX = 20;
        constexpr uint16_t seekTextY = 65;
        const char *seekText = seekUp ? "Seek Up..." : "Seek Down...";
        uint16_t textWidth = tft.textWidth(seekText);    // Szöveg szélességének lekérése
        uint16_t textHeight = tft.fontHeight();          // Szöveg magasságának lekérése
        tft.drawString(seekText, seekTextX, seekTextY);  // Szöveg kirajzolása

        // Callback flag reset
        seekStoppedByUser = false;

        // Keresés indítása a megfelelő callbackekkel
        // A setSeekFmSpacing és setSeekFmLimits beállítása a Band.bandInit() során megtörtént.
        si4735.seekStationProgress(seekFreqCallback, checkStopSeekingCallback, seekUp ? SEEK_UP : SEEK_DOWN);

        // Pontosan a szöveg helyét töröljük a kiszámított koordinátákkal
        tft.fillRect(seekTextX, seekTextY, textWidth, textHeight, TFT_COLOR_BACKGROUND);

        // Ha nem a felhasználó állította le
        if (!seekStoppedByUser) {

            // Új frekvencia lekérdezése és beállítása
            uint16_t newFreq = si4735.getFrequency();
            band.getCurrentBand().varData.currFreq = newFreq;

            // Kijelző frissítésének jelzése
            DisplayBase::frequencyChanged = true;

            // Mono/Stereo frissítése az új frekvencián
            si4735.getCurrentReceivedSignalQuality();  // Szükséges lehet a pilot lekérdezése előtt
            this->showMonoStereo(si4735.getCurrentPilot());

        } else {
            // Ha a felhasználó állította le, a frekvencia már visszaállt az eredetire a seekStationProgress által (feltételezve)
            // Csak a kijelzőt kell frissíteni az eredeti frekvenciával
            DisplayBase::frequencyChanged = true;
        }
    }
}

/**
 * Touch (nem képrnyő button) esemény lekezelése
 * A további gui elemek vezérléséhez
 */
bool FmDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; }

/**
 * Mono/Stereo vétel megjelenítése
 */
void FmDisplay::showMonoStereo(bool stereo) {

    // STEREO/MONO háttér
    uint32_t backGroundColor = stereo ? TFT_RED : TFT_BLUE;
    tft.fillRect(rtv::freqDispX + 191, rtv::freqDispY + 60, 38, 12, backGroundColor);

    // Felirat
    tft.setFreeFont();
    tft.setTextColor(TFT_WHITE, backGroundColor);
    tft.setTextSize(1);
    tft.setTextDatum(BC_DATUM);
    tft.setTextPadding(0);
    tft.drawString(stereo ? "STEREO" : "MONO", rtv::freqDispX + 210, rtv::freqDispY + 71);
}

/**
 * Rotary encoder esemény lekezelése
 */
bool FmDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {

    // Ha éppen keresünk, ne reagáljunk a forgatásra (a megszakítást a callback kezeli)
    // Bár a seekStationProgress valószínűleg blokkol, ez biztonsági ellenőrzés.
    // A seekStoppedByUser flaget a callback állítja be. Ha a keresés fut, ez false.
    // Ha a felhasználó forgat, a callback true-ra állítja és a seekStationProgress leáll.
    // Így ide már csak akkor jutunk el, ha a keresés nem aktív.

    BandTable &currentBand = band.getCurrentBand();

    // Kiszámítjuk a frekvencia lépés nagyságát
    uint16_t step = encoderState.value * currentBand.varData.currStep;  // A lépés nagysága

    // Beállítjuk a frekvenciát
    si4735.setFrequency(si4735.getFrequency() + step);

    // Elmentjük a band táblába az aktuális frekvencia értékét
    currentBand.varData.currFreq = si4735.getFrequency();

    // RDS törlés
    pRds->clearRds();

    // Beállítjuk, hogy kell majd új frekvenciakijelzés
    DisplayBase::frequencyChanged = true;

    return true;
}

/**
 * Esemény nélküli display loop -> Adatok periódikus megjelenítése
 */
void FmDisplay::displayLoop() {

    // Ha éppen keresünk (a seekStoppedByUser false, de a keresés elindult),
    // akkor ne frissítsük a kijelzőt itt, mert a callback frissíti a frekvenciát.
    // A seekStationProgress valószínűleg blokkoló jellegű lehet a callbackekkel,
    // így ez a loop ritkábban fut le keresés alatt.
    // A biztonság kedvéért ellenőrizhetnénk egy 'seeking' flag-et, amit a gombnyomáskor állítunk.

    // Ha van dialóg, akkor nem frissítjük a komponenseket
    if (DisplayBase::pDialog != nullptr) {
        return;
    }

    BandTable &currentBand = band.getCurrentBand();

    // Néhány adatot csak ritkábban frissítünk
    static uint32_t elapsedTimedValues = 0;  // Kezdőérték nulla
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {

        // RSSI
        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();
        pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

        // RDS adatok megszerzése és megjelenítése
        if (config.data.rdsEnabled) {
            pRds->showRDS(snr);
        }

        // Mono/Stereo
        static bool prevStereo = false;
        bool stereo = si4735.getCurrentPilot();
        // Ha változott, akkor frissítünk
        if (stereo != prevStereo) {
            this->showMonoStereo(stereo);
            prevStereo = stereo;  // Frissítsük az előző értéket
        }

        // Frissítjük az időbélyeget
        elapsedTimedValues = millis();
    }

    // Az RDS szöveg görgetése nagyobb sebességgel történik
    static uint32_t rdsScrollTime = 0;  // Kezdőérték nulla
    if (config.data.rdsEnabled) {
        if ((millis() - rdsScrollTime) >= 100) {  // 200ms
            pRds->scrollRdsText();
            rdsScrollTime = millis();  // Frissítjük az időbélyeget
        }
    }

    // A Frekvenciát azonnal frissítjuk, de csak ha változott
    if (DisplayBase::frequencyChanged) {
        pSevenSegmentFreq->freqDispl(currentBand.varData.currFreq);
        DisplayBase::frequencyChanged = false;  // Reset
    }
}
