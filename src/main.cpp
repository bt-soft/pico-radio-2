#include <Arduino.h>

#include "PicoSensorUtils.h"
#include "core1_logic.h"  // Core1 logika
#include "defines.h"
#include "pico/multicore.h"  // Core1 kezeléséhez
#include "rtVars.h"
#include "utils.h"

//------------------ TFT
#include <TFT_eSPI.h>
TFT_eSPI tft;

//------------------- Rotary Encoder
#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
// Pico Hardware timer a Rotary encoder olvasására
#include <RPi_Pico_TimerInterrupt.h>
RPI_PICO_Timer rotaryTimer(0);  // 0-ás timer használata
#endif
#include "RotaryEncoder.h"
RotaryEncoder rotaryEncoder = RotaryEncoder(PIN_ENCODER_CLK, PIN_ENCODER_DT, PIN_ENCODER_SW, ROTARY_ENCODER_STEPS_PER_NOTCH);
#define ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC 1  // 1msec

//------------------- EEPROM Config
#include "Config.h"
Config config;

//------------------- si4735
#include <SI4735.h>
SI4735 si4735;

//------------------- Band
#include "Band.h"
Band band(si4735, config);

//------------------- Memória információk megjelenítése
#ifdef SHOW_MEMORY_INFO
#include "PicoMemoryInfo.h"
#endif

//------------------- Állomás memória
#include "StationStore.h"
FmStationStore fmStationStore;
AmStationStore amStationStore;

//------------------- Képernyők
#include "AmDisplay.h"
#include "AudioAnalyzerDisplay.h"
#include "FmDisplay.h"
#include "FreqScanDisplay.h"
#include "MemoryDisplay.h"
#include "ScreenSaverDisplay.h"
#include "SetupDisplay.h"
#include "SplashScreen.h"
#include "SstvDisplay.h"
#include "core1_logic.h"  // Core1 belépési pontja
DisplayBase *pDisplay = nullptr;

//---- Dekóderek

/**
 * Globális változó az aktuális kijelző váltásának előjegyzése
 * Induláskor FM - módban indulunk
 * (Ezt a globális változót a képernyők állítgatják, ha más képernyőt választ a felhasználó)
 */
DisplayBase::DisplayType newDisplay;
DisplayBase::DisplayType currentDisplay = DisplayBase::DisplayType::none;

// A képernyővédő elindulása előtti screen pointere, majd erre állunk vissza
DisplayBase *pDisplayBeforeScreenSaver = nullptr;

//---- Power Management ----
bool isSystemShuttingDown = false;

// Forward declarations
void wakeupSystem();

/**
 * @brief GPIO interrupt callback a felébredéshez
 * Ez a függvény hívódik meg, amikor a rotary gomb megnyomását érzékeli alvás alatt
 */
void gpio_callback() {
    // Csak jelezzük, hogy ébredés történt
    // A tényleges felébredést az attachInterrupt mechanizmus kezeli
}

/**
 * @brief Rendszer kikapcsolása (dormant mode)
 * Szinte teljes kikapcsolás, csak a rotary gomb tud felébreszteni
 */
void shutdownSystem() {
    DEBUG("System shutdown initiated...\n");  // Mentés minden fontos adatnak
    config.checkSave();
    fmStationStore.checkSave();
    amStationStore.checkSave();

    // Képernyő és háttérvilágítás kikapcsolása
    tft.writecommand(0x10);                  // Sleep mode
    analogWrite(PIN_TFT_BACKGROUND_LED, 0);  // Háttérvilágítás ki

    // Audio mute
    si4735.setAudioMute(true);

    // Hangjelzés kikapcsolásról
    Utils::beepTick();
    delay(100);
    Utils::beepTick();
    delay(500);

    // Core1 leállítása
    multicore_reset_core1();

    // FIFO tisztítása a Core1 leállítása után
    while (rp2040.fifo.available() > 0) {
        uint32_t dummy;
        rp2040.fifo.pop_nb(&dummy);
    }

    DEBUG("Core1 stopped and FIFO cleared.\n");

    DEBUG("Going to sleep...\n");
    delay(100);  // Kis várakozás a debug üzenet kiiratásához
    // Setup wake-up interrupt a rotary buttonra
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
    attachInterrupt(PIN_ENCODER_SW, gpio_callback, FALLING);

    // Egyszerű deep sleep - CPU leállítása, csak interrupt ébresztheti fel
    // Megjegyzés: Ez nem igazi dormant mode, de energiatakarékos
    while (digitalRead(PIN_ENCODER_SW) == HIGH) {
        delay(10);  // Egyszerű várakozás interrupt-ra
    }

    // Ha ide eljutunk, akkor felébredtünk
    wakeupSystem();
}

