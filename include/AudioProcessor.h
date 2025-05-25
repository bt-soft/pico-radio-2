#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <ArduinoFFT.h>

// Constants that are independent of the sampling rate provided at construction
namespace AudioProcessorConstants {
// Default FFT sizes - these can be overridden at runtime
constexpr uint16_t DEFAULT_FFT_SAMPLES = 256;  // Alapértelmezett FFT minták száma (2 hatványának kell lennie)
constexpr uint16_t MIN_FFT_SAMPLES = 64;       // Minimális FFT minták száma
constexpr uint16_t MAX_FFT_SAMPLES = 2048;     // Maximális FFT minták száma

// Backward compatibility
constexpr uint16_t FFT_SAMPLES = DEFAULT_FFT_SAMPLES;  // DEPRECATED: Use setFftSize() instead

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

class AudioProcessor {
   public:
    AudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency, uint16_t fftSize = AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    ~AudioProcessor();

    void process(bool collectOsciSamples);

    const double* getMagnitudeData() const { return RvReal; }

    const int* getOscilloscopeData() const { return osciSamples; }

    float getBinWidthHz() const { return binWidthHz_; }

    // Dynamic FFT size management
    bool setFftSize(uint16_t newFftSize);

    uint16_t getFftSize() const { return currentFftSize_; }

    uint16_t getMagnitudeDataSize() const { return currentFftSize_ / 2; }

    // Getters for backward compatibility and utility
    int getOscilloscopeDataSize() const { return AudioProcessorConstants::MAX_INTERNAL_WIDTH; }

   private:
    // Dynamic memory for FFT arrays
    double* vReal;
    double* vImag;
    double* RvReal;                                                // Magnitúdók tárolására
    int osciSamples[AudioProcessorConstants::MAX_INTERNAL_WIDTH];  // MAX_INTERNAL_WIDTH maradhat itt, ha az oszcilloszkóp buffer mérete fix

    // FFT configuration
    uint16_t currentFftSize_;  // Current FFT size
    ArduinoFFT<double> FFT;    // FFT objektum

    // Helper method to allocate/reallocate FFT arrays
    bool allocateFftArrays(uint16_t size);
    void deallocateFftArrays();
    bool validateFftSize(uint16_t size) const;

   protected:
    float& activeFftGainConfigRef;  // Referencia a Config_t gain mezőjére
    int audioInputPin;

    double targetSamplingFrequency_;   // Cél mintavételezési frekvencia
    uint32_t sampleIntervalMicros_;    // Egy mintára jutó időköz mikroszekundumban
    float binWidthHz_;                 // Cél frekvencia alapján számolt bin szélesség
    float smoothed_auto_gain_factor_;  // Simított erősítési faktor az auto gain-hez
};

#endif  // AUDIO_PROCESSOR_H
