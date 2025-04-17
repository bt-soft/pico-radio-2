#include "Config.h"

#include "Si4735Utils.h"

/**
 * Alapértelmezett readonly konfigurációs adatok
 */
const Config_t DEFAULT_CONFIG = {

    //-- Band
    .bandIdx = 0,  // Default band, FM
    // .currentFreq = 9390,  // Petőfi 93.9MHz
    // .currentStep = 100,   // Default step, 100kHz
    // .currentMode = 0,     // FM-ben infulunk először

    // BandWidht
    .bwIdxAM = 0,   // BandWidth AM - Band::bandWidthAM index szerint -> "6.0" kHz
    .bwIdxFM = 0,   // BandWidth FM - Band::bandWidthFM[] index szerint -> "AUTO"
    .bwIdxMW = 0,   // BandWidth MW - Band::bandWidthAM index szerint -> "6.0" kHz
    .bwIdxSSB = 0,  // BandWidth SSB - Band::bandWidthSSB[] index szerint -> "1.2" kHz  (the valid values are 0, 1, 2, 3, 4 or 5)

    // Step
    .ssIdxMW = 2,  // Band::stepSizeAM[] index szerint -> 9kHz
    .ssIdxAM = 1,  // Band::stepSizeAM[] index szerint -> 5kHz
    .ssIdxFM = 1,  // Band::stepSizeFM[] index szerint -> 100kHz

    // BFO
    .currentBFO = 0,
    .currentBFOStep = 25,
    .currentBFOmanu = 0,

    // Squelch
    .currentSquelch = 0,
    .squelchUsesRSSI = true,  // A squlech RSSI alapú legyen?

    // FM RDS
    .rdsEnabled = true,

    // Hangerő
    .currVolume = 50,

    // AGC
    .agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Automatic),         // -> 1,
    .currentAGCgain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Automatic),  // -> 1

    //--- TFT
    //.tftCalibrateData = {0, 0, 0, 0, 0},  // TFT touch kalibrációs adatok
    .tftCalibrateData = {213, 3717, 234, 3613, 7},
    .tftBackgroundBrightness = TFT_BACKGROUND_LED_MAX_BRIGHTNESS,  // TFT Háttérvilágítás
    .tftDigitLigth = true,                                         // Inaktív szegmens látszódjon?
};