/**
 * @brief Rendszer felébredése
 * Dormant mode után újrainicializálás
 */
void wakeupSystem() {
    DEBUG("System waking up...\n");  // GPIO interrupt kikapcsolása
    detachInterrupt(PIN_ENCODER_SW);

    // TFT újrainicializálása
    tft.init();
    tft.setRotation(1);
    tft.setTouch(config.data.tftCalibrateData);

    // Háttérvilágítás visszakapcsolása
    analogWrite(PIN_TFT_BACKGROUND_LED, config.data.tftBackgroundBrightness);

    // SI4735 újrainicializálása
    Wire.begin();
    si4735.reset();  // Reset a chipet
    delay(100);      // Core1 újraindítása
    multicore_launch_core1(setup1);

    // FIFO tisztítása a Core1 újraindítása után
    // Ez biztosítja, hogy a kommunikáció tiszta állapotból indul
    while (rp2040.fifo.available() > 0) {
        uint32_t dummy;
        rp2040.fifo.pop_nb(&dummy);
    }

    DEBUG("Core1 restarted and FIFO cleared.\n");

    // Audio unmute
    si4735.setAudioMute(false);

    // Képernyő újrarajzolása
    if (pDisplay != nullptr) {
        pDisplay->drawScreen();
    }

    // Hangjelzés bekapcsolásról
    Utils::beepTick();

    isSystemShuttingDown = false;
    DEBUG("System wake up complete.\n");
}

/**
 * Aktuális kijelző váltása
 * A loop()-ból hívjuk, ha van képernyő váltási igény
 */
void changeDisplay() {

    // Ha a ScreenSaver-re váltunk...
    if (::newDisplay == DisplayBase::DisplayType::screenSaver) {

        // Elmentjük az aktuális képernyő pointerét
        ::pDisplayBeforeScreenSaver = ::pDisplay;

        // Létrehozzuk a ScreenSaver képernyőt
        ::pDisplay = new ScreenSaverDisplay(tft, si4735, band);

    } else if (::currentDisplay == DisplayBase::DisplayType::screenSaver and ::newDisplay != DisplayBase::DisplayType::screenSaver) {
        // Ha ScreenSaver-ről váltunk vissza az eredeti képernyőre
        delete ::pDisplay;  // akkor töröljük a ScreenSaver-t

        // Visszaállítjuk a korábbi képernyő pointerét
        ::pDisplay = ::pDisplayBeforeScreenSaver;
        ::pDisplayBeforeScreenSaver = nullptr;

    } else {

        // Ha más képernyőről váltunk egy másik képernyőre, akkor az aktuáils képernyőt töröljük
        if (::pDisplay) {
            delete ::pDisplay;
            ::pDisplay = nullptr;  // Fontos a pointer nullázása törlés után
        }

        // Létrehozzuk az új képernyő példányát
        switch (::newDisplay) {
            case DisplayBase::DisplayType::fm:
                ::pDisplay = new FmDisplay(tft, si4735, band);
                break;

            case DisplayBase::DisplayType::am:
                ::pDisplay = new AmDisplay(tft, si4735, band);
                break;

            case DisplayBase::DisplayType::freqScan:
                ::pDisplay = new FreqScanDisplay(tft, si4735, band);
                break;

            case DisplayBase::DisplayType::setup:
                ::pDisplay = new SetupDisplay(tft, si4735, band);
                ::pDisplay->setPrevDisplayType(::currentDisplay);
                break;

            case DisplayBase::DisplayType::memory:
                ::pDisplay = new MemoryDisplay(tft, si4735, band);
                ::pDisplay->setPrevDisplayType(::currentDisplay);
                break;

            case DisplayBase::DisplayType::audioAnalyzer:
                ::pDisplay = new AudioAnalyzerDisplay(tft, si4735, band, config.data.miniAudioFftConfigAnalyzer);
                ::pDisplay->setPrevDisplayType(::currentDisplay);
                break;

            case DisplayBase::DisplayType::sstv:
                ::pDisplay = new SstvDisplay(tft, si4735, band);
                // Az SSTV-nek nincs prevDisplay-je, mert általában az AM/FM-ről jövünk ide
                break;
        }
    }

    // Megjeleníttetjük az új képernyőt
    ::pDisplay->drawScreen();

    // Ha volt aktív dialógja a képernyőnek (még mielőtt a képernyővédő aktivvá vált volna), akkor azt is kirajzoltatjuk
    if (::pDisplay->getPDialog() != nullptr) {
        ::pDisplay->getPDialog()->drawDialog();
    }

    // Elmentjük az aktuális képernyő típust
    ::currentDisplay = newDisplay;

    // Jelezzük, hogy nem akarunk képernyőváltást, megtörtént már
    ::newDisplay = DisplayBase::DisplayType::none;
}

