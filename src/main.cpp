#include <Arduino.h>

#include "PicoSensorUtils.h"
#include "defines.h"
#include "rtVars.h"
#include "utils.h"

//------------------ TFT
#define SCREEN_SAVER_TIME 1000 * 60 * 1  // 10 perc a képernyővédő időzítése
#include <TFT_eSPI.h>                     // TFT_eSPI könyvtár
TFT_eSPI tft;                             // TFT objektum

//------------------- Rotary Encoder
#define __USE_ROTARY_ENCODER_IN_HW_TIMER
#ifdef __USE_ROTARY_ENCODER_IN_HW_TIMER
// Pico Hardware timer a Rotary encoder olvasására
#include <RPi_Pico_TimerInterrupt.h>
RPI_PICO_Timer ITimer1(1);
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
Band band(si4735);

//------------------- Memória információk megjelenítése
#ifdef SHOW_MEMORY_INFO
#include "PicoMemoryInfo.h"
#endif

//------------------- Állomás memória
#include "StationStore.h"
FmStationStore fmStationStore;
AmStationStore amStationStore;

//------------------- Képernyő
#include "AmDisplay.h"
#include "FmDisplay.h"
#include "FreqScanDisplay.h"
#include "MemoryDisplay.h"
#include "ScreenSaverDisplay.h"
#include "SetupDisplay.h"
DisplayBase *pDisplay = nullptr;

/**
 * Globális változó az aktuális kijelző váltásának előjegyzése
 * Induláskor FM - módban indulunk
 * (Ezt a globális változót a képernyők állítgatják, ha más képernyőt választ a felhasználó)
 */
DisplayBase::DisplayType newDisplay;
DisplayBase::DisplayType currentDisplay = DisplayBase::DisplayType::none;

// A képernyővédő elindulása előtti screen pointere, majd erre állunk vissza
DisplayBase *pDisplayBeforeScreenSaver = nullptr;

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
                // Elmentjük a beállítások képernyőnek, hogy hova térjen vissza
                ::pDisplay->setPrevDisplayType(::currentDisplay);
                break;

            case DisplayBase::DisplayType::memory:
                ::pDisplay = new MemoryDisplay(tft, si4735, band);
                // Beállítjuk a képernyőnek, hogy hova térjen vissza
                ::pDisplay->setPrevDisplayType(::currentDisplay);
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
bool hardwareTimerHandler1(struct repeating_timer *t) {
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
    pinMode(LED_BUILTIN, OUTPUT);
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
    ITimer1.attachInterruptInterval(ROTARY_ENCODER_SERVICE_INTERVAL_IN_MSEC * 1000, hardwareTimerHandler1);
#endif

    // TFT inicializálása
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_COLOR_BACKGROUND);

// Várakozás a soros port megnyitására DEBUG módban
#ifdef DEBUG_WAIT_FOR_SERIAL
    Utils::debugWaitForSerial(tft);
#endif

    // EEPROM inicializálása (A fordítónak muszáj megadni egy típust, itt most egy Config_t-t használunk, igaziból mindegy)
    EepromManager<Config_t>::init();  // Meghívjuk a statikus init metódust

    // Ha a bekapcsolás alatt nyomva tartjuk a rotary gombját, akkor töröljük a konfigot
    if (digitalRead(PIN_ENCODER_SW) == LOW) {
        Utils::beepTick();
        delay(1500);
        if (digitalRead(PIN_ENCODER_SW) == LOW) {  // Ha még mindig nyomják
            config.loadDefaults();
            Utils::beepTick();
            DEBUG("Default settings resored!\n");
        }
    } else {
        // konfig betöltése
        config.load();
    }
    // Kell kalibrálni a TFT Touch-t?
    if (Utils::isZeroArray(config.data.tftCalibrateData)) {
        Utils::beepError();
        Utils::tftTouchCalibrate(tft, config.data.tftCalibrateData);
    }
    // Beállítjuk a touch scren-t
    tft.setTouch(config.data.tftCalibrateData);

    // Állomáslisták betöltése az EEPROM-ból (a config után!) // <-- ÚJ
    fmStationStore.load();
    amStationStore.load();

    // Az si473x (Nem a default I2C lábakon [4,5] van!!!)
    Wire.setSDA(PIN_SI4735_I2C_SDA);  // I2C for SI4735 SDA
    Wire.setSCL(PIN_SI4735_I2C_SCL);  // I2C for SI4735 SCL
    Wire.begin();

    // Si4735 inicializálása
    int16_t si4735Addr = si4735.getDeviceI2CAddress(PIN_SI4735_RESET);
    if (si4735Addr == 0) {
        tft.setTextColor(TFT_RED, TFT_COLOR_BACKGROUND);
        const char *txt = "Si4735 not detected";
        tft.print(txt);
        DEBUG(txt);
        Utils::beepError();
        while (true)  // nem megyünk tovább
            ;
    }
    si4735.setDeviceI2CAddress(si4735Addr == 0x11 ? 0 : 1);  // Sets the I2C Bus Address, erre is szükség van...
    si4735.setAudioMuteMcuPin(PIN_AUDIO_MUTE);               // Audio Mute pin

    // Megtaláltuk az SI4735-öt, kiírjuk az I2C címét a képernyőre
    tft.setFreeFont();
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_COLOR_BACKGROUND);
    tft.print(F("Si473X addr:  0x"));
    tft.println(si4735Addr, HEX);

    //--------------------------------------------------------------------

    rtv::freqstep = 1000;  // hz
    rtv::freqDec = config.data.currentBFO;

    // Kezdő képernyőtípus beállítása
    ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;

    //--------------------------------------------------------------------

    // Kezdő mód képernyőjének megjelenítése
    changeDisplay();

    // Csippantunk egyet
    Utils::beepTick();

    // PICO AD inicializálása
    PicoSensorUtils::init();
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
#endif

    // Rotary Encoder olvasása
    RotaryEncoder::EncoderState encoderState = rotaryEncoder.read();
    // Ha folyamatosan nyomva tartják a rotary gombját akkor kikapcsolunk
    if (encoderState.buttonState == RotaryEncoder::ButtonState::Held) {
        // TODO: Kikapcsolás figyelését még implementálni
        DEBUG("Ki kellene kapcsolni...\n");
        Utils::beepError();
        delay(1000);
        return;
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
        if (millis() - lastScreenSaver >= SCREEN_SAVER_TIME) {

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
