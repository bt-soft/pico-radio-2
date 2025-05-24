#include "CwDecoder.h"

#include <cmath>  // round, sin, cos, sqrt, abs, min, max (already in .h for constexpr, but good practice)

#include "defines.h"  // DEBUG

// Goertzel filter parameters are now constexpr in CwDecoder.h

const unsigned long CwDecoder::SAMPLING_PERIOD_US = static_cast<unsigned long>(1000000.0f / CwDecoder::SAMPLING_FREQ);

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

CwDecoder::CwDecoder(int audioPin) : audioInputPin_(audioPin) { initialize(); }

CwDecoder::~CwDecoder() {}

void CwDecoder::initialize() {
    noiseBlankerLoops_ = 2;
    startReferenceMs_ = 250;
    currentReferenceMs_ = startReferenceMs_;
    leadingEdgeTimeMs_ = 0;
    trailingEdgeTimeMs_ = 0;
    toneIndex_ = 0;
    toneMaxDurationMs_ = 0L;
    toneMinDurationMs_ = 9999L;
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
    memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
}

void CwDecoder::resetDecoderState() {
    initialize();
    DEBUG("CW Decoder state reset.\n");
}

bool CwDecoder::goertzelProcess() {
    unsigned long loopStartTimeMicros;
    for (short index = 0; index < N_SAMPLES; index++) {
        loopStartTimeMicros = micros();
        testData[index] = analogRead(audioInputPin_);
        unsigned long processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < SAMPLING_PERIOD_US) {
            delayMicroseconds(SAMPLING_PERIOD_US - processingTimeMicros);
        }
    }

    q1 = 0;
    q2 = 0;
    for (short index = 0; index < N_SAMPLES; index++) {
        q0 = COEFF * q1 - q2 + (float)testData[index];
        q2 = q1;
        q1 = q0;
    }
    float magnitudeSquared = (q1 * q1) + (q2 * q2) - q1 * q2 * COEFF;
    float magnitude = sqrt(magnitudeSquared);

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
        if (goertzelProcess()) {
            no_tone_count = 0;
            tone_count++;
            if (tone_count >= noiseBlankerLoops_) return true;
        } else {
            tone_count = 0;
            no_tone_count++;
            if (no_tone_count >= noiseBlankerLoops_) return false;
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
    return ' ';
}

