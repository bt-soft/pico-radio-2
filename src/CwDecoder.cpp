#include "CwDecoder.h"

#include <cmath>  // round, sin, cos, sqrt, abs, min, max

#include "defines.h"  // DEBUG

// Goertzel filter parameters
const float CwDecoder::TARGET_FREQ = 930.0f;
const float CwDecoder::SAMPLING_FREQ = 8912.0f;  // Derived from 48 samples in 5386 us
const short CwDecoder::N_SAMPLES = 48;
const short CwDecoder::K_CONSTANT = static_cast<short>(round(N_SAMPLES * TARGET_FREQ / SAMPLING_FREQ));  // Should be 5
const float CwDecoder::OMEGA = (2.0 * PI * K_CONSTANT) / N_SAMPLES;
const float CwDecoder::SINE_OMEGA = sin(OMEGA);
const float CwDecoder::COS_OMEGA = cos(OMEGA);
const float CwDecoder::COEFF = 2.0 * COS_OMEGA;
const unsigned long CwDecoder::SAMPLING_PERIOD_US = static_cast<unsigned long>(1000000.0f / SAMPLING_FREQ);  // Approx 112 us

// Morse timing and state
const float CwDecoder::THRESHOLD = 100.0f;  // Threshold for Goertzel magnitude

// Morse tree
const char CwDecoder::MORSE_TREE_SYMBOLS[] = {
    ' ', '5', ' ', 'H', ' ',  '4', ' ', 'S',  // 0
    ' ', ' ', ' ', 'V', ' ',  '3', ' ', 'I',  // 8
    ' ', ' ', ' ', 'F', ' ',  ' ', ' ', 'U',  // 16
    '?', ' ', '_', ' ', ' ',  '2', ' ', 'E',  // 24
    ' ', '&', ' ', 'L', '"',  ' ', ' ', 'R',  // 32
    ' ', '+', '.', ' ', ' ',  ' ', ' ', 'A',  // 40
    ' ', ' ', ' ', 'P', '@',  ' ', ' ', 'W',  // 48
    ' ', ' ', ' ', 'J', '\'', '1', ' ', ' ',  // 56
    ' ', '6', '-', 'B', ' ',  '=', ' ', 'D',  // 64
    ' ', '/', ' ', 'X', ' ',  ' ', ' ', 'N',  // 72
    ' ', ' ', ' ', 'C', ';',  ' ', '!', 'K',  // 80
    ' ', '(', ')', 'Y', ' ',  ' ', ' ', 'T',  // 88
    ' ', '7', ' ', 'Z', ' ',  ' ', ',', 'G',  // 96
    ' ', ' ', ' ', 'Q', ' ',  ' ', ' ', 'M',  // 104
    ':', '8', ' ', ' ', ' ',  ' ', ' ', 'O',  // 112
    ' ', '9', ' ', ' ', ' ',  '0', ' ', ' '   // 120
};

CwDecoder::CwDecoder(int audioPin) : audioInputPin_(audioPin) {
    initialize();
    // The Blackman window calculation from .ino's setup() is omitted
    // as it was commented out in the goertzel() function of the .ino.
    // If needed, it would be:
    // for (short i = 0; i < N_SAMPLES; i++) {
    //   blackmanWindow[i] = (0.426591 - 0.496561 * cos((2.0 * PI * i) / N_SAMPLES) + 0.076848 * cos((4.0 * PI * i) / N_SAMPLES));
    // }
}

CwDecoder::~CwDecoder() {}

void CwDecoder::initialize() {
    noiseBlankerLoops_ = 1;   // Default from .ino
    startReferenceMs_ = 200;  // Default from .ino
    currentReferenceMs_ = startReferenceMs_;
    leadingEdgeTimeMs_ = 0;
    trailingEdgeTimeMs_ = 0;
    toneIndex_ = 0;
    toneMaxDurationMs_ = 0;
    toneMinDurationMs_ = 9999;
    currentLetterStartTimeMs_ = 0;
    symbolCountForWpm_ = 0;
    decoderStarted_ = false;
    measuringTone_ = false;
    toneDetectedState_ = false;
    resetMorseTree();
    q0 = 0;
    q1 = 0;
    q2 = 0;
    memset(testData, 0, sizeof(testData));
    memset(toneDurationHistory_, 0, sizeof(toneDurationHistory_));
}

void CwDecoder::resetDecoderState() {
    initialize();
    DEBUG("CW Decoder state reset.\n");
}

