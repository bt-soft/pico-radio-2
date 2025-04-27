#include "picoSensorUtils.h"  // Include a header a deklarációkhoz és konstansokhoz

namespace PicoSensorUtils {

// --- Konstansok ---
#define V_REFERENCE 3.3f
#define CONVERSION_FACTOR (1 << AD_RESOLUTION)

// Külső feszültségosztó ellenállásai a VBUS méréshez (A0-ra kötve)
#define PIN_VBUS_DIVIDER A0
#define R1 197.5f                       // Ellenállás VBUS és A0 között (kOhm)
#define R2 99.5f                        // Ellenállás A0 és GND között (kOhm)
#define DIVIDER_RATIO ((R1 + R2) / R2)  // Feszültségosztó aránya

/**
 * ADC olvasás és VBUS feszültség kiszámítása külső osztóval
 * @return A VBUS mért feszültsége Voltban.
 */
float readVBus() {

    // ADC érték átalakítása feszültséggé
    float voltageOut = (analogRead(PIN_VBUS_DIVIDER) * V_REFERENCE) / CONVERSION_FACTOR;

    // Eredeti feszültség számítása a feszültségosztó alapján
    return voltageOut * DIVIDER_RATIO;
}

/**
 * Kiolvassa a processzor hőmérsékletét
 * @return processzor hőmérséklete Celsius fokban
 */
float readCoreTemperature() { return analogReadTemp(); }

};  // namespace PicoSensorUtils
