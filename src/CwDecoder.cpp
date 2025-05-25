#include "CwDecoder.h"

#include <cmath>  // round, sin, cos, sqrt, abs, min, max (already in .h for constexpr, but good practice)

#include "defines.h"  // DEBUG

// CW működés debug engedélyezése de csak DEBUG módban

#ifdef nem__DEBUG
#define CW_DEBUG(fmt, ...) DEBUG(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define CW_DEBUG(fmt, ...)  // Üres makró, ha __DEBUG nincs definiálva
#endif

// Goetzel filter paraméter a SAMPLING_FREQ-hez
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
    noiseBlankerLoops_ = 1;
    startReferenceMs_ = 150;  // Kezdő referencia 15WPM-hez (pont ~80ms, küszöb ~150-160ms)
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
    lastActivityMs_ = 0;  // Tagváltozó inicializálása
    lastDecodedChar_ = '\0';
    wordSpaceProcessed_ = false;
    inInactiveState = false;
    resetMorseTree();
    q0 = 0;
    q1 = 0;
    q2 = 0;
    memset(testData, 0, sizeof(testData));
    memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
    // Puffer inicializálása
    memset(decodedCharBuffer_, 0, sizeof(decodedCharBuffer_));
    charBufferReadPos_ = 0;
    charBufferWritePos_ = 0;
    charBufferCount_ = 0;
}

void CwDecoder::resetDecoderState() {
    initialize();
    CW_DEBUG("CW Decoder state reset.\n");
}

