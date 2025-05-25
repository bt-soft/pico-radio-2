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
    // Goertzel szűrő paraméterek 750Hz-hez
    static constexpr float TARGET_FREQ = CW_SHIFT_FREQUENCY;
    static constexpr float SAMPLING_FREQ = 8400.0f;
    static constexpr short N_SAMPLES = 45;  // Beállítva a jobb 750Hz hangoláshoz 8400Hz mintavételezéssel
    // K_CONSTANT = round(45 * 750 / 8400) = round(4.0178) = 4
    // Tényleges szűrő középfrekvencia: (4 / 45) * 8400 = 746.67 Hz
    static constexpr short K_CONSTANT = 4;  // Előre kiszámított: round(45 * 750 / 8400)
    static constexpr float OMEGA =
        (2.0f * M_PI * static_cast<float>(K_CONSTANT)) / static_cast<float>(N_SAMPLES);  // COEFF értéket előre kiszámoljuk az OMEGA alapján: 2.0 * cos(0.5585) = 1.7015
    static constexpr float COEFF = 1.7015f;                                              // Előre kiszámított érték: 2.0f * cos(OMEGA)
    static const unsigned long SAMPLING_PERIOD_US;                                       // Definiálva a .cpp fájlban

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
    static constexpr unsigned long DOT_MIN_MS = 25;               // Minimum pont hossz (ms) - csökkentve a rövid jelek fogadásához
    static constexpr unsigned long DOT_MAX_MS = 200;              // Maximum pont hossz (ms)
    static constexpr unsigned long DASH_MAX_MS = DOT_MAX_MS * 7;  // Maximum vonás hossz (ms)

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

    bool inInactiveState;  // Ha inaktív állapotban vagyunk

    // Szóköz dekódoláshoz
    char lastDecodedChar_;     // Utoljára dekódolt karakter
    bool wordSpaceProcessed_;  // Jelzi, ha egy adott csendperiódusért már adtunk szóközt

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
};

#endif  // CWDECODER_H
