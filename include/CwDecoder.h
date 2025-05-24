#ifndef CWDECODER_H
#define CWDECODER_H

#include <Arduino.h>  // millis(), analogRead(), etc.

#include <cmath>  // round, sin, cos, sqrt, abs, min, max - szükséges a constexpr számításokhoz

#include "defines.h"  // AUDIO_INPUT_PIN, DEBUG

class CwDecoder {
   public:
    CwDecoder(int audioPin);
    ~CwDecoder();

    char decodeNextCharacter();
    void resetDecoderState();  // To be called when switching to CW mode   private:
    // Goertzel filter parameters for 750Hz
    static constexpr float TARGET_FREQ = 750.0f;
    static constexpr float SAMPLING_FREQ = 8400.0f;
    static constexpr short N_SAMPLES = 45;  // Adjusted for better 750Hz tuning with 8400Hz Fs
    // K_CONSTANT = round(45 * 750 / 8400) = round(4.0178) = 4
    // Actual filter center frequency: (4 / 45) * 8400 = 746.67 Hz
    static constexpr short K_CONSTANT = 4;  // Pre-calculated: round(45 * 750 / 8400)
    static constexpr float OMEGA = (2.0f * M_PI * K_CONSTANT) / N_SAMPLES;
    static constexpr float SINE_OMEGA = 0.5591929f;  // Pre-calculated: sin(OMEGA)
    static constexpr float COS_OMEGA = 0.8290376f;   // Pre-calculated: cos(OMEGA)
    static constexpr float COEFF = 1.6580751f;       // Pre-calculated: 2.0 * COS_OMEGA
    static const unsigned long SAMPLING_PERIOD_US;   // Defined in .cpp

    float q0, q1, q2;
    short testData[N_SAMPLES];  // Adjusted size

    // Morse timing and state
    static constexpr float THRESHOLD = 250.0f;                          // Threshold for Goertzel magnitude
    static constexpr unsigned long MIN_MORSE_ELEMENT_DURATION_MS = 25;  // Minimum duration for a valid Morse element (ms)
    short noiseBlankerLoops_;                                           // Number of confirmations for tone/no-tone

    unsigned long startReferenceMs_;
    unsigned long currentReferenceMs_;
    unsigned long leadingEdgeTimeMs_;
    unsigned long trailingEdgeTimeMs_;
    unsigned long rawToneDurations_[6];
    short toneIndex_;
    unsigned long toneMaxDurationMs_;
    unsigned long toneMinDurationMs_;

    unsigned long currentLetterStartTimeMs_;
    short symbolCountForWpm_;

    bool decoderStarted_;     // True if the first leading edge of a character has been detected
    bool measuringTone_;      // True if currently measuring a tone (between leading and trailing edge)
    bool toneDetectedState_;  // True if Goertzel filter output is above threshold

    // Morse tree
    static const char MORSE_TREE_SYMBOLS[];
    static const short MORSE_TREE_ROOT_INDEX = 63;
    static const short MORSE_TREE_INITIAL_OFFSET = 32;
    static const short MORSE_TREE_MAX_DEPTH = 6;

    short treeIndex_;
    short treeOffset_;
    short treeCount_;

    // Audio input
    int audioInputPin_;

    // Private methods
    bool goertzelProcess();
    bool sampleWithNoiseBlanking();
    void processDot();
    void processDash();
    char getCharFromTree();
    void resetMorseTree();
    void updateReferenceTimings(unsigned long duration);
    void initialize();  // Common initialization logic
};

#endif  // CWDECODER_H