bool CwDecoder::goertzelProcess() {
    unsigned long loopStartTimeMicros;
    // Sample the input waveform
    for (short index = 0; index < N_SAMPLES; index++) {
        loopStartTimeMicros = micros();
        testData[index] = analogRead(audioInputPin_);
        // Apply Blackman window if it were enabled
        // testData[index] *= blackmanWindow[index];
        unsigned long processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < SAMPLING_PERIOD_US) {
            delayMicroseconds(SAMPLING_PERIOD_US - processingTimeMicros);
        }
    }

    // Goertzel algorithm
    q1 = 0;
    q2 = 0;
    for (short index = 0; index < N_SAMPLES; index++) {
        q0 = COEFF * q1 - q2 + (float)testData[index];
        q2 = q1;
        q1 = q0;
    }
    float magnitudeSquared = (q1 * q1) + (q2 * q2) - q1 * q2 * COEFF;
    float magnitude = sqrt(magnitudeSquared);

    // Apply threshold
    if (magnitude > THRESHOLD) {
        toneDetectedState_ = true;
        return true;
    } else {
        toneDetectedState_ = false;
        return false;
    }
}

bool CwDecoder::sampleWithNoiseBlanking() {
    int no_tone_count = 0;
    int tone_count = 0;

    while (true) {
        if (goertzelProcess()) {  // goertzelProcess updates toneDetectedState_
            no_tone_count = 0;
            tone_count++;
            if (tone_count > noiseBlankerLoops_) return true;  // Tone confirmed
        } else {
            tone_count = 0;
            no_tone_count++;
            if (no_tone_count > noiseBlankerLoops_) return false;  // No-tone confirmed
        }
    }
}

void CwDecoder::resetMorseTree() {
    treeIndex_ = MORSE_TREE_ROOT_INDEX;
    treeOffset_ = MORSE_TREE_INITIAL_OFFSET;
    treeCount_ = MORSE_TREE_MAX_DEPTH;
}

char CwDecoder::getCharFromTree() {
    if (treeIndex_ >= 0 && treeIndex_ < sizeof(MORSE_TREE_SYMBOLS)) {
        return MORSE_TREE_SYMBOLS[treeIndex_];
    }
    return ' ';  // Error or unrecognized
}

void CwDecoder::processDot() {
    treeIndex_ -= treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    symbolCountForWpm_ += 2;  // 1*dot + 1*space

    if (treeCount_ < 0) {  // Error in tree traversal
        DEBUG("CW Decoder: Tree error (dot)\n");
        resetMorseTree();
        // Reset speed estimation as well, as something is wrong
        currentReferenceMs_ = startReferenceMs_;
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        toneIndex_ = 0;
        symbolCountForWpm_ = 0;
        decoderStarted_ = false;
        measuringTone_ = false;
    }
}

void CwDecoder::processDash() {
    treeIndex_ += treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    symbolCountForWpm_ += 4;  // 3*dots + 1*space

    if (treeCount_ < 0) {  // Error in tree traversal
        DEBUG("CW Decoder: Tree error (dash)\n");
        resetMorseTree();
        currentReferenceMs_ = startReferenceMs_;
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        toneIndex_ = 0;
        symbolCountForWpm_ = 0;
        decoderStarted_ = false;
        measuringTone_ = false;
    }
}

void CwDecoder::updateReferenceTimings(unsigned long duration) {
    if ((toneMinDurationMs_ < 9999) && (toneMaxDurationMs_ > 0) && (toneMinDurationMs_ != toneMaxDurationMs_)) {
        if (duration < currentReferenceMs_) {  // Dot
            toneMinDurationMs_ = (toneMinDurationMs_ + duration) / 2;
        } else {  // Dash
            toneMaxDurationMs_ = (toneMaxDurationMs_ + duration) / 2;
        }
        currentReferenceMs_ = (toneMinDurationMs_ + toneMaxDurationMs_) / 2;
    } else {
        // Initial phase, or if min and max became equal (e.g. after a series of identical elements)
        toneMinDurationMs_ = min(toneMinDurationMs_, duration);
        toneMaxDurationMs_ = max(toneMaxDurationMs_, duration);
        if (toneMinDurationMs_ != toneMaxDurationMs_ && toneMaxDurationMs_ > 0) {  // Check to avoid division by zero or using uninitialized max
            currentReferenceMs_ = (toneMinDurationMs_ + toneMaxDurationMs_) / 2;
        } else {
            currentReferenceMs_ = startReferenceMs_;  // Fallback if min/max are still problematic
        }
    }
    // Ensure reference is not zero to prevent division by zero issues later
    if (currentReferenceMs_ == 0) {
        currentReferenceMs_ = startReferenceMs_;
    }
}

