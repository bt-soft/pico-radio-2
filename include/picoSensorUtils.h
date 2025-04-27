#ifndef __PICO_SENSOR_UTILS_H
#define __PICO_SENSOR_UTILS_H

#include <Arduino.h>

namespace PicoSensorUtils {

#define AD_RESOLUTION 12  // 12 bites az ADC

/**
 * AD inicializálása
 */
inline void init() { analogReadResolution(AD_RESOLUTION); }

/**
 * ADC olvasás és VBUS feszültség kiszámítása külső osztóval GP29-en.
 * @return A VBUS mért feszültsége Voltban.
 */
float readVBus();

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
float readCoreTemperature();

};  // namespace PicoSensorUtils

#endif  // __PICO_SENSOR_UTILS_H
