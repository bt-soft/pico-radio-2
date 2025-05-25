/**
 * @file HighResAudioProcessor.h
 * @brief Nagy felbontású audio feldolgozó osztály speciálisan TuningAid módhoz
 *
 * Ez a modul egy speciális audio feldolgozót valósít meg, amely 4x-es FFT
 * felbontást (1024 minták) biztosít az alapértelmezett 512 mintával szemben.
 * Elsősorban TuningAid módban való használatra tervezték, ahol maximális
 * frekvencia precizitás szükséges CW és RTTY jelek elemzéséhez.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#ifndef HIGH_RES_AUDIO_PROCESSOR_H
#define HIGH_RES_AUDIO_PROCESSOR_H

#include "AudioProcessor.h"

/**
 * @brief Nagy felbontású audio feldolgozó csak a TuningAid módhoz
 *
 * Ez az osztály az AudioProcessor-ból származik és nagyobb FFT mérettel dolgozik,
 * hogy 4x jobb frekvencia felbontást biztosítson a CW és RTTY hangolási segédhez.
 * Az osztály öröklés útján használja az alapvető audio feldolgozási funkciókat,
 * de saját nagy felbontású FFT pufferekkel és feldolgozással rendelkezik.
 *
 * Jellemzők:
 * - 1024 mintás FFT (vs. 512 az alapértelmezett AudioProcessor-ban)
 * - 4x jobb frekvencia felbontás (pl. 5.85 Hz/bin vs. 11.7 Hz/bin)
 * - Speciálisan TuningAid alkalmazásokhoz optimalizált
 * - Nagyobb memóriaigény, ezért csak szükség esetén használandó
 */
class HighResAudioProcessor : public AudioProcessor {
   public:
    /** @brief Nagy felbontású FFT minták száma (4x az alapértelmezetthez képest) */
    static constexpr uint16_t HIGH_RES_FFT_SAMPLES = 1024;

    /**
     * @brief Konstruktor - Nagy felbontású audio feldolgozó inicializálása
     *
     * @param gainConfigRef Referencia az FFT erősítési konfigurációra
     * @param audioPin Audio input pin száma az ADC-hez
     * @param targetSamplingFrequency Cél mintavételi frekvencia Hz-ben
     */
    HighResAudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency);

    /**
     * @brief Destruktor - Erőforrások felszabadítása
     */
    ~HighResAudioProcessor();

    /**
     * @brief Nagy felbontású audio feldolgozás végrehajtása
     *
     * Ez a metódus végzi el a teljes nagy felbontású FFT feldolgozást:
     * 1. 1024 minta gyűjtése az ADC-ről
     * 2. DC komponens eltávolítása és erősítés alkalmazása
     * 3. Hamming ablakozás alkalmazása
     * 4. FFT számítás
     * 5. Komplex -> magnitúdó konverzió
     * 6. Alacsony frekvenciás csillapítás
     *
     * @param collectOsciSamples Opcionális oszcilloszkóp minták gyűjtése (alapértelmezett: false)
     */
    void processHighRes(bool collectOsciSamples = false);

    /**
     * @brief Nagy felbontású FFT magnitúdó adatok lekérése
     *
     * @return Pointer a nagy felbontású magnitúdó adatokra (1024 elem fele = 512 használható bin)
     */
    const double* getHighResMagnitudeData() const { return highResRvReal; }

    /**
     * @brief Nagy felbontású bin szélesség lekérése
     *
     * @return Egy FFT bin szélessége Hz-ben (körülbelül 5.85 Hz 48kHz mintavételnél)
     */
    float getHighResBinWidthHz() const { return highResBinWidthHz_; }

   private:
    /** @brief Nagy felbontású FFT valós rész (bemenet) */
    double highResVReal[HIGH_RES_FFT_SAMPLES];

    /** @brief Nagy felbontású FFT imaginárius rész (bemenet) */
    double highResVImag[HIGH_RES_FFT_SAMPLES];

    /** @brief Nagy felbontású FFT magnitúdó eredmények (kimenet) */
    double highResRvReal[HIGH_RES_FFT_SAMPLES];

    /** @brief Nagy felbontású bin szélesség Hz-ben */
    float highResBinWidthHz_;

    /** @brief Nagy felbontású FFT objektum */
    ArduinoFFT<double> highResFFT;
};

#endif  // HIGH_RES_AUDIO_PROCESSOR_H
