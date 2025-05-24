#ifndef CWDECODER_H
#define CWDECODER_H

#include <Arduino.h>  // millis(), analogRead(), etc.

#include "defines.h"  // AUDIO_INPUT_PIN, DEBUG

class CwDecoder {
   public:
    CwDecoder(int audioPin);
    ~CwDecoder();

    char decodeNextCharacter();
    void resetDecoderState();  // To be called when switching to CW mode

   private:
    // Goertzel filter parameters
    // Sampling_freq = 8912.0; N_SAMPLES = 48; Target_freq = 930.0;
    // K_CONSTANT = round(48 * 930 / 8912) = round(5.0089) = 5
    static const float TARGET_FREQ;
    static const float SAMPLING_FREQ;
    static const short N_SAMPLES;
    static const short K_CONSTANT;
    static const float OMEGA;
    static const float SINE_OMEGA;
    static const float COS_OMEGA;
    static const float COEFF;
    static const unsigned long SAMPLING_PERIOD_US;  // 1_000_000 / SAMPLING_FREQ

    float q0, q1, q2;
    short testData[48];  // N_SAMPLES, casted from analogRead

    // Morse timing and state
    static const float THRESHOLD;
    short noiseBlankerLoops_;  // Number of confirmations for tone/no-tone

    unsigned long startReferenceMs_;
    unsigned long currentReferenceMs_;
    unsigned long leadingEdgeTimeMs_;
    unsigned long trailingEdgeTimeMs_;
    unsigned long toneDurationHistory_[6];
    short toneIndex_;
    unsigned long toneMaxDurationMs_;
    unsigned long toneMinDurationMs_;

    unsigned long currentLetterStartTimeMs_;
    short symbolCountForWpm_;
    // short wpm_; // Not directly used for output char

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