#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
/**
 * Hardware timer interrupt service routine a rotaryhoz
 */
bool rotaryTimerHardwareInterruptHandler(struct repeating_timer *t) {
    rotaryEncoder.service();
    return true;
}
#endif

/** ----------------------------------------------------------------------------------------------------------------------------------------
 *  Arduino Setup
 */
void setup() {
#ifdef __DEBUG
    Serial.begin(115200);
#endif

    // Beeper
    pinMode(PIN_BEEPER, OUTPUT);
    digitalWrite(PIN_BEEPER, LOW);

    // TFT LED háttérvilágítás kimenet
    pinMode(PIN_TFT_BACKGROUND_LED, OUTPUT);
    analogWrite(PIN_TFT_BACKGROUND_LED, TFT_BACKGROUND_LED_MAX_BRIGHTNESS);

    // Rotary Encoder beállítása
    rotaryEncoder.setDoubleClickEnabled(true);
    rotaryEncoder.setAccelerationEnabled(true);
#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
    // Pico HW Timer1 beállítása a rotaryhoz
    rotaryTimer.attachInterruptInterval(ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC * 1000, rotaryTimerHardwareInterruptHandler);
#endif  // TFT inicializálása
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);  // Fekete háttér a splash screen-hez

// Várakozás a soros port megnyitására DEBUG módban
#ifdef DEBUG_WAIT_FOR_SERIAL
    Utils::debugWaitForSerial(tft);
