/**
 * @file HighResAudioProcessor.h
 * @brief Nagy felbontású audio feldolgozó osztály speciálisan TuningAid módhoz
 * * Ez a modul egy speciális audio feldolgozót valósít meg, amely 2x-es FFT
 * felbontást (512 minták) biztosít az alapértelmezett 256 mintával szemben.
 * Elsősorban TuningAid módban való használatra tervezték, ahol jobb
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
 * * Ez az osztály az AudioProcessor-ból származik és nagyobb FFT mérettel dolgozik,
 * hogy 2x jobb frekvencia felbontást biztosítson a CW és RTTY hangolási segédhez.
 * Az osztály öröklés útján használja az alapvető audio feldolgozási funkciókat,
 * de saját nagy felbontású FFT pufferekkel és feldolgozással rendelkezik.
 * * Jellemzők:
 * - 512 mintás FFT (vs. 256 az alapértelmezett AudioProcessor-ban)
 * - 2x jobb frekvencia felbontás (pl. 93.75 Hz/bin vs. 187.5 Hz/bin)
 * - Speciálisan TuningAid alkalmazásokhoz optimalizált
 * - Nagyobb memóriaigény, ezért csak szükség esetén használandó
 */
class HighResAudioProcessor : public AudioProcessor {
   public:
    /** @brief Nagy felbontású FFT minták száma (2x az alapértelmezetthez képest) */
    static constexpr uint16_t HIGH_RES_FFT_SAMPLES = 512;

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
    ~HighResAudioProcessor() = default;

    /**
     * @brief Nagy felbontású audio feldolgozás végrehajtása
     *
     * Ez a metódus egyszerűen meghívja az örökölt process() metódust,
     * mivel a dinamikus FFT méret már be van állítva a konstruktorban.
     *
     * @param collectOsciSamples Opcionális oszcilloszkóp minták gyűjtése (alapértelmezett: false)
     */
    void processHighRes(bool collectOsciSamples = false) { process(collectOsciSamples); }

    /**
     * @brief Nagy felbontású FFT magnitúdó adatok lekérése
     *
     * @return Pointer a nagy felbontású magnitúdó adatokra (512 elem fele = 256 használható bin)
     */
    const double* getHighResMagnitudeData() const { return getMagnitudeData(); }

    /**
     * @brief Nagy felbontású bin szélesség lekérése
     *
     * @return Egy FFT bin szélessége Hz-ben (körülbelül 93.75 Hz 48kHz mintavételnél)
     */
    float getHighResBinWidthHz() const { return getBinWidthHz(); }
};

#endif  // HIGH_RES_AUDIO_PROCESSOR_H
