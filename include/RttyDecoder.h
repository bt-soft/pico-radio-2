#ifndef RTTYDECODER_H
#define RTTYDECODER_H

#include <Arduino.h>

#include <cmath>

#include "defines.h"

class RttyDecoder {
   public:
    RttyDecoder(int audioPin);
    ~RttyDecoder();
    void updateDecoder();           // Core1 hívja ciklikusan az RTTY dekódoláshoz
    char getCharacterFromBuffer();  // Core0 kéri le a dekódolt karaktert a pufferből
    void resetDecoderState();       // Hívandó az RTTY módra váltáskor az állapot visszaállításához   private:
    // Goertzel szűrő paraméterek Mark/Space frekvenciákhoz
    static constexpr float SAMPLING_FREQ = 8400.0f;
    static constexpr short N_SAMPLES = 84;  // 10ms ablak

    // Goertzel konstansok (a .cpp fájlban lesznek kiszámítva)
    static const short K_MARK;
    static const float COEFF_MARK;
    static const short K_SPACE;
    static const float COEFF_SPACE;

    static const unsigned long SAMPLING_PERIOD_US;

    // Goertzel szűrő változók Mark-hoz
    float q0_mark, q1_mark, q2_mark;
    // Goertzel szűrő változók Space-hez
    float q0_space, q1_space, q2_space;

    short testData[N_SAMPLES];

    // RTTY detektálási paraméterek
    static constexpr float SIGNAL_THRESHOLD = 100.0f;          // Minimális jelerősség küszöb
    static constexpr float MARK_SPACE_RATIO_THRESHOLD = 1.5f;  // Mark/Space vagy Space/Mark arány küszöb

    // RTTY időzítés és állapot
    static constexpr unsigned long MIN_BAUD_RATE = 45;                          // 45 baud (22ms/bit)
    static constexpr unsigned long MAX_BAUD_RATE = 100;                         // 100 baud (10ms/bit)
    static constexpr unsigned long MIN_BIT_DURATION_MS = 1000 / MAX_BAUD_RATE;  // 10ms
    static constexpr unsigned long MAX_BIT_DURATION_MS = 1000 / MIN_BAUD_RATE;  // 22ms
    static constexpr unsigned long DEFAULT_BIT_DURATION_MS = 22;                // 45.45 baud alapértelmezett

    // Timing toleranciák
    static constexpr unsigned long BIT_TIMING_TOLERANCE_MS = 3;        // ±3ms tolerancia bit időzítéshez
    static constexpr unsigned long START_BIT_CONFIRM_DELAY_RATIO = 3;  // Start bit megerősítés: bit_duration / 3

    // Shift frekvencia detektálás
    static constexpr float TARGET_SHIFT_850HZ = 850.0f;  // 850Hz shift
    static constexpr float TARGET_SHIFT_170HZ = 170.0f;  // 170Hz shift (defines.h-ban megadott)
    static constexpr float SHIFT_TOLERANCE = 25.0f;      // ±25Hz tolerancia

    // RTTY állapotgép
    enum RttyState { IDLE, WAITING_FOR_START_BIT, RECEIVING_DATA_BITS, RECEIVING_STOP_BIT };

    RttyState currentState_;
    unsigned long bitDurationMs_;
    unsigned long lastBitTimeMs_;
    unsigned long startBitTimeMs_;
    short bitsReceived_;
    uint8_t currentByte_;
    bool currentToneState_;  // true = Mark, false = Space
    bool lastToneState_;

    // Automatikus detektálás
    bool autoDetectActive_;
    unsigned long autoDetectStartMs_;
    static constexpr unsigned long AUTO_DETECT_TIMEOUT_MS = 10000;  // 10 sec
    float detectedMarkFreq_;
    float detectedSpaceFreq_;
    float detectedShiftFreq_;
    unsigned long detectedBaudRate_;

    // Baudrate detektálás
    unsigned long transitionTimes_[10];  // Utolsó 10 állapotváltás időpontja
    short transitionIndex_;
    unsigned long lastTransitionMs_;

    // Karakter puffer
    static constexpr short DECODED_CHAR_BUFFER_SIZE = 5;
    char decodedCharBuffer_[DECODED_CHAR_BUFFER_SIZE];
    short charBufferReadPos_;
    short charBufferWritePos_;
    short charBufferCount_;

    // Baudot kód tábla
    static const char BAUDOT_LTRS_TABLE[32];
    static const char BAUDOT_FIGS_TABLE[32];
    bool figsShift_;  // true = FIGS mód, false = LTRS mód    // Audio bemenet
    int audioInputPin_;

    // Privát metódusok
    void initialize();
    bool processGoertzelFilters(bool& isMarkStronger);
    bool detectToneState();
    void updateBaudRateDetection();
    bool detectShiftFrequency();
    char decodeBaudotCharacter(uint8_t baudotCode);
    void addToBuffer(char c);
    void resetRttyStateMachine();
    unsigned long estimateBaudRate();
};

#endif  // RTTYDECODER_H
