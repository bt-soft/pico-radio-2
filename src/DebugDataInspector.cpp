#include "DebugDataInspector.h"

#include "Config.h"
#include "utils.h"

/**
 * @brief Kiírja az FM állomáslista tartalmát a soros portra.
 * @param fmStore Az FM állomáslista objektum.
 */
void DebugDataInspector::printFmStationData(const FmStationList_t& fmData) {
#ifdef __DEBUG
    DEBUG("=== DebugDataInspector -> FM Station Store ===\n");
    for (size_t i = 0; i < fmData.count; ++i) {
        const StationData& station = fmData.stations[i];
        DEBUG("  Station %d: Freq: %d, Name: %s, Mod: %d, BFO: %d, BW: %d\n", i, station.frequency, station.name, station.modulation, station.bfoOffset, station.bandwidthIndex);
    }
    DEBUG("====================\n");
#endif
}

/**
 * @brief Kiírja az AM állomáslista tartalmát a soros portra.
 * @param amStore Az AM állomáslista objektum.
 */
void DebugDataInspector::printAmStationData(const AmStationList_t& amData) {
#ifdef __DEBUG
    Serial.println("=== DebugDataInspector -> AM Station Store ===");
    for (size_t i = 0; i < amData.count; ++i) {
        const StationData& station = amData.stations[i];
        DEBUG("  Station %d: Freq: %d, Name: %s, Mod: %d, BFO: %d, BW: %d\n", i, station.frequency, station.name, station.modulation, station.bfoOffset, station.bandwidthIndex);
    }
    DEBUG("====================\n");
#endif
}

/**
 * @brief Kiírja a Config struktúra tartalmát a soros portra.
 * @param config A Config objektum.
 */
void DebugDataInspector::printConfigData(const Config_t& configData) {
#ifdef __DEBUG
    DEBUG("=== DebugDataInspector -> Config Data ===\n");
    DEBUG("  bandIdx: %u\n", configData.bandIdx);
    DEBUG("  bwIdxAM: %u\n", configData.bwIdxAM);
    DEBUG("  bwIdxFM: %u\n", configData.bwIdxFM);
    DEBUG("  bwIdxMW: %u\n", configData.bwIdxMW);
    DEBUG("  bwIdxSSB: %u\n", configData.bwIdxSSB);
    DEBUG("  ssIdxMW: %u\n", configData.ssIdxMW);
    DEBUG("  ssIdxAM: %u\n", configData.ssIdxAM);
    DEBUG("  ssIdxFM: %u\n", configData.ssIdxFM);
    DEBUG("  currentBFO: %d\n", configData.currentBFO);
    DEBUG("  currentBFOStep: %u\n", configData.currentBFOStep);
    DEBUG("  currentBFOmanu: %d\n", configData.currentBFOmanu);
    DEBUG("  currentSquelch: %u\n", configData.currentSquelch);
    DEBUG("  squelchUsesRSSI: %s\n", configData.squelchUsesRSSI ? "true" : "false");
    DEBUG("  rdsEnabled: %s\n", configData.rdsEnabled ? "true" : "false");
    DEBUG("  currVolume: %u\n", configData.currVolume);
    DEBUG("  agcGain: %u\n", configData.agcGain);
    DEBUG("  currentAGCgain: %u\n", configData.currentAGCgain);
    DEBUG("  tftCalibrateData: [%u, %u, %u, %u, %u]\n", configData.tftCalibrateData[0], configData.tftCalibrateData[1], configData.tftCalibrateData[2],
          configData.tftCalibrateData[3], configData.tftCalibrateData[4]);
    DEBUG("  tftBackgroundBrightness: %u\n", configData.tftBackgroundBrightness);
    DEBUG("  tftDigitLigth: %s\n", configData.tftDigitLigth ? "true" : "false");
    DEBUG("  screenSaverTimeoutMinutes: %u\n", configData.screenSaverTimeoutMinutes);
    DEBUG("  miniAudioFftModeAm: %u\n", configData.miniAudioFftModeAm);
    DEBUG("  miniAudioFftModeFm: %u\n", configData.miniAudioFftModeFm);
    DEBUG("====================\n");
#endif
}
