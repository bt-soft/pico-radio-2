#ifndef CWDECODER_H
#define CWDECODER_H

#include <Arduino.h>  // millis(), analogRead(), stb.

#include <cmath>  // round, sin, cos, sqrt, abs, min, max - szükséges a constexpr számításokhoz

#include "defines.h"  // AUDIO_INPUT_PIN, DEBUG

class CwDecoder {
   public:
    CwDecoder(int audioPin);
    ~CwDecoder();

    void updateDecoder();           // Core1 hívja ciklikusan a CW dekódoláshoz
    char getCharacterFromBuffer();  // Core0 kéri le a dekódolt karaktert a pufferből
    void resetDecoderState();       // Hívandó a CW módra váltáskor az állapot visszaállításáhozprivate:

    // CW vételi hangfrekvencia
    static constexpr float TARGET_FREQ = CW_SHIFT_FREQUENCY;

    // Goertzel szűrő paraméterek TARGET_FREQ-hez (automatikus számítás)
    static constexpr float SAMPLING_FREQ = 8400.0f;
    static constexpr short N_SAMPLES = 45;

    static inline short K_CONSTANT() { return static_cast<short>((N_SAMPLES * TARGET_FREQ / SAMPLING_FREQ) + 0.5f); }

    static inline float OMEGA() { return (2.0f * M_PI * static_cast<float>(K_CONSTANT())) / static_cast<float>(N_SAMPLES); }

    static inline float COEFF() { return 2.0f * cos(OMEGA()); }

    static const unsigned long SAMPLING_PERIOD_US;  // Definiálva a .cpp fájlban

    float q0, q1, q2;
    short testData[N_SAMPLES];  // Beállított méret

    // Morse időzítés és állapot
    static constexpr float THRESHOLD = 250.0f;  // Beállított küszöb, kísérletezzen ezzel az értékkel (pl. 200-400)
    static constexpr unsigned long MIN_MORSE_ELEMENT_DURATION_MS =
        25;  // Minimum időtartam egy érvényes Morse elemhez (ms) - Ezt lehet, hogy a DOT_MIN_MS-re kellene cserélni vagy összehangolni
    // Szünetek időzítéséhez konstansok
    static constexpr float CHAR_GAP_DOT_MULTIPLIER = 3.0f;
    static constexpr float WORD_GAP_DOT_MULTIPLIER = 6.5f;  // Kb. 7 dit, de a char gap-nél biztosan nagyobb
    static constexpr unsigned long MIN_CHAR_GAP_MS_FALLBACK = 180;
    static constexpr unsigned long MIN_WORD_GAP_MS_FALLBACK = 400;
    static constexpr unsigned long DOT_MIN_MS = 5;                // Minimum pont hossz (ms) - 50+ WPM támogatásához
    static constexpr unsigned long DOT_MAX_MS = 250;              // Maximum pont hossz (ms) - 7 WPM támogatásához
    static constexpr unsigned long DASH_MAX_MS = DOT_MAX_MS * 4;  // Maximum vonás hossz (ms) - reálisabb arány

    // Dinamikus zaj szűrés - aggresívebb beállítások 25 WPM-hez
    static constexpr unsigned long NOISE_THRESHOLD_FACTOR = 5;  // Zaj küszöb szorzó: min_duration / 5 (toleránsabb)
    static constexpr unsigned long MIN_ADAPTIVE_DOT_MS = 15;    // Adaptív minimum - 15ms (~40 WPM alsó határ)

    short noiseBlankerLoops_;  // Megerősítések száma hang/nincs hang állapothoz

    unsigned long startReferenceMs_;
    unsigned long currentReferenceMs_;
    unsigned long leadingEdgeTimeMs_;
    unsigned long trailingEdgeTimeMs_;
    unsigned long rawToneDurations_[6];  // Morse karakterek maximum 6 elem hosszúak
    short toneIndex_;
    unsigned long toneMaxDurationMs_;
    unsigned long toneMinDurationMs_;

    unsigned long lastActivityMs_;  // Korábban static volt a decodeNextCharacter-ben
    unsigned long currentLetterStartTimeMs_;
    short symbolCountForWpm_;

    bool decoderStarted_;     // Igaz, ha egy karakter első felfutó éle észlelésre került
    bool measuringTone_;      // Igaz, ha éppen hangot mér (felfutó és lefutó él között)
    bool toneDetectedState_;  // Igaz, ha a Goertzel szűrő kimenete a küszöb felett van

    bool inInactiveState;      // Ha inaktív állapotban vagyunk    // Szóköz dekódoláshoz
    char lastDecodedChar_;     // Utoljára dekódolt karakter
    bool wordSpaceProcessed_;  // Jelzi, ha egy adott csendperiódusért már adtunk szóközt

    // Debug kimenet optimalizálásához
    unsigned long lastSpaceDebugMs_;                                // Utolsó "Szóköz ellenőrzés" debug üzenet időpontja
    static constexpr unsigned long SPACE_DEBUG_INTERVAL_MS = 1000;  // Debug üzenetek közötti minimum idő (ms)

    // Karakter puffer a folyamatos dekódoláshoz
    static constexpr short DECODED_CHAR_BUFFER_SIZE = 3;
    char decodedCharBuffer_[DECODED_CHAR_BUFFER_SIZE];
    short charBufferReadPos_;
    short charBufferWritePos_;
    short charBufferCount_;  // Morse fa
    static const char MORSE_TREE_SYMBOLS[];
    static const short MORSE_TREE_ROOT_INDEX = 63;
    static const short MORSE_TREE_INITIAL_OFFSET = 32;
    static const short MORSE_TREE_MAX_DEPTH = 6;

    short treeIndex_;
    short treeOffset_;
    short treeCount_;

    // Audio bemenet
    int audioInputPin_;

    // Privát metódusok
    bool goertzelProcess();
    bool sampleWithNoiseBlanking();
    void processDot();
    void processDash();
    char getCharFromTree();
    void resetMorseTree();
    void updateReferenceTimings(unsigned long duration);
    void initialize();                // Közös inicializálási logika
    char processCollectedElements();  // Összegyűjtött Morse elemek feldolgozása
    void addToBuffer(char c);         // Karakter hozzáadása a pufferhez

    // WPM sebesség becslés az aktuális pont hossz alapján
    uint8_t estimateWpm() const {
        if (toneMinDurationMs_ == 9999L || toneMinDurationMs_ == 0) {
            return 15;  // Alapértelmezett érték
        }
        // WPM = 1200 / (pont hossz ms-ban)
        int wpm = 1200 / toneMinDurationMs_;
        return constrain(wpm, 5, 30);  // Biztonságos tartomány
    }
};

#endif  // CWDECODER_H
