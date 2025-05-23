#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <ArduinoFFT.h>

// Constants that are independent of the sampling rate provided at construction
namespace AudioProcessorConstants {
constexpr uint16_t FFT_SAMPLES = 256;  // Minták száma az FFT-hez (2 hatványának kell lennie)

constexpr float AMPLITUDE_SCALE = 40.0f;                     // Skálázási faktor az FFT eredményekhez (tovább csökkentve az érzékenység növeléséhez)
constexpr float LOW_FREQ_ATTENUATION_THRESHOLD_HZ = 200.0f;  // Ez alatti frekvenciákat csillapítjuk
constexpr float LOW_FREQ_ATTENUATION_FACTOR = 10.0f;         // Ezzel a faktorral osztjuk az alacsony frekvenciák magnitúdóját

// Konstansok az Auto Gain módhoz
constexpr float FFT_AUTO_GAIN_TARGET_PEAK = 500.0f;  // Cél csúcsérték az Auto Gain módhoz (a +/-2047 tartományból) - CSÖKKENTVE
constexpr float FFT_AUTO_GAIN_MIN_FACTOR = 0.1f;      // Minimális erősítési faktor Auto módban
constexpr float FFT_AUTO_GAIN_MAX_FACTOR = 10.0f;     // Maximális erősítési faktor Auto módban
constexpr float AUTO_GAIN_ATTACK_COEFF = 0.5f;   // Erősítés csökkentésének sebessége (0.0-1.0, nagyobb = gyorsabb)
constexpr float AUTO_GAIN_RELEASE_COEFF = 0.05f; // Erősítés növelésének sebessége (0.0-1.0, nagyobb = gyorsabb)

constexpr int MAX_INTERNAL_WIDTH = 86;  // Oszcilloszkóp és magas felbontású spektrum belső bufferéhez

constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;  // Oszcilloszkóp mintavételi decimációs faktora

};  // namespace AudioProcessorConstants

class AudioProcessor {
   public:
    AudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency);
    ~AudioProcessor();

    void process(bool collectOsciSamples);

    const double* getMagnitudeData() const { return RvReal; }

    const int* getOscilloscopeData() const { return osciSamples; }

    float getBinWidthHz() const { return binWidthHz_; }

    // Opcionális: Getterek a belső tömbök méretéhez, ha szükséges
    // int getMagnitudeDataSize() const { return AudioProcessorConstants::FFT_SAMPLES / 2; }
    // int getOscilloscopeDataSize() const { return AudioProcessorConstants::MAX_INTERNAL_WIDTH; }

   private:
    ArduinoFFT<double> FFT;
    double vReal[AudioProcessorConstants::FFT_SAMPLES];
    double vImag[AudioProcessorConstants::FFT_SAMPLES];
    double RvReal[AudioProcessorConstants::FFT_SAMPLES];           // Magnitúdók tárolására
    int osciSamples[AudioProcessorConstants::MAX_INTERNAL_WIDTH];  // MAX_INTERNAL_WIDTH maradhat itt, ha az oszcilloszkóp buffer mérete fix

    float& activeFftGainConfigRef;  // Referencia a Config_t gain mezőjére
    int audioInputPin;

    double targetSamplingFrequency_;  // Cél mintavételezési frekvencia
    uint32_t sampleIntervalMicros_;   // Egy mintára jutó időköz mikroszekundumban
    float binWidthHz_;                // Cél frekvencia alapján számolt bin szélesség
    float smoothed_auto_gain_factor_; // Simított erősítési faktor az auto gain-hez
};

#endif  // AUDIO_PROCESSOR_H
