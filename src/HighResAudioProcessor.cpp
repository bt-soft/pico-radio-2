/**
 * @file HighResAudioProcessor.cpp
 * @brief Nagy felbontású audio feldolgozó implementációja
 *
 * Ez a fájl tartalmazza a nagy felbontású (1024 mintás) FFT audio feldolgozó
 * implementációját, amely 4x jobb frekvencia felbontást biztosít a TuningAid
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
 * Inicializálja a nagy felbontású FFT rendszert és kiszámítja a bin szélességet.
 * Az alapvető AudioProcessor funkcionalitást öröklés útján használja fel.
 *
 * @param gainConfigRef Referencia az FFT erősítési konfigurációra
 * @param audioPin Audio input pin száma az ADC-hez
 * @param targetSamplingFrequency Cél mintavételi frekvencia Hz-ben
 */
HighResAudioProcessor::HighResAudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency)
    : AudioProcessor(gainConfigRef, audioPin, targetSamplingFrequency), highResFFT(ArduinoFFT<double>(highResVReal, highResVImag, HIGH_RES_FFT_SAMPLES, targetSamplingFrequency)) {

    // Nagy felbontású bin szélesség kiszámítása
    if (targetSamplingFrequency > 0) {
        highResBinWidthHz_ = static_cast<float>(targetSamplingFrequency) / HIGH_RES_FFT_SAMPLES;
    } else {
        // Fallback esetén a sampleIntervalMicros_ alapján számoljuk
        double fallbackSamplingFreq = 1000000.0 / sampleIntervalMicros_;
        highResBinWidthHz_ = static_cast<float>(fallbackSamplingFreq) / HIGH_RES_FFT_SAMPLES;
    }

    // Debug információ a felbontás javulásról
    DEBUG("HighResAudioProcessor: Nagy felbontású bin szélesség: %.2f Hz (4x jobb mint a standard %.2f Hz)\n", highResBinWidthHz_, binWidthHz_);

    // Nagy felbontású pufferek inicializálása nullával
    memset(highResVReal, 0, sizeof(highResVReal));
    memset(highResVImag, 0, sizeof(highResVImag));
    memset(highResRvReal, 0, sizeof(highResRvReal));
}

/**
 * @brief Destruktor - Erőforrások felszabadítása
 *
 * Az alapértelmezett destruktor elegendő, mivel nincsenek dinamikusan
 * allokált erőforrások. A tömb pufferek automatikusan felszabadulnak.
 */
HighResAudioProcessor::~HighResAudioProcessor() {
    // Alapértelmezett destruktor - nincs különleges teendő
}

/**
 * @brief Nagy felbontású audio feldolgozás végrehajtása
 *
 * Ez a metódus végzi el a teljes nagy felbontású FFT feldolgozási folyamatot:
 * 1. 1024 audio minta gyűjtése az ADC-ről pontos időzítéssel
 * 2. DC komponens eltávolítása és középre pozicionálás
 * 3. FFT erősítés alkalmazása
 * 4. Hamming ablakozás alkalmazása a spektrális szivárgás csökkentésére
 * 5. FFT transzformáció végrehajtása
 * 6. Komplex eredmények konvertálása magnitúdó értékekre
 * 7. Alacsony frekvenciás csillapítás alkalmazása
 *
 * A folyamat hasonló az alapvető AudioProcessor-hoz, de 1024 mintával dolgozik
 * a 512 helyett, így 4x jobb frekvencia felbontást érve el.
 *
 * @param collectOsciSamples Opcionális oszcilloszkóp minták gyűjtése (jelenleg nem implementált)
 */
void HighResAudioProcessor::processHighRes(bool collectOsciSamples) {
    // Mintavételezés nagy felbontáshoz - 1024 minta gyűjtése
    uint32_t currentMicros = micros();
    uint32_t lastSampleMicros = currentMicros;

    for (uint16_t i = 0; i < HIGH_RES_FFT_SAMPLES; i++) {
        // Várakozás a következő mintára a pontos időzítés biztosítására
        while (micros() - lastSampleMicros < sampleIntervalMicros_) {
            // Aktív várakozás - pontosabb mint delayMicroseconds()
            // Ez biztosítja a konzisztens mintavételi frekvenciát
        }
        lastSampleMicros = micros();

        // ADC olvasás a megadott audio pin-ről
        int rawValue = analogRead(audioInputPin);

        // DC eltávolítása és középre pozicionálás
        // Az ADC 0-4095 tartományban ad értékeket, a középpont 2048 körül van
        double centeredValue = rawValue - 2048.0;

        // FFT erősítés alkalmazása (a felhasználó által beállított értékkel)
        centeredValue *= activeFftGainConfigRef;

        // FFT bemenet pufferbe mentés
        highResVReal[i] = centeredValue;
        highResVImag[i] = 0.0;  // Valós jel, imaginárius rész mindig nulla
    }

    // FFT feldolgozás végrehajtása
    // 1. Hamming ablakozás alkalmazása a spektrális szivárgás csökkentésére
    highResFFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);

    // 2. FFT transzformáció végrehajtása
    highResFFT.compute(FFTDirection::Forward);

    // 3. Komplex eredmények konvertálása magnitúdó értékekre
    highResFFT.complexToMagnitude();

    // Magnitúdó adatok feldolgozása és alacsony frekvenciás csillapítás
    for (uint16_t i = 0; i < HIGH_RES_FFT_SAMPLES / 2; i++) {
        // FFT eredmények másolása a kimeneti pufferbe
        highResRvReal[i] = highResVReal[i];

        // Alacsony frekvenciák csillapítása (ahogy a standard AudioProcessor-ban is)
        // Ez csökkenti az alacsony frekvenciás zajokat és DC komponenseket
        float freqHz = i * highResBinWidthHz_;
        if (freqHz < AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ) {
            highResRvReal[i] /= AudioProcessorConstants::LOW_FREQ_ATTENUATION_FACTOR;
        }
    }

    // Debug információ a feldolgozás befejezéséről
    DEBUG("HighResAudioProcessor: Feldolgozva %d minta, bin szélesség: %.2f Hz\n", HIGH_RES_FFT_SAMPLES, highResBinWidthHz_);
}
