/**
 * @file HighResAudioProcessor.cpp
 * @brief Nagy felbontású audio feldolgozó implementációja
 * * Ez a fájl tartalmazza a nagy felbontású (512 mintás) FFT audio feldolgozó
 * implementációját, amely 2x jobb frekvencia felbontást biztosít a TuningAid
 * mód számára. A feldolgozás hasonló az alapvető AudioProcessor-hoz, de
 * nagyobb FFT mérettel dolgozik.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#include "HighResAudioProcessor.h"

#include "defines.h"

/**
 * @brief Konstruktor - Nagy felbontású audio feldolgozó inicializálása
 *
 * Inicializálja a nagy felbontású FFT rendszert a dinamikus AudioProcessor
 * alaposztály segítségével. Az FFT méret HIGH_RES_FFT_SAMPLES-re lesz beállítva.
 *
 * @param gainConfigRef Referencia az FFT erősítési konfigurációra
 * @param audioPin Audio input pin száma az ADC-hez
 * @param targetSamplingFrequency Cél mintavételi frekvencia Hz-ben
 */
HighResAudioProcessor::HighResAudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency)
    : AudioProcessor(gainConfigRef, audioPin, targetSamplingFrequency, HIGH_RES_FFT_SAMPLES) {
    DEBUG("HighResAudioProcessor: Initialized with FFT size %d, bin width: %.2f Hz\n", getFftSize(), getBinWidthHz());
}
