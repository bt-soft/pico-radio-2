#ifndef __DEBUGDATAINSPECTOR_H
#define __DEBUGDATAINSPECTOR_H

#include <Arduino.h>

// Forward declare Config_t to break the include cycle
struct Config_t;
// Include StationData for list types
#include "StationData.h"  // FmStationList_t, AmStationList_t, StationData definíciók

// A Config.h-t itt már nem includoljuk, mert az körkörös függőséget okoz.
// A Config.h includolja ezt a fájlt, és mire a printConfigData inline definíciójához ér a fordító,
// addigra a Config_t már definiálva lesz a Config.h-ban.

class DebugDataInspector {
   public:
    /**
     * @brief Kiírja a Config struktúra tartalmát a soros portra.
     * @param config A Config objektum.
     */
    static void printConfigData(const Config_t& configData);  // Csak a deklaráció marad

    /**
     * @brief Kiírja az FM állomáslista tartalmát a soros portra.
     * @param fmStore Az FM állomáslista objektum.
     */
    static void printFmStationData(const FmStationList_t& fmData);  // Csak a deklaráció marad

    /**
     * @brief Kiírja az AM állomáslista tartalmát a soros portra.
     * @param amStore Az AM állomáslista objektum.
     */
    static void printAmStationData(const AmStationList_t& amData);  // Csak a deklaráció marad
};

#endif  // __DEBUGDATAINSPECTOR_H