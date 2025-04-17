#include "Si4735Utils.h"

#include "Config.h"
#include "rtVars.h"

int8_t Si4735Utils::currentBandIdx = -1;  // Induláskor nincs kiválasztvba band

/**
 * Manage Squelch
 */
void Si4735Utils::manageSquelch() {

    // squelchIndicator(pCfg->vars.currentSquelch);
    if (!rtv::muteStat) {

        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();

        uint8_t signalQuality = config.data.squelchUsesRSSI ? rssi : snr;
        if (signalQuality >= config.data.currentSquelch) {

            if (rtv::SCANpause == true) {

                si4735.setAudioMute(false);
                rtv::squelchDecay = millis();
                DEBUG("Si4735Utils::manageSuelch ->  si4735.setAudioMute(false)\n");
            }
        } else {
            if (millis() > (rtv::squelchDecay + SQUELCH_DECAY_TIME)) {
                si4735.setAudioMute(true);
                DEBUG("Si4735Utils::manageSuelch -> si4735.setAudioMute(true)\n");
            }
        }
    }
}

/**
 * AGC beállítása
 */
void Si4735Utils::checkAGC() {

    // Először lekérdezzük az SI4735 chip aktuális AGC állapotát.
    //  Ez a hívás frissíti az SI4735 objektum belső állapotát az AGC-vel kapcsolatban (pl. hogy engedélyezve van-e vagy sem).
    si4735.getAutomaticGainControl();

    bool chipAgcEnabled = si4735.isAgcEnabled();
    AgcGainMode desiredMode = static_cast<AgcGainMode>(config.data.agcGain);
    bool stateChanged = false;  // Jelző, hogy történt-e változás, küldtünk-e AGC parancsot?

    // Ha az AGC engedélyezve van
    if (si4735.isAgcEnabled()) {

        if (desiredMode == AgcGainMode::Off) {

            DEBUG("Si4735Utils::checkAGC() -> AGC Off\n");
            // A felhasználó az AGC kikapcsolását kérte.
            if (chipAgcEnabled) {
                si4735.setAutomaticGainControl(1, 0);  // disabled
                stateChanged = true;
            }

        } else if (desiredMode == AgcGainMode::Manual) {

            DEBUG("Si4735Utils::checkAGC() -> AGC Manual\n");
            // A felhasználó manuális AGC beállítást kért
            si4735.setAutomaticGainControl(1, config.data.currentAGCgain);
            stateChanged = true;  // Feltételezzük, hogy változott az állapot
        }

    } else if (desiredMode == AgcGainMode::Automatic) {
        // Ha az AGC nincs engedélyezve az AGC, de a felhasználó az AGC engedélyezését kérte
        // Ez esetben az AGC-t engedélyezzük (0), és a csillapítást nullára állítjuk (0).
        // Ez a teljesen automatikus AGC működést jelenti.
        if (!chipAgcEnabled) {
            DEBUG("Si4735Utils::checkAGC() -> AGC Automatic\n");
            si4735.setAutomaticGainControl(0, 0);  // enabled
            stateChanged = true;
        }
    }

    // Ha küldtünk parancsot, olvassuk vissza az állapotot, hogy az SI4735 C++ objektum belső jelzője frissüljön
    if (stateChanged) {
        si4735.getAutomaticGainControl();  // Állapot újraolvasása
    }
}

/**
 * Loop függvény
 */
void Si4735Utils::loop() {
    //
    // this.manageSquelch();

    // A némítás után a hangot vissza kell állítani
    manageHardwareAudioMute();
}

/**
 * Konstruktor
 */
Si4735Utils::Si4735Utils(SI4735& si4735, Band& band) : si4735(si4735), band(band), hardwareAudioMuteState(false), hardwareAudioMuteElapsed(millis()) {

    DEBUG("Si4735Utils::Si4735Utils\n");

    // Band init, ha változott az épp használt band
    if (currentBandIdx != config.data.bandIdx) {

        // A Band  visszaállítása a konfiogból
        band.bandInit(currentBandIdx == -1);  // Rendszer induláskor -1 a currentBandIdx változást figyelő flag

        // A sávra preferált demodulációs mód betöltése
        band.bandSet(true);

        // Hangerő beállítása
        si4735.setVolume(config.data.currVolume);

        currentBandIdx = config.data.bandIdx;
    }

    // Rögtön be is állítjuk az AGC-t
    checkAGC();
}

/**
 *
 */
void Si4735Utils::setStep() {

    // This command should work only for SSB mode
    BandTable& currentBand = band.getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;

    if (rtv::bfoOn && (currMod == LSB or currMod == USB or currMod == CW)) {
        if (config.data.currentBFOStep == 1)
            config.data.currentBFOStep = 10;
        else if (config.data.currentBFOStep == 10)
            config.data.currentBFOStep = 25;
        else
            config.data.currentBFOStep = 1;
    }

    if (!rtv::SCANbut) {
        band.useBand();
        checkAGC();
    }
}

/**
 * Manage Audio Mute
 * (SSB/CW frekvenciaváltáskor a zajszűrés miatt)
 */
void Si4735Utils::hardwareAudioMuteOn() {
    si4735.setHardwareAudioMute(true);
    hardwareAudioMuteState = true;
    hardwareAudioMuteElapsed = millis();
}

/**
 *
 */
void Si4735Utils::manageHardwareAudioMute() {
#define MIN_ELAPSED_HARDWARE_AUDIO_MUTE_TIME 0  // Noise surpression SSB in mSec. 0 mSec = off //Was 0 (LWH)

    // Stop muting only if this condition has changed
    if (hardwareAudioMuteState and ((millis() - hardwareAudioMuteElapsed) > MIN_ELAPSED_HARDWARE_AUDIO_MUTE_TIME)) {
        // Ha a mute állapotban vagyunk és eltelt a minimális idő, akkor kikapcsoljuk a mute-t
        hardwareAudioMuteState = false;
        si4735.setHardwareAudioMute(false);
    }
}