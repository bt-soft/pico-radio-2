#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <ArduinoFFT.h>

namespace AudioProcessorConstants {
// A 40kHz-es SAMPLING_FREQUENCY miatt az FFT eljárás 0 Hz-től 20 kHz-ig terjedő sávszélességben képes mérni a frekvenciakomponenseket.
constexpr double SAMPLING_FREQUENCY = 40000.0;                                    // Mintavételezési frekvencia Hz-ben (a tényleges mért sebesség alapján ~40kHz)
constexpr uint16_t FFT_SAMPLES = 256;                                             // Minták száma az FFT-hez (2 hatványának kell lennie)
const float BIN_WIDTH_HZ = static_cast<float>(SAMPLING_FREQUENCY) / FFT_SAMPLES;  // Bin szélessége Hz-ben

constexpr float AMPLITUDE_SCALE = 40.0f;                     // Skálázási faktor az FFT eredményekhez (tovább csökkentve az érzékenység növeléséhez)
constexpr float LOW_FREQ_ATTENUATION_THRESHOLD_HZ = 200.0f;  // Ez alatti frekvenciákat csillapítjuk
constexpr float LOW_FREQ_ATTENUATION_FACTOR = 10.0f;         // Ezzel a faktorral osztjuk az alacsony frekvenciák magnitúdóját

// Konstansok az Auto Gain módhoz
constexpr float FFT_AUTO_GAIN_TARGET_PEAK = 1500.0f;  // Cél csúcsérték az Auto Gain módhoz (a +/-2047 tartományból)
constexpr float FFT_AUTO_GAIN_MIN_FACTOR = 0.1f;      // Minimális erősítési faktor Auto módban
constexpr float FFT_AUTO_GAIN_MAX_FACTOR = 10.0f;     // Maximális erősítési faktor Auto módban

constexpr int MAX_INTERNAL_WIDTH = 86;  // Oszcilloszkóp és magas felbontású spektrum belső bufferéhez

constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;  // Oszcilloszkóp mintavételi decimációs faktora

};  // namespace AudioProcessorConstants

class AudioProcessor {
   public:
    AudioProcessor(float& gainConfigRef, int audioPin);
    ~AudioProcessor();

    void process(bool collectOsciSamples);

    const double* getMagnitudeData() const { return RvReal; }

    const int* getOscilloscopeData() const { return osciSamples; }

    // Opcionális: Getterek a belső tömbök méretéhez, ha szükséges
    // int getMagnitudeDataSize() const { return MiniAudioFftConstants::FFT_SAMPLES / 2; }
    // int getOscilloscopeDataSize() const { return MiniAudioFftConstants::MAX_INTERNAL_WIDTH; }

   private:
    ArduinoFFT<double> FFT;
    double vReal[AudioProcessorConstants::FFT_SAMPLES];
    double vImag[AudioProcessorConstants::FFT_SAMPLES];
    double RvReal[AudioProcessorConstants::FFT_SAMPLES];  // Magnitúdók tárolására
    int osciSamples[AudioProcessorConstants::MAX_INTERNAL_WIDTH];

    float& activeFftGainConfigRef;  // Referencia a Config_t gain mezőjére
    int audioInputPin;
};

#endif  // AUDIO_PROCESSOR_H
