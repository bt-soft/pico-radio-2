#include "AudioProcessor.h"

#include <cmath>  // std::abs, std::round

#include "defines.h"  // DEBUG makróhoz, ha szükséges

AudioProcessor::AudioProcessor(float& gainConfigRef, int audioPin, double targetSamplingFrequency)
    : FFT(),
      activeFftGainConfigRef(gainConfigRef),
      audioInputPin(audioPin),
      targetSamplingFrequency_(targetSamplingFrequency),
      binWidthHz_(0.0f),
      smoothed_auto_gain_factor_(1.0f)  // Simított erősítési faktor inicializálása
{

    if (targetSamplingFrequency_ > 0) {
        sampleIntervalMicros_ = static_cast<uint32_t>(1000000.0 / targetSamplingFrequency_);
        binWidthHz_ = static_cast<float>(targetSamplingFrequency_) / AudioProcessorConstants::FFT_SAMPLES;
    } else {
        sampleIntervalMicros_ = 25;  // Fallback to 40kHz
        binWidthHz_ = (1000000.0f / sampleIntervalMicros_) / AudioProcessorConstants::FFT_SAMPLES;
        DEBUG("AudioProcessor: Figyelmeztetés - targetSamplingFrequency nulla, tartalék használata.");
    }
    DEBUG("AudioProcessor: Target Fs: %.1f Hz, Sample Interval: %lu us, Bin Width: %.2f Hz\n", targetSamplingFrequency_, sampleIntervalMicros_, binWidthHz_);

    // Oszcilloszkóp minták inicializálása középpontra (ADC nyers érték)
    for (int i = 0; i < AudioProcessorConstants::MAX_INTERNAL_WIDTH; ++i) {
        osciSamples[i] = 2048;
    }
    // Pufferek nullázása
    memset(vReal, 0, sizeof(vReal));
    memset(vImag, 0, sizeof(vImag));
    memset(RvReal, 0, sizeof(RvReal));
}

AudioProcessor::~AudioProcessor() {
    // Dinamikusan allokált erőforrások itt szabadíthatók fel, ha lennének.
}