void CwDecoder::processDot() {
    treeIndex_ -= treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    symbolCountForWpm_ += 2;
    if (treeCount_ < 0) {
        DEBUG("CW Decoder: Tree error (dot)\n");
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

void CwDecoder::processDash() {
    treeIndex_ += treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    symbolCountForWpm_ += 4;
    if (treeCount_ < 0) {
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
    bool likely_dot = false;
    if (toneMinDurationMs_ == 9999L) {
        likely_dot = (duration < startReferenceMs_);
    } else {
        likely_dot = (duration < currentReferenceMs_);
    }

    if (likely_dot) {
        if (toneMinDurationMs_ == 9999L) {
            toneMinDurationMs_ = duration;
        } else {
            toneMinDurationMs_ = (toneMinDurationMs_ * 3 + duration) / 4;
        }
    }

    if (!likely_dot || (toneMinDurationMs_ != 9999L && duration > toneMinDurationMs_ * 1.8f) || toneMaxDurationMs_ == 0L) {
        if (toneMaxDurationMs_ == 0L || duration > toneMaxDurationMs_) {
            toneMaxDurationMs_ = duration;
        } else {
            toneMaxDurationMs_ = (toneMaxDurationMs_ * 3 + duration) / 4;
        }
    }

    if (toneMinDurationMs_ != 9999L && toneMaxDurationMs_ != 0L && toneMinDurationMs_ > toneMaxDurationMs_) {
        DEBUG("CW: Correcting Min (%lu) > Max (%lu) by swapping.\n", toneMinDurationMs_, toneMaxDurationMs_);
        unsigned long temp = toneMinDurationMs_;
        toneMinDurationMs_ = toneMaxDurationMs_;
        toneMaxDurationMs_ = temp;
    }

    if (toneMinDurationMs_ < 9999L && toneMaxDurationMs_ > 0L) {
        currentReferenceMs_ = (toneMinDurationMs_ + toneMaxDurationMs_) / 2;
    } else if (toneMinDurationMs_ < 9999L) {
        currentReferenceMs_ = toneMinDurationMs_ * 2;
    } else if (toneMaxDurationMs_ > 0L) {
        currentReferenceMs_ = toneMaxDurationMs_ * 2 / 3;
    } else {
        currentReferenceMs_ = startReferenceMs_;
    }

    if (currentReferenceMs_ == 0) currentReferenceMs_ = startReferenceMs_;
    currentReferenceMs_ = constrain(currentReferenceMs_, 50UL, 800UL);
}

char CwDecoder::decodeNextCharacter() {
    bool currentToneState = sampleWithNoiseBlanking();
    unsigned long currentTimeMs = millis();
    char decodedChar = '\0';

    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        leadingEdgeTimeMs_ = currentTimeMs;
        currentLetterStartTimeMs_ = leadingEdgeTimeMs_;
        decoderStarted_ = true;
        measuringTone_ = true;
    } else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        trailingEdgeTimeMs_ = currentTimeMs;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;

        if (duration > MIN_MORSE_ELEMENT_DURATION_MS && toneIndex_ < 6) {
            updateReferenceTimings(duration);
            rawToneDurations_[toneIndex_] = duration;
            DEBUG("CW: Tone %d, RawDur: %lu, Ref: %lu, Min: %lu, Max: %lu\n", toneIndex_, duration, currentReferenceMs_, toneMinDurationMs_, toneMaxDurationMs_);
            toneIndex_++;
        } else if (duration <= MIN_MORSE_ELEMENT_DURATION_MS && duration > 0) {
            DEBUG("CW: Ignored short pulse, Dur: %lu ms\n", duration);
        } else if (toneIndex_ >= 6) {
            DEBUG("CW: Tone history buffer full.\n");
        }
        measuringTone_ = false;
    } else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        // Check if the gap is short enough to be an inter-element space
        if ((currentTimeMs - trailingEdgeTimeMs_) < currentReferenceMs_) {
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        } else {
            // Gap is too long, treat as end of character / start of new character
            // This logic branch will lead to the next 'else if' if the tone persists
            // or if the tone ends, leading to character decoding.
            // If a new tone starts after a long gap, it will be caught by the first 'if'
            // after the current character (if any) is decoded due to timeout.
            // To be absolutely robust, we might need to force a character decode here
            // if toneIndex_ > 0, but the timeout logic should generally catch it.
        }
    } else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        // Timeout for end of character / word space
        if ((currentTimeMs - trailingEdgeTimeMs_) > currentReferenceMs_ * 1.5f) {  // Character space threshold
            if (toneIndex_ > 0) {
                for (short i = 0; i < toneIndex_; i++) {
                    if (rawToneDurations_[i] < currentReferenceMs_) {
                        processDot();
                    } else {
                        processDash();
                    }
                }
                decodedChar = getCharFromTree();
                DEBUG("CW: Decoded: '%c' (idx:%d)\n", decodedChar, treeIndex_);
            }
            // Check for word space after character decoding or if no elements were in buffer
            if ((currentTimeMs - trailingEdgeTimeMs_) > currentReferenceMs_ * 3.0f && decodedChar != '\0' && decodedChar != ' ') {
                // If a character was just decoded, and the pause is even longer, consider it a word space.
                // This is a simplification; a more robust word space might need to be handled
                // by ensuring the *next* character starts after an even longer pause,
                // or by sending a space if no new tone starts within a word-space timeout.
                // For now, if a char was decoded, and a long pause follows, we assume the space is implicit
                // or will be handled by the next character's timing.
                // If no char was decoded (e.g. just noise then long pause), we might send a space.
                if (toneIndex_ == 0 && decodedChar == '\0') {  // No elements, long pause
                                                               // decodedChar = ' '; // Potentially send a space if desired after a long silence with no prior elements.
                }
            }
            resetMorseTree();
            toneIndex_ = 0;
            decoderStarted_ = false;  // Reset decoder started state
            // toneMinDurationMs_ = 9999L; // Optionally reset timing references for very distinct speed changes
            // toneMaxDurationMs_ = 0L;
            // currentReferenceMs_ = startReferenceMs_;
        }
    }
    return decodedChar;
}