char CwDecoder::decodeNextCharacter() {
    bool currentToneState = sampleWithNoiseBlanking();  // This updates toneDetectedState_ and returns it
    unsigned long currentTimeMs = millis();
    char decodedChar = '\0';

    // Wait for initial leading_edge
    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        leadingEdgeTimeMs_ = currentTimeMs;
        currentLetterStartTimeMs_ = leadingEdgeTimeMs_;

        // Word space detection (simplified from .ino)
        // A word space is typically 7 units. A letter space is 3 units.
        // Reference is ~2 units (midpoint of dot and dash).
        // If space > 3*Reference (approx 6 units), consider it a word space.
        if ((currentTimeMs - trailingEdgeTimeMs_) > currentReferenceMs_ * 3 && trailingEdgeTimeMs_ != 0) {
            // This implies a word space might have occurred before this new character.
            // The .ino prints ' ' here. We can return it if no other char is decoded this cycle.
            // However, standard practice is to return the decoded char, and the caller handles spacing.
            // For now, we'll let the timeout logic handle inter-character spaces.
            // If a very long pause occurs, the timeout will eventually produce a space.
        }
        decoderStarted_ = true;
        measuringTone_ = true;
    }
    // Wait for trailing_edge
    else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        trailingEdgeTimeMs_ = currentTimeMs;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;

        if (duration > 0 && toneIndex_ < 6) {  // Basic validation and buffer check
            updateReferenceTimings(duration);
            toneDurationHistory_[toneIndex_] = (duration < currentReferenceMs_) ? toneMinDurationMs_ : toneMaxDurationMs_;
            // DEBUG("CW: Tone %d, Dur: %lu, Ref: %lu, Min: %lu, Max: %lu\n", toneIndex_, duration, currentReferenceMs_, toneMinDurationMs_, toneMaxDurationMs_);
            toneIndex_++;
        } else if (toneIndex_ >= 6) {
            DEBUG("CW: Tone history buffer full.\n");
            // Potentially reset or handle error, for now, we just stop adding.
        }

        measuringTone_ = false;
    }
    // Wait for second and subsequent leading edges (inter-element space)
    else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        // This is a new tone element starting after a short space.
        if ((currentTimeMs - trailingEdgeTimeMs_) < currentReferenceMs_) {  // Gap between elements of the same letter (1 unit)
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        }
        // If the gap is larger, it will be handled by the timeout logic below as a letter space.
    }
    // Timeout (end of letter or word)
    else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        // currentReferenceMs_ is roughly 2 units. Letter space is 3 units. Word space is 7 units.
        if ((currentTimeMs - trailingEdgeTimeMs_) > currentReferenceMs_ * 1.5f) {  // Timeout for end of letter (approx > 3 units)
                                                                                   // Using 1.5 * currentReference (which is avg of dot/dash)
                                                                                   // A dot is 1 unit, dash is 3. Ref is ~2. 1.5*2 = 3 units.
            if (toneIndex_ > 0) {                                                  // If we have received some tones for a letter
                if (toneMaxDurationMs_ != toneMinDurationMs_ && toneMinDurationMs_ < 9999 && toneMaxDurationMs_ > 0) {
                    // Update reference based on the whole letter if min/max are distinct
                    // This is a bit redundant if updateReferenceTimings does its job well per element.
                    // currentReferenceMs_ = (toneMaxDurationMs_ + toneMinDurationMs_) / 2;
                }

                for (short i = 0; i < toneIndex_; i++) {
                    if (toneDurationHistory_[i] < currentReferenceMs_) {
                        processDot();
                    } else {
                        processDash();
                    }
                }
                decodedChar = getCharFromTree();
                // DEBUG("CW: Decoded: '%c' (idx:%d)\n", decodedChar, treeIndex_);
            } else {
                // Timeout occurred but no tones were recorded for the current letter.
                // This could be a space between words if the timeout is long enough.
                // The .ino inserts a space if (millis() - Trailing_edge) > Reference * 3
                // which is roughly > 6 units.
                if ((currentTimeMs - trailingEdgeTimeMs_) > currentReferenceMs_ * 3.0f) {
                    decodedChar = ' ';  // Word space
                }
            }

            // Reset for next character/word
            resetMorseTree();
            toneIndex_ = 0;
            // Keep toneMin/Max for continuous speed adaptation, or reset them periodically/conditionally.
            // The .ino keeps them.
            // symbolCountForWpm_ = 0; // Reset for WPM calculation of the *next* word/char.
            decoderStarted_ = false;  // Ready for a new character.
            // currentLetterStartTimeMs_ = currentTimeMs; // Start time for the *next* letter.
        }
    }
    return decodedChar;
}
