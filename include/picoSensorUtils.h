#ifndef __PICO_SENSOR_UTILS_H
#define __PICO_SENSOR_UTILS_H

#include <Arduino.h>

namespace PicoSensorUtils {

// --- Konstansok ---
const int ADC_GPIO_PIN = 29;                 // GP26 -> ADC0
const float VREF = 3.3;                      // Pico referenciafeszültsége
const float R1 = 197.5;                      // Felső ellenállás (Ohm)
const float R2 = 99.5;                       // Alsó ellenállás (Ohm)
const float DIVIDER_RATIO = (R1 + R2) / R2;  // Feszültségosztó aránya (Vin / Vout)
const int ADC_MAX_READING = 4095;            // 12 bites ADC maximális értéke

/**
 * ADC olvasás és feszültség kiszámítása
 */
float readVBus() {
    // 1. Nyers ADC érték olvasása (0-4095)
    int rawValue = analogRead(A0);

    // 2. Nyers érték átalakítása az ADC lábán mért feszültségre (Vout)
    //    Fontos a float típus használata a pontos osztáshoz!
    float vout = (float)rawValue * (VREF / (float)ADC_MAX_READING);

    // 3. Az eredeti feszültség (Vin) kiszámítása az osztó arányával
    float vin = vout * DIVIDER_RATIO;

    return vin;  // Visszaadjuk a feszültséget
}

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
float readCoreTemperature() {

    return analogReadTemp();  // Kiolvassuk a processzor hőmérsékletét
}

};  // namespace PicoSensorUtils

#endif  // __SENSOR_UTILS_H