void AudioProcessor::process(bool collectOsciSamples) {
    int osci_sample_idx = 0;
    uint32_t loopStartTimeMicros;
    double max_abs_sample_for_auto_gain = 0.0;

    // Ha az FFT ki van kapcsolva (-1.0f), akkor töröljük a puffereket és visszatérünk
    if (activeFftGainConfigRef == -1.0f) {
        memset(RvReal, 0, sizeof(RvReal));  // Magnitúdó buffer törlése
        if (collectOsciSamples) {
            for (int i = 0; i < AudioProcessorConstants::MAX_INTERNAL_WIDTH; ++i) osciSamples[i] = 2048;  // Oszcilloszkóp buffer reset
        }
        return;
    }

    // 1. Mintavételezés és középre igazítás, opcionális oszcilloszkóp mintagyűjtés
    // A teljes mintavételezési ciklus idejét is mérhetnénk, de az egyes minták időzítése fontosabb.
    for (int i = 0; i < AudioProcessorConstants::FFT_SAMPLES; i++) {
        loopStartTimeMicros = micros();
        uint32_t sum = 0;
        for (int j = 0; j < 4; j++) {  // 4 minta átlagolása
            sum += analogRead(audioInputPin);
        }
        double averaged_sample = sum / 4.0;

        if (collectOsciSamples) {
            if (i % AudioProcessorConstants::OSCI_SAMPLE_DECIMATION_FACTOR == 0 && osci_sample_idx < AudioProcessorConstants::MAX_INTERNAL_WIDTH) {
                if (osci_sample_idx < sizeof(osciSamples) / sizeof(osciSamples[0])) {  // Biztonsági ellenőrzés
                    osciSamples[osci_sample_idx] = static_cast<int>(averaged_sample);
                    osci_sample_idx++;
                }
            }
        }
        vReal[i] = averaged_sample - 2048.0;  // Középre igazítás (feltételezve, hogy 2048 a nulla szint)
        vImag[i] = 0.0;

        if (activeFftGainConfigRef == 0.0f) {  // Auto Gain mód
            if (std::abs(vReal[i]) > max_abs_sample_for_auto_gain) {
                max_abs_sample_for_auto_gain = std::abs(vReal[i]);
            }
        }

        // Időzítés a cél mintavételezési frekvencia eléréséhez
        uint32_t processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < sampleIntervalMicros_) {
            delayMicroseconds(sampleIntervalMicros_ - processingTimeMicros);
        }
    }

    // 2. Erősítés alkalmazása (manuális vagy automatikus)
    if (activeFftGainConfigRef > 0.0f) {  // Manual Gain
        for (int i = 0; i < AudioProcessorConstants::FFT_SAMPLES; i++) {
            vReal[i] *= activeFftGainConfigRef;
        }
    } else if (activeFftGainConfigRef == 0.0f) {  // Auto Gain
        float target_auto_gain_factor = 1.0f;     // Alapértelmezett erősítés, ha nincs jel

        if (max_abs_sample_for_auto_gain > 0.001) {  // Nullával osztás és extrém erősítés elkerülése
            target_auto_gain_factor = AudioProcessorConstants::FFT_AUTO_GAIN_TARGET_PEAK / max_abs_sample_for_auto_gain;
            target_auto_gain_factor = constrain(target_auto_gain_factor, AudioProcessorConstants::FFT_AUTO_GAIN_MIN_FACTOR, AudioProcessorConstants::FFT_AUTO_GAIN_MAX_FACTOR);
        }

        // Az erősítési faktor simítása
        if (target_auto_gain_factor < smoothed_auto_gain_factor_) {
            // A jel hangosabb lett, vagy a cél erősítés alacsonyabb -> gyorsabb "attack"
            smoothed_auto_gain_factor_ += AudioProcessorConstants::AUTO_GAIN_ATTACK_COEFF * (target_auto_gain_factor - smoothed_auto_gain_factor_);
        } else {
            // A jel halkabb lett, vagy a cél erősítés magasabb -> lassabb "release"
            smoothed_auto_gain_factor_ += AudioProcessorConstants::AUTO_GAIN_RELEASE_COEFF * (target_auto_gain_factor - smoothed_auto_gain_factor_);
        }
        // Biztosítjuk, hogy a simított faktor is a határokon belül maradjon
        smoothed_auto_gain_factor_ = constrain(smoothed_auto_gain_factor_, AudioProcessorConstants::FFT_AUTO_GAIN_MIN_FACTOR, AudioProcessorConstants::FFT_AUTO_GAIN_MAX_FACTOR);

        for (int i = 0; i < AudioProcessorConstants::FFT_SAMPLES; i++) {
            vReal[i] *= smoothed_auto_gain_factor_;
        }

        // #ifdef __DEBUG
        //         static unsigned long lastGainPrintTime = 0;
        //         if (millis() - lastGainPrintTime >= 1000) {
        //             if (activeFftGainConfigRef == 0.0f) { // Csak auto gain módban írjuk ki
        //                 DEBUG("AudioProcessor: Smoothed Auto Gain Factor: %.2f\n", smoothed_auto_gain_factor_);
        //             }
        //             lastGainPrintTime = millis();
        //         }
        // #endif
    }
    // 3. Ablakozás, FFT számítás, magnitúdó
    FFT.windowing(vReal, AudioProcessorConstants::FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, AudioProcessorConstants::FFT_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, AudioProcessorConstants::FFT_SAMPLES);  // Az eredmény a vReal-be kerül

    // Magnitúdók átmásolása az RvReal tömbbe
    for (int i = 0; i < AudioProcessorConstants::FFT_SAMPLES; ++i) {
        RvReal[i] = vReal[i];
    }

    // 4. Alacsony frekvenciák csillapítása az RvReal tömbben
    // A binWidthHz_ már tagváltozóként rendelkezésre áll
    const int attenuation_cutoff_bin = static_cast<int>(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / binWidthHz_);

    for (int i = 0; i < (AudioProcessorConstants::FFT_SAMPLES / 2); ++i) {  // Csak a releváns (nem tükrözött) frekvencia bin-eken iterálunk
        if (i < attenuation_cutoff_bin) {
            RvReal[i] /= AudioProcessorConstants::LOW_FREQ_ATTENUATION_FACTOR;
        }
    }
}
