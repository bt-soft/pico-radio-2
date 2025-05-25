#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <ArduinoFFT.h>

/**
 * @brief Konstansok az AudioProcessor osztályhoz - mintavételezési frekvenciától függetlenek
 */
namespace AudioProcessorConstants {

constexpr uint16_t MIN_FFT_SAMPLES = 64;    // Minimális FFT minták száma
constexpr uint16_t MAX_FFT_SAMPLES = 2048;  // Maximális FFT minták száma

// Alapértelmezett FFT méretek - futásidőben felülírhatók
constexpr uint16_t DEFAULT_FFT_SAMPLES = 256;   // Alapértelmezett FFT minták száma (2 hatványának kell lennie)
constexpr uint16_t HIGH_RES_FFT_SAMPLES = 512;  // Magas felbontású FFT minták száma

// Visszafelé kompatibilitás
constexpr uint16_t FFT_SAMPLES = DEFAULT_FFT_SAMPLES;  // DEPRECATED: Használd a setFftSize()-t helyette

constexpr float AMPLITUDE_SCALE = 40.0f;                     // Skálázási faktor az FFT eredményekhez (tovább csökkentve az érzékenység növeléséhez)
constexpr float LOW_FREQ_ATTENUATION_THRESHOLD_HZ = 200.0f;  // Ez alatti frekvenciákat csillapítjuk
constexpr float LOW_FREQ_ATTENUATION_FACTOR = 10.0f;         // Ezzel a faktorral osztjuk az alacsony frekvenciák magnitúdóját

// Konstansok az Auto Gain módhoz
constexpr float FFT_AUTO_GAIN_TARGET_PEAK = 500.0f;  // Cél csúcsérték az Auto Gain módhoz (a +/-2047 tartományból) - CSÖKKENTVE
constexpr float FFT_AUTO_GAIN_MIN_FACTOR = 0.1f;     // Minimális erősítési faktor Auto módban
constexpr float FFT_AUTO_GAIN_MAX_FACTOR = 10.0f;    // Maximális erősítési faktor Auto módban
constexpr float AUTO_GAIN_ATTACK_COEFF = 0.5f;       // Erősítés csökkentésének sebessége (0.0-1.0, nagyobb = gyorsabb)
constexpr float AUTO_GAIN_RELEASE_COEFF = 0.05f;     // Erősítés növelésének sebessége (0.0-1.0, nagyobb = gyorsabb)

constexpr int MAX_INTERNAL_WIDTH = 86;  // Oszcilloszkóp és magas felbontású spektrum belső bufferéhez

constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;  // Oszcilloszkóp mintavételi decimációs faktora

};  // namespace AudioProcessorConstants

/**
 * @brief Audio feldolgozó osztály FFT analízissel és oszcilloszkóp funkcióval
 */
class AudioProcessor {
   public:
    /**
     * @brief AudioProcessor konstruktor
     * @param gainConfigRef Referencia a gain konfigurációs értékre
     * @param audioPin Az audio bemenet pin száma
     * @param targetSamplingFrequency Cél mintavételezési frekvencia Hz-ben
     * @param fftSize FFT méret (alapértelmezett: DEFAULT_FFT_SAMPLES)
     */
    AudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency, uint16_t fftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    /**
     * @brief AudioProcessor destruktor
     */
    ~AudioProcessor();

    /**
     * @brief Fő audio feldolgozó függvény - mintavételezés, FFT számítás és spektrum analízis
     * @param collectOsciSamples true ha oszcilloszkóp mintákat is gyűjteni kell
     */
    void process(bool collectOsciSamples);

    /**
     * @brief Magnitúdó adatok lekérdezése
     * @return Pointer a magnitúdó adatokra
     */
    const double* getMagnitudeData() const { return RvReal; }

    /**
     * @brief Oszcilloszkóp adatok lekérdezése
     * @return Pointer az oszcilloszkóp adatokra
     */
    const int* getOscilloscopeData() const { return osciSamples; }

    /**
     * @brief Frekvencia bin szélesség lekérdezése Hz-ben
     * @return Bin szélesség Hz-ben
     */
    float getBinWidthHz() const { return binWidthHz_; }

    /**
     * @brief FFT méret beállítása futásidőben
     * @param newFftSize Az új FFT méret
     * @return true ha sikeres, false ha hiba történt
     */
    bool setFftSize(uint16_t newFftSize);

    /**
     * @brief Aktuális FFT méret lekérdezése
     * @return Az aktuális FFT méret
     */
    uint16_t getFftSize() const { return currentFftSize_; }

    /**
     * @brief Magnitúdó adatok méretének lekérdezése
     * @return A magnitúdó adatok száma (FFT méret fele)
     */
    uint16_t getMagnitudeDataSize() const { return currentFftSize_ / 2; }

    /**
     * @brief Oszcilloszkóp adatok méretének lekérdezése (visszafelé kompatibilitás és segédprogram)
     * @return Az oszcilloszkóp adatok száma
     */
    int getOscilloscopeDataSize() const { return AudioProcessorConstants::MAX_INTERNAL_WIDTH; }

   private:
    // Dinamikus memória az FFT tömbökhez
    double* vReal;
    double* vImag;
    double* RvReal;                                                // Magnitúdók tárolására
    int osciSamples[AudioProcessorConstants::MAX_INTERNAL_WIDTH];  // Oszcilloszkóp buffer (fix méret)

    // FFT konfiguráció
    uint16_t currentFftSize_;  // Aktuális FFT méret
    ArduinoFFT<double> FFT;    // FFT objektum

    /**
     * @brief Segédfüggvény FFT tömbök allokálásához/újrallokálásához
     * @param size A kívánt FFT méret
     * @return true ha sikeres, false ha hiba történt
     */
    bool allocateFftArrays(uint16_t size);

    /**
     * @brief FFT tömbök felszabadítása
     */
    void deallocateFftArrays();

    /**
     * @brief FFT méret érvényesítése
     * @param size Az ellenőrizendő FFT méret
     * @return true ha érvényes, false ha nem
     */
    bool validateFftSize(uint16_t size) const;

   protected:
    float& activeFftGainConfigRef;  // Referencia a Config_t gain mezőjére
    int audioInputPin;              // Audio bemenet pin száma

    double targetSamplingFrequency_;   // Cél mintavételezési frekvencia
    uint32_t sampleIntervalMicros_;    // Egy mintára jutó időköz mikroszekundumban
    float binWidthHz_;                 // Cél frekvencia alapján számolt bin szélesség
    float smoothed_auto_gain_factor_;  // Simított erősítési faktor az auto gain-hez
};

#endif  // AUDIO_PROCESSOR_H