#endif

    // Korai splash screen inicializálás (SI4735 még nincs kész, de a többi infót már meg tudjuk jeleníteni)
    // Egy üres SI4735 objektummal hozunk létre splash screen-t az általános infókhoz
    SplashScreen splashEarly(tft, si4735);

    // Csak az általános információkat jelenítjük meg először (SI4735 nélkül)
    tft.fillScreen(TFT_BLACK);

    // Program cím és build info megjelenítése
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(PROGRAM_NAME, tft.width() / 2, 20);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Version " + String(PROGRAM_VERSION), tft.width() / 2, 50);
    tft.drawString(PROGRAM_AUTHOR, tft.width() / 2, 70);

    // Build info
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Build: " + String(__DATE__) + " " + String(__TIME__), 10, 100);

    // Inicializálási progress
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(TC_DATUM);
    tft.drawString("Initializing...", tft.width() / 2, 140);

    // EEPROM inicializálása (A fordítónak muszáj megadni egy típust, itt most egy Config_t-t használunk, igaziból mindegy)
    tft.drawString("Loading EEPROM...", tft.width() / 2, 160);
    EepromManager<Config_t>::init();  // Meghívjuk a statikus init metódust    // Ha a bekapcsolás alatt nyomva tartjuk a rotary gombját, akkor töröljük a konfigot
    if (digitalRead(PIN_ENCODER_SW) == LOW) {
        tft.drawString("Reset detected...", tft.width() / 2, 180);
        Utils::beepTick();
        delay(1500);
        if (digitalRead(PIN_ENCODER_SW) == LOW) {  // Ha még mindig nyomják
            tft.drawString("Loading defaults...", tft.width() / 2, 200);
            config.loadDefaults();
            Utils::beepTick();
            DEBUG("Default settings resored!\n");
        }
    } else {
        // konfig betöltése
        tft.drawString("Loading config...", tft.width() / 2, 180);
        config.load();
    }
    // Kell kalibrálni a TFT Touch-t?
    if (Utils::isZeroArray(config.data.tftCalibrateData)) {
        Utils::beepError();
        Utils::tftTouchCalibrate(tft, config.data.tftCalibrateData);
    }
    // Beállítjuk a touch scren-t
    tft.setTouch(config.data.tftCalibrateData);  // Állomáslisták betöltése az EEPROM-ból (a config után!) // <-- ÚJ
    tft.drawString("Loading stations...", tft.width() / 2, 200);
    fmStationStore.load();
    amStationStore.load();

    // Most átváltunk a teljes splash screen-re az SI4735 infókkal
    delay(1000);  // Kis szünet, hogy lássa a felhasználó az eddigi progress-t

    // Az si473x (Nem a default I2C lábakon [4,5] van!!!)
    Wire.setSDA(PIN_SI4735_I2C_SDA);  // I2C for SI4735 SDA
    Wire.setSCL(PIN_SI4735_I2C_SCL);  // I2C for SI4735 SCL
    Wire.begin();

    // Splash screen megjelenítése inicializálás közben
    SplashScreen splash(tft, si4735);

    // Splash screen megjelenítése progress bar-ral
    splash.show(true, 6);

    // Lépés 1: I2C inicializálás
    splash.updateProgress(1, 6, "Initializing I2C...");
    delay(300);

    // Si4735 inicializálása
    splash.updateProgress(2, 6, "Detecting SI4735...");
    int16_t si4735Addr = si4735.getDeviceI2CAddress(PIN_SI4735_RESET);
    if (si4735Addr == 0) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("SI4735 NOT DETECTED!", tft.width() / 2, tft.height() / 2);
        DEBUG("Si4735 not detected");
        Utils::beepError();
        while (true)  // nem megyünk tovább
            ;
    }

    // Lépés 3: SI4735 konfigurálás
    splash.updateProgress(3, 6, "Configuring SI4735...");
    si4735.setDeviceI2CAddress(si4735Addr == 0x11 ? 0 : 1);  // Sets the I2C Bus Address, erre is szükség van...
    si4735.setAudioMuteMcuPin(PIN_AUDIO_MUTE);               // Audio Mute pin
    delay(300);

    DEBUG("Si473X addr: 0x%02X\n", si4735Addr);
    //--------------------------------------------------------------------

    // Lépés 4: Frekvencia beállítások
    splash.updateProgress(4, 6, "Setting up frequency...");
    rtv::freqstep = 1000;  // hz
    rtv::freqDec = config.data.currentBFO;
    delay(200);

    // Kezdő képernyőtípus beállítása
    splash.updateProgress(5, 6, "Preparing display...");
    ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;
    delay(200);

    //--------------------------------------------------------------------

    // Lépés 6: Finalizálás
    splash.updateProgress(6, 6, "Starting up...");
    delay(300);

    // Splash screen eltűntetése
    splash.hide();

    // Kezdő mód képernyőjének megjelenítése
    changeDisplay();

    // PICO AD inicializálása
    PicoSensorUtils::init();

    // Csippantunk egyet
    Utils::beepTick();
}

/** ----------------------------------------------------------------------------------------------------------------------------------------
 *  Arduino Loop
 */
