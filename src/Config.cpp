#include "Config.h"

#include "Si4735Utils.h"

/**
 * Alapértelmezett readonly konfigurációs adatok
 */
const Config_t DEFAULT_CONFIG = {

    //-- Band
    .bandIdx = 0,  // Default band, FM

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
    .currentBFOStep = 25,  // BFO lépésköz (pl. 1, 10, 25 Hz)
    .currentBFOmanu = 0,   // Manuális BFO eltolás (pl. -999 ... +999 Hz)

    // Squelch
    .currentSquelch = 0,      // Squelch szint (0...50)
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
    .screenSaverTimeoutMinutes = SCREEN_SAVER_TIMEOUT,             // Képernyővédő alapértelmezetten 5 perc
    .beeperEnabled = true,                                         // Hangjelzés engedélyezése

    // MiniAudioFft módok (kezdetben SpectrumLowRes)
    .miniAudioFftModeAm = static_cast<uint8_t>(MiniAudioFft::DisplayMode::SpectrumLowRes),
    .miniAudioFftModeFm = static_cast<uint8_t>(MiniAudioFft::DisplayMode::SpectrumLowRes),

    // MiniAudioFft erősítés
    .miniAudioFftConfigAm = 0.0f,  // Auto Gain
    .miniAudioFftConfigFm = 0.0f,  // Auto Gain
    .miniAudioFftConfigAnalyzer = 0.0f, // Analyzerhez alapértelmezetten Auto Gain
};
