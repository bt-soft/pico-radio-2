#ifndef RTTYDECODER_H
#define RTTYDECODER_H
#include <vector>  // Szükséges a std::vector-hoz

#include "AudioProcessor.h"  // Szükséges az audio feldolgozáshoz
#include "defines.h"         // DEBUG eléréséhez

class RttyDecoder {
   public:
    RttyDecoder(AudioProcessor& audioProcessor);
    ~RttyDecoder();

    char decodeNextCharacter();  // Dekódolja a következő karaktert az audio bemenetből

    void setFigsShift(bool shift) { figsShift_ = shift; }

    bool getFigsShift() const { return figsShift_; }

    void startAutoDetect();  // Automatikus frekvencia detektálás indítása
    void setMarkFrequencyIsLower(bool isLower); // Új metódus

   private:
    AudioProcessor& audioProcessor;
    bool figsShift_ = false;  // Baudot LTRS/FIGS shift állapota

    // RTTY paraméterek (kezdetben fixen)
    static constexpr float RTTY_BAUD_RATE = 45.45f;        // Standard Baud rate
    static constexpr float RTTY_TARGET_SHIFT_HZ = 450.0f;  // Cél RTTY Shift az auto-detektáláshoz (a jelzett 945/1395 Hz-hez)
    // A Mark és Space frekvenciák most már tagváltozók lesznek
    float rttyMarkFreqHz_;   // Aktuális Mark frekvencia
    float rttySpaceFreqHz_;  // Aktuális Space frekvencia
    bool markFrequencyIsLower_ = false; // Alapértelmezetten a Mark a magasabb (standard)

    // Automatikus frekvencia detektáláshoz
    bool autoDetectModeActive_ = false;
    unsigned long autoDetectStartTime_ = 0;
    static constexpr unsigned long AUTO_DETECT_DURATION_MS = 5000;   // Meddig próbálkozzon (ms)
    static constexpr float AUTO_DETECT_SHIFT_TOLERANCE_HZ = 50.0f;   // Tolerancia a shift detektálásához
    static constexpr float MIN_RTTY_FREQ_HZ_FOR_DETECT = 800.0f;     // Minimum frekvencia a detektáláshoz (945 Hz miatt csökkentve)
    static constexpr float MAX_RTTY_FREQ_HZ_FOR_DETECT = 3000.0f;    // Maximum frekvencia a detektáláshoz
    static constexpr float AUTO_DETECT_MIN_PEAK_MAGNITUDE = 150.0f;  // Minimális magnitúdó egy csúcshoz
    static constexpr float AUTO_DETECT_MAX_MAG_RATIO = 5.0f;        // Maximális magnitúdó arány Mark/Space között az auto-detektáláshoz (csökkentve 15.0f-ről)
    // További kísérleti csökkentés
    static constexpr float MIN_TONE_MAGNITUDE_THRESHOLD = 15.0f; // Ez az érték megfelelőnek tűnik
    static constexpr float TONE_DIFFERENCE_THRESHOLD = 0.1f;     // Előző: 1.0f. Nagyon kis értékre csökkentve.
    static constexpr int NUM_BINS_TO_AVERAGE = 5;                 // Hány FFT bin-t átlagoljunk a Mark/Space detektálásakor (3-ról növelve)

    // RTTY dekódolási állapotgép változói
    enum RttyState { IDLE, WAITING_FOR_START_BIT, RECEIVING_DATA_BIT, RECEIVING_STOP_BIT };

    RttyState currentState = IDLE;
    unsigned long lastBitTime = 0;
    int bitsReceived = 0;
    uint8_t currentByte = 0;

    // Baudot -> ASCII lookup tábla (egyszerűsített)
    char baudotToAscii[2][32];  // [0] = LTRS, [1] = FIGS

    void initBaudotToAscii();
    char decodeBaudot(uint8_t byte, bool& figsShift);
    bool attemptAutoDetectFrequencies();  // Új metódus

    void getSignalState(bool& outIsSignalPresent, bool& outIsMarkTone);  // Deklaráció hozzáadása

    // Segédstruktúra a csúcsok tárolásához
    struct PeakInfo {
        float freq;
        double mag;
    };

    friend class AmDisplay;  // Hozzáférés a tagváltozókhoz
};

#endif