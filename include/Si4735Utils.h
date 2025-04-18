#ifndef __SI4735UTILS_H
#define __SI4735UTILS_H

#include <SI4735.h>

#include "Band.h"

/**
 * si4735 utilities
 */
class Si4735Utils {
   private:
    static int8_t currentBandIdx;
    bool hardwareAudioMuteState;        // SI4735 hardware audio mute állapot
    uint32_t hardwareAudioMuteElapsed;  // SI4735 hardware audio mute állapot start ideje

    /**
     * Manage Audio Mute
     * (SSB/CW frekvenciaváltáskor a zajszűrés miatt)
     */
    void manageHardwareAudioMute();

   protected:
    // SI4735
    SI4735 &si4735;

    // Band objektum
    Band &band;

    /**
     * Manage Squelch
     */
    void manageSquelch();

    /**
     * AGC beállítása
     */
    void checkAGC();

    /**
     * Arduino loop
     */
    void loop();

   public:
    // AGC beállítási lehetőségek
    enum class AgcGainMode : uint8_t {
        Off = 0,        // AGC kikapcsolva (de technikailag aktív marad, csak a csillapítás 0)
        Automatic = 1,  // AGC engedélyezve (teljesen automatikus működés)
        Manual = 2      // AGC manuális beállítással (a config.data.currentAGCgain értékével)
    };

    /**
     * Konstruktor
     */
    Si4735Utils(SI4735 &si4735, Band &band);

    /**
     * Frequency Step set
     */
    void setStep();

    /**
     * Mute Audio On
     * (SSB/CW frekvenciaváltáskor a zajszűrés miatt)
     */
    void hardwareAudioMuteOn();
};

#endif  //__SI4735UTILS_H