void loop() {

    // Ha kell display-t váltani, akkor azt itt tesszük meg
    if (::newDisplay != DisplayBase::DisplayType::none) {
        changeDisplay();
    }

#if !defined(__USE_ROTARY_ENCODER_IN_HW_TIMER)
    //------------------- Rotary Encoder Service
    static uint32_t lastRotaryEncoderService = 0;
    if (millis() - lastRotaryEncoderService >= ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC) {
        rotaryEncoder.service();
        lastRotaryEncoderService = millis();
    }
#endif

//------------------- EEPROM mentés figyelése
#define EEPROM_SAVE_CHECK_INTERVAL 1000 * 60 * 5  // 5 perc
    static uint32_t lastEepromSaveCheck = 0;
    if (millis() - lastEepromSaveCheck >= EEPROM_SAVE_CHECK_INTERVAL) {
        config.checkSave();
        fmStationStore.checkSave();
        amStationStore.checkSave();
        lastEepromSaveCheck = millis();
    }
//------------------- Memória információk megjelenítése
#ifdef SHOW_MEMORY_INFO
    static uint32_t lasDebugMemoryInfo = 0;
    if (millis() - lasDebugMemoryInfo >= MEMORY_INFO_INTERVAL) {
        debugMemoryInfo();
        lasDebugMemoryInfo = millis();
    }
#endif  // Rotary Encoder olvasása
    RotaryEncoder::EncoderState encoderState = rotaryEncoder.read();

    // Ha folyamatosan nyomva tartják a rotary gombját akkor kikapcsolunk
    if (encoderState.buttonState == RotaryEncoder::ButtonState::Held && !isSystemShuttingDown) {
        isSystemShuttingDown = true;
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setTextSize(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("SHUTTING DOWN...", tft.width() / 2, tft.height() / 2 - 20);
        tft.setTextSize(1);
        tft.drawString("Press rotary button to wake up", tft.width() / 2, tft.height() / 2 + 20);

        delay(2000);  // 2 másodperc várakozás

        shutdownSystem();
        // Ez a pont sosem érhető el, mert a shutdownSystem() dormant(majdnem) módba teszi a processzort
    }

    // Aktuális Display loopja
    bool handleInLoop = pDisplay->loop(encoderState);

    static uint32_t lastScreenSaver = millis();
    // Ha volt touch valamelyik képernyőn, vagy volt rotary esemény...
    // Volt felhasználói interakció?
    bool userInteraction = (handleInLoop or encoderState.buttonState != RotaryEncoder::Open or encoderState.direction != RotaryEncoder::Direction::None);

    if (userInteraction) {
        // Ha volt interakció, akkor megnézzük, hogy az a képernyővédőn történt-e
        if (::currentDisplay == DisplayBase::DisplayType::screenSaver) {

            // Ha képernyővédőn volt az interakció, visszaállítjuk az előző képernyőt
            ::newDisplay = ::pDisplayBeforeScreenSaver->getDisplayType();  // Bejegyezzük visszaállításra a korábbi képernyőt
        }

        // Minden esetben frissítjük a timeoutot
        lastScreenSaver = millis();

    } else {

        // Ha nincs user interakció, megnézzük, hogy lejárt-e a timeout
        if (millis() - lastScreenSaver >= (unsigned long)config.data.screenSaverTimeoutMinutes * 60 * 1000) {

            // Ha letelt a timeout és nem a képernyővédőn vagyunk, elindítjuk a képernyővédőt
            if (::currentDisplay != DisplayBase::DisplayType::screenSaver) {
                ::newDisplay = DisplayBase::DisplayType::screenSaver;

            } else {
                // ha a screen saver már fut, akkor a timeout-ot frissítjük
                lastScreenSaver = millis();
            }
        }
    }
}

/**
 * Core1 belépési pontja
 */
void setup1() {
    // Core1 belépési pontja, itt indítjuk el a Core1 logikát
    DEBUG("Core1: Setup started.\n");

    // // Végtelen loop a Core1-en
    // FONTOS!!!: Ezt azért kell itt, mert felébredés után a Core1 nem indul újra automatikusan, csak ha itt van egy loop
    while (true) {
        loop1();
    }
}

/**
 * @brief Core1 logika futtatása a loop-ban.
 */
void loop1() {
    // Core1 logika indítása
    core1_main();
}