bool CwDecoder::goertzelProcess() {
    unsigned long loopStartTimeMicros;
    for (short index = 0; index < N_SAMPLES; index++) {
        loopStartTimeMicros = micros();
        int raw_adc = analogRead(audioInputPin_);
        testData[index] = raw_adc - 2048;  // DC offset removal (assuming 12-bit ADC, 0-4095 range)
        unsigned long processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < SAMPLING_PERIOD_US) {
            if (SAMPLING_PERIOD_US > processingTimeMicros) delayMicroseconds(SAMPLING_PERIOD_US - processingTimeMicros);
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

    for (int i = 0; i < noiseBlankerLoops_; i++) {
        if (goertzelProcess()) {
            tone_count++;
        } else {
            no_tone_count++;
        }
    }
    return (tone_count > no_tone_count);
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
    return '\0';
}

void CwDecoder::processDot() {
    treeIndex_ -= treeOffset_;
    treeOffset_ /= 2;
    treeCount_--;
    symbolCountForWpm_ += 2;
    if (treeCount_ < 0) {
        CW_DEBUG("CW Decoder: Tree error (dot)\n");
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

        CW_DEBUG("CW Decoder: Tree error (dash)\n");

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
    // Adaptív referencia
    const unsigned long ADAPTIVE_WEIGHT_OLD = 3;  // Régi érték súlya (csökkentve 7-ről)
    const unsigned long ADAPTIVE_WEIGHT_NEW = 1;  // Új érték súlya
    const unsigned long ADAPTIVE_DIVISOR = ADAPTIVE_WEIGHT_OLD + ADAPTIVE_WEIGHT_NEW;

    if (toneMinDurationMs_ == 9999L) {
        // Első elem
        if (duration < (startReferenceMs_ * 1.2)) {
            toneMinDurationMs_ = duration;
            currentReferenceMs_ = duration * 2.0;
        } else {
            toneMinDurationMs_ = duration / 3;
            toneMaxDurationMs_ = duration;
            currentReferenceMs_ = (toneMinDurationMs_ + toneMaxDurationMs_) / 2;
        }
    } else {
        unsigned long currentThreshold = currentReferenceMs_;
        if (duration < currentThreshold) {
            toneMinDurationMs_ = (toneMinDurationMs_ * ADAPTIVE_WEIGHT_OLD + duration * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
        } else {
            if (toneMaxDurationMs_ == 0L) {
                toneMaxDurationMs_ = duration;
            } else {
                toneMaxDurationMs_ = (toneMaxDurationMs_ * ADAPTIVE_WEIGHT_OLD + duration * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
            }
        }

        if (toneMaxDurationMs_ > 0L && toneMinDurationMs_ < 9999L) {
            unsigned long calculatedRef = toneMinDurationMs_ * 2.0;
            currentReferenceMs_ = (currentReferenceMs_ * ADAPTIVE_WEIGHT_OLD + calculatedRef * ADAPTIVE_WEIGHT_NEW) / ADAPTIVE_DIVISOR;
        }
    }

    // Biztonságos tartomány
    unsigned long lowerBound = DOT_MIN_MS * 1.25f;  // Pl. 40 * 1.25 = 50ms
    unsigned long upperBound = DOT_MAX_MS * 1.5f;   // Pl. 200 * 1.5 = 300ms.
    currentReferenceMs_ = constrain(currentReferenceMs_, lowerBound, upperBound);

    CW_DEBUG("CW: Ref frissítve - min: %lu, max: %lu, ref: %lu, új elem: %lu\n", toneMinDurationMs_, toneMaxDurationMs_, currentReferenceMs_, duration);
}

char CwDecoder::processCollectedElements() {
    if (toneIndex_ == 0) return '\0';
    CW_DEBUG("CW: Feldolgozás - %d elem, ref: %lu ms\n", toneIndex_, currentReferenceMs_);
    resetMorseTree();

    for (short i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];
        if (duration < currentReferenceMs_) {
            processDot();
            CW_DEBUG("CW: [%d] Pont: %lu ms\n", i, duration);
        } else {
            processDash();
            CW_DEBUG("CW: [%d] Vonás: %lu ms\n", i, duration);
        }
    }

    // Érvényes karakter?
    char result = getCharFromTree();
    if (result != '\0') {       // Csak azt ellenőrizzük, hogy nem null karakter-e
        if (isprint(result)) {  // Nyomtatható karakter
            CW_DEBUG("CW: Érvényes karakter dekódolva: '%c'\n", result);
            return result;
        }
    } else {
        // Ha result '\0', az azt jelenti, hogy a getCharFromTree() ' '-t adott vissza, de a fa gyökerénél vagy érvénytelen indexen volt
        CW_DEBUG("CW: Ismeretlen minta - treeIndex: %d\n", treeIndex_);
    }
    return '\0';
}

void CwDecoder::addToBuffer(char c) {
    if (c == '\0') {  // Üres karaktert nem teszünk a pufferbe
        return;
    }

    decodedCharBuffer_[charBufferWritePos_] = c;
    charBufferWritePos_ = (charBufferWritePos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    if (charBufferCount_ < DECODED_CHAR_BUFFER_SIZE) {
        charBufferCount_++;
    } else {
        // Ha a puffer tele volt, a legrégebbi elem (readPos) felülíródott,
        // ezért a readPos-t is el kell léptetni.
        charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    }
    CW_DEBUG("CW: Char '%c' added to buffer. Count: %d, R:%d, W:%d\n", c, charBufferCount_, charBufferReadPos_, charBufferWritePos_);
}

char CwDecoder::getCharacterFromBuffer() {
    if (charBufferCount_ == 0) {
        return '\0';
    }
    char c = decodedCharBuffer_[charBufferReadPos_];
    charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    charBufferCount_--;
    CW_DEBUG("CW: Char '%c' read from buffer. Count: %d, R:%d, W:%d\n", c, charBufferCount_, charBufferReadPos_, charBufferWritePos_);
    return c;
}

void CwDecoder::updateDecoder() {
    // Ez a metódus hívja a belső logikát, ami a pufferbe teszi a karaktert.
    internalProcessNextCharacter();
}

void CwDecoder::internalProcessNextCharacter() {
    static const unsigned long MAX_SILENCE_MS = 4000;
    static const unsigned long ELEMENT_GAP_MIN_MS = DOT_MIN_MS / 2;

    unsigned long estimatedDotLength = (toneMinDurationMs_ == 9999L || toneMinDurationMs_ == 0) ? (currentReferenceMs_ / 2) : toneMinDurationMs_;
    if (estimatedDotLength < DOT_MIN_MS || currentReferenceMs_ == 0) estimatedDotLength = DOT_MIN_MS;  // Biztosítjuk, hogy legyen értelmes alap

    unsigned long charGapMs = max(MIN_CHAR_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * CHAR_GAP_DOT_MULTIPLIER));
    unsigned long wordGapMs = max(MIN_WORD_GAP_MS_FALLBACK, (unsigned long)(estimatedDotLength * WORD_GAP_DOT_MULTIPLIER));
    // Biztosítjuk, hogy a wordGapMs nagyobb legyen, mint a charGapMs
    if (wordGapMs <= charGapMs) wordGapMs = charGapMs + max(1UL, MIN_CHAR_GAP_MS_FALLBACK / 2);

    bool currentToneState = sampleWithNoiseBlanking();
    unsigned long currentTimeMs = millis();
    char decodedChar = '\0';

    if (currentToneState) {
        lastActivityMs_ = currentTimeMs;
        // Ha hangot észlelünk, és előtte nem mértünk hangot (azaz egy új hang kezdődött)
        if (!measuringTone_) {
            wordSpaceProcessed_ = false;  // Új hang megszakítja az előző csendperiódust, reseteljük a flag-et
        }
    }

    if (currentTimeMs - lastActivityMs_ > MAX_SILENCE_MS) {
        if (!inInactiveState) {   // Csak akkor írjuk ki, ha még nem tettük
            resetDecoderState();  // Teljes reset, ami az initialize()-t hívja
            CW_DEBUG("CW: Reset inactivity (%lu ms) miatt\n", MAX_SILENCE_MS);
            inInactiveState = true;  // Beállítjuk, hogy kiírtuk
        }
        return;  // Nem adunk vissza karaktert, csak resetelünk
    }

    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        leadingEdgeTimeMs_ = currentTimeMs;
        decoderStarted_ = true;
        inInactiveState = false;  // Újra aktívak vagyunk, reseteljük a flag-et
        measuringTone_ = true;
        wordSpaceProcessed_ = false;  // Új hangjel kezdődött

        CW_DEBUG("CW: Első él, idő: %lu\n", currentTimeMs);

    } else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        trailingEdgeTimeMs_ = currentTimeMs;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;

        CW_DEBUG("CW: Hang vége, tartam: %lu ms\n", duration);

        if (toneIndex_ >= 6) {

            CW_DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás hang végén\n", toneIndex_);

            decodedChar = processCollectedElements();
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            resetMorseTree();
            toneIndex_ = 0;
        }

        if (duration >= DOT_MIN_MS && duration <= DASH_MAX_MS && toneIndex_ < 6) {
            rawToneDurations_[toneIndex_] = duration;
            toneIndex_++;
            updateReferenceTimings(duration);

            CW_DEBUG("CW: Elem hozzáadva [%d]: %lu ms, ref: %lu ms\n", toneIndex_ - 1, duration, currentReferenceMs_);
        } else {

            if (duration > DASH_MAX_MS) {

                CW_DEBUG("CW: TÚL HOSSZÚ elem: %lu ms (max: %lu, index: %d)\n", duration, DASH_MAX_MS, toneIndex_);
            } else if (duration < DOT_MIN_MS) {
                CW_DEBUG("CW: TÚL RÖVID elem: %lu ms (min: %lu, index: %d)\n", duration, DOT_MIN_MS, toneIndex_);
            }
        }
        measuringTone_ = false;
    } else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        unsigned long gapDuration = currentTimeMs - trailingEdgeTimeMs_;
        wordSpaceProcessed_ = false;  // Új hangjel kezdődött egy szünet után
        if (toneIndex_ >= 6) {
            CW_DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás\n", toneIndex_);
            decodedChar = processCollectedElements();
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            resetMorseTree();
            toneIndex_ = 0;
        }
        if (gapDuration >= charGapMs && toneIndex_ > 0) {  // Karakter elválasztó szünet
            CW_DEBUG("CW: Karakterhatár detektálva, gap: %lu ms (küszöb: %lu ms)\n", gapDuration, charGapMs);
            decodedChar = processCollectedElements();
            if (decodedChar != '\0') {
                lastDecodedChar_ = decodedChar;
                lastActivityMs_ = currentTimeMs;
            }
            resetMorseTree();
            // trailingEdgeTimeMs_ nem frissül itt, mert a következő hang leadingEdgeTimeMs_-e lesz a mérvadó
            toneIndex_ = 0;
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
            CW_DEBUG("CW: Karakterhatár, gap: %lu ms\n", gapDuration);
        } else if (gapDuration >= ELEMENT_GAP_MIN_MS || toneIndex_ == 0) {
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        } else {
            CW_DEBUG("CW: Rövid gap: %lu ms (küszöb: %lu)\n", gapDuration, charGapMs);
            if (toneIndex_ >= 1 && gapDuration >= 150) {
                CW_DEBUG("CW: 1+ elem 150ms+ gap-pel - karakterhatár feltételezés\n");
                // Ez az ág potenciálisan problémás lehet, ha a 150ms rövidebb, mint a charGapMs.
                // De meghagyjuk az eredeti logika szerint.
                decodedChar = processCollectedElements();
                if (decodedChar != '\0') lastDecodedChar_ = decodedChar;  // lastActivityMs frissül, ha van karakter
                memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
                resetMorseTree();
                toneIndex_ = 0;
            }
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        }
    } else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        unsigned long spaceDuration = currentTimeMs - trailingEdgeTimeMs_;
        if ((spaceDuration > charGapMs && toneIndex_ > 0) || toneIndex_ >= 6) {
            if (toneIndex_ >= 6) {
                CW_DEBUG("CW: Tömb tele csendben (%d elem), kényszer dekódolás\n", toneIndex_);
            } else {
                CW_DEBUG("CW: Hosszú csend detektálva, space: %lu ms (küszöb: %lu ms)\n", spaceDuration, charGapMs);
            }
            decodedChar = processCollectedElements();
            resetMorseTree();
            memset(rawToneDurations_, 0, sizeof(rawToneDurations_));
            toneIndex_ = 0;
            decoderStarted_ = false;
            CW_DEBUG("CW: Hosszú csend, space: %lu ms\n", spaceDuration);
        }
    }

    if (decodedChar != '\0') {
        CW_DEBUG("CW: Dekódolt karakter: '%c'\n", decodedChar);
    }
    addToBuffer(decodedChar);
    return;
}
