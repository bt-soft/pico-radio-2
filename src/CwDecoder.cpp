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
    noiseBlankerLoops_ = 1;   // Csökkentett zajszűrő az érzékenység javításához
    startReferenceMs_ = 250;  // Alacsonyabb kezdő referencia a jobb detektáláshoz
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
    // DEBUG("Goertzel: Sampling %d times, period %lu us\n", N_SAMPLES, SAMPLING_PERIOD_US);
    for (short index = 0; index < N_SAMPLES; index++) {
        loopStartTimeMicros = micros();
        int raw_adc = analogRead(audioInputPin_);
        testData[index] = raw_adc - 2048;  // DC offset removal (assuming 12-bit ADC, 0-4095 range)
        // if (index < 3) DEBUG("Raw ADC: %d, Centered: %d\n", raw_adc, testData[index]); // Log only a few samples
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
    // DEBUG("Goertzel Mag: %.2f, Threshold: %.2f\n", magnitude, THRESHOLD);

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

    // Egyszerűsített zajszűrés
    for (int i = 0; i < noiseBlankerLoops_; i++) {
        if (goertzelProcess()) {
            tone_count++;
        } else {
            no_tone_count++;
        }
    }

    // Többségi döntés
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
    // Agresszívebb adaptív referencia - alacsonyabb küszöbök a valós CW-hez
    // Cél: 50-200ms = pont, 250-800ms = vonás, küszöb ~200-250ms körül

    if (toneMinDurationMs_ == 9999L) {
        // Első elem - alacsonyabb kezdő küszöb
        if (duration < 250) {
            // Valószínűleg pont
            toneMinDurationMs_ = duration;
            currentReferenceMs_ = duration * 2.5;  // 2.5x pont hossz = vonás küszöb
        } else {
            // Valószínűleg vonás
            toneMinDurationMs_ = 100;  // Becsült pont érték
            toneMaxDurationMs_ = duration;
            currentReferenceMs_ = 200;  // Alacsonyabb kezdő küszöb
        }
    } else {
        // Adaptív frissítés - dinamikus küszöb alapján
        unsigned long currentThreshold = currentReferenceMs_;

        if (duration < currentThreshold) {
            // Pont kategória - gyorsabb adaptáció
            toneMinDurationMs_ = (toneMinDurationMs_ * 3 + duration) / 4;
        } else {
            // Vonás kategória - gyorsabb adaptáció
            if (toneMaxDurationMs_ == 0L) {
                toneMaxDurationMs_ = duration;
            } else {
                toneMaxDurationMs_ = (toneMaxDurationMs_ * 3 + duration) / 4;
            }
        }

        // Referencia újraszámítás - 2.5:1 arány (agresszívebb)
        if (toneMaxDurationMs_ > 0L && toneMinDurationMs_ < 9999L) {
            // Pont * 2.5 = vonás küszöb
            unsigned long calculatedRef = toneMinDurationMs_ * 2.5;
            // Gyorsabb mozgás a számított érték felé
            currentReferenceMs_ = (currentReferenceMs_ + calculatedRef) / 2;
        }
    }

    // Biztonságos tartomány - még alacsonyabb értékek
    currentReferenceMs_ = constrain(currentReferenceMs_, 150UL, 300UL);

    DEBUG("CW: Ref frissítve - min: %lu, max: %lu, ref: %lu, új elem: %lu\n", toneMinDurationMs_, toneMaxDurationMs_, currentReferenceMs_, duration);
}

char CwDecoder::decodeNextCharacter() {
    static unsigned long lastActivityMs = 0;
    static const unsigned long MAX_SILENCE_MS = 4000;                // 4 másodperc után reset
    static const unsigned long DOT_MIN_MS = 30;                      // Alacsonyabb minimum
    static const unsigned long DOT_MAX_MS = 400;                     // Magasabb maximum
    static const unsigned long DASH_MAX_MS = DOT_MAX_MS * 5;         // Vonás maximum 5x pont (2000ms max)
    static const unsigned long ELEMENT_GAP_MIN_MS = DOT_MIN_MS / 2;  // Elemek közötti szünet minimum

    // Agresszívabb karakterhatár-detektálás - rövidebb küszöbök
    unsigned long charGapMs = max(200UL, currentReferenceMs_ / 2);  // Karakterhatár = referencia/2, min 200ms

    bool currentToneState = sampleWithNoiseBlanking();
    unsigned long currentTimeMs = millis();
    char decodedChar = '\0';

    // Aktivitás frissítése
    if (currentToneState) {
        lastActivityMs = currentTimeMs;
    }

    // Ha huzamosabb ideig nincs aktivitás, reset
    if (currentTimeMs - lastActivityMs > MAX_SILENCE_MS) {
        resetMorseTree();
        toneIndex_ = 0;
        decoderStarted_ = false;
        measuringTone_ = false;
        toneMinDurationMs_ = 9999L;
        toneMaxDurationMs_ = 0L;
        currentReferenceMs_ = startReferenceMs_;
        DEBUG("CW: Reset inactivity miatt\n");
        return '\0';
    }

    // Állapotgép
    if (!decoderStarted_ && !measuringTone_ && currentToneState) {
        // Első hangjelzés érzékelése
        leadingEdgeTimeMs_ = currentTimeMs;
        decoderStarted_ = true;
        measuringTone_ = true;
        DEBUG("CW: Első él, idő: %lu\n", currentTimeMs);
    } else if (decoderStarted_ && measuringTone_ && !currentToneState) {
        // Hangjelzés vége
        trailingEdgeTimeMs_ = currentTimeMs;
        unsigned long duration = trailingEdgeTimeMs_ - leadingEdgeTimeMs_;

        DEBUG("CW: Hang vége, tartam: %lu ms\n", duration);  // Ha a tömb már tele van (6 elem), dekódoljuk azonnal
        if (toneIndex_ >= 6) {
            DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás hang végén\n", toneIndex_);
            decodedChar = processCollectedElements();
            resetMorseTree();
            toneIndex_ = 0;
        }

        // Csak érvényes hosszúságú elemeket fogadunk el - bővebb tartomány
        if (duration >= DOT_MIN_MS && duration <= DASH_MAX_MS && toneIndex_ < 6) {  // 6 elemig engedélyezünk
            rawToneDurations_[toneIndex_] = duration;
            toneIndex_++;

            // Adaptív referencia frissítése egyszerűbb módon
            updateReferenceTimings(duration);

            DEBUG("CW: Elem hozzáadva [%d]: %lu ms, ref: %lu ms\n", toneIndex_ - 1, duration, currentReferenceMs_);
        } else {
            DEBUG("CW: Érvénytelen elem: %lu ms (tartomány: %lu-%lu, index: %d)\n", duration, DOT_MIN_MS, DASH_MAX_MS, toneIndex_);
        }

        measuringTone_ = false;
    } else if (decoderStarted_ && !measuringTone_ && currentToneState) {
        // Új hangjelzés kezdete (elemek közötti szünet után)
        unsigned long gapDuration = currentTimeMs - trailingEdgeTimeMs_;  // Ha a tömb már tele van, dekódoljuk
        if (toneIndex_ >= 6) {
            DEBUG("CW: Tömb tele (%d elem), kényszer dekódolás\n", toneIndex_);
            decodedChar = processCollectedElements();
            resetMorseTree();
            toneIndex_ = 0;
        }
        if (gapDuration >= charGapMs && toneIndex_ > 0) {
            // Karakterhatár detektálás - dekódoljuk az eddigi elemeket
            DEBUG("CW: Karakterhatár detektálva, gap: %lu ms (küszöb: %lu ms)\n", gapDuration, charGapMs);
            decodedChar = processCollectedElements();

            // Új karakter kezdése
            resetMorseTree();
            toneIndex_ = 0;
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;

            DEBUG("CW: Karakterhatár, gap: %lu ms\n", gapDuration);
        } else if (gapDuration >= ELEMENT_GAP_MIN_MS || toneIndex_ == 0) {
            // Elem közötti szünet - ugyanazon karakter folytatása
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        } else {
            DEBUG("CW: Rövid gap: %lu ms (küszöb: %lu)\n", gapDuration, charGapMs);
            // Agresszív karakterhatár - ha van elem és a gap elég hosszú, dekódoljunk
            if (toneIndex_ >= 1 && gapDuration >= 150) {
                DEBUG("CW: 1+ elem 150ms+ gap-pel - karakterhatár feltételezés\n");
                decodedChar = processCollectedElements();
                resetMorseTree();
                toneIndex_ = 0;
            }
            leadingEdgeTimeMs_ = currentTimeMs;
            measuringTone_ = true;
        }
    } else if (decoderStarted_ && !measuringTone_ && !currentToneState) {
        // Csend folytatása - ellenőrizzük, hogy elég hosszú-e a karakterhatárhoz
        unsigned long spaceDuration = currentTimeMs - trailingEdgeTimeMs_;  // Automatikus dekódolás hosszabb szünet után vagy ha a tömb tele van
        if ((spaceDuration > charGapMs && toneIndex_ > 0) || toneIndex_ >= 6) {
            // Hosszú csend vagy tele tömb - dekódoljuk a karaktert
            if (toneIndex_ >= 6) {
                DEBUG("CW: Tömb tele csendben (%d elem), kényszer dekódolás\n", toneIndex_);
            } else {
                DEBUG("CW: Hosszú csend detektálva, space: %lu ms (küszöb: %lu ms)\n", spaceDuration, charGapMs);
            }
            decodedChar = processCollectedElements();

            // Reset
            resetMorseTree();
            toneIndex_ = 0;
            decoderStarted_ = false;

            DEBUG("CW: Hosszú csend, space: %lu ms\n", spaceDuration);
        }
    }

    if (decodedChar != '\0') {
        DEBUG("CW: Dekódolt karakter: '%c'\n", decodedChar);
    }

    return decodedChar;
}

char CwDecoder::processCollectedElements() {
    if (toneIndex_ == 0) return '\0';

    DEBUG("CW: Feldolgozás - %d elem, ref: %lu ms\n", toneIndex_, currentReferenceMs_);

    // Reset a Morse fa
    resetMorseTree();

    // Dekódoljuk az elemeket
    for (short i = 0; i < toneIndex_; i++) {
        unsigned long duration = rawToneDurations_[i];

        if (duration < currentReferenceMs_) {
            processDot();
            DEBUG("CW: [%d] Pont: %lu ms\n", i, duration);
        } else {
            processDash();
            DEBUG("CW: [%d] Vonás: %lu ms\n", i, duration);
        }
    }

    char result = getCharFromTree();

    // Bővebb validáció és debug info
    if (result != ' ' && result != '\0') {
        if ((result >= 'A' && result <= 'Z') || (result >= '0' && result <= '9')) {
            DEBUG("CW: Érvényes karakter dekódolva: '%c'\n", result);
            return result;
        } else {
            DEBUG("CW: Speciális karakter: '%c' (ASCII: %d)\n", result, (int)result);
            // Egyes speciális karaktereket is engedélyezzünk
            if (result == '.' || result == ',' || result == '?' || result == '!' || result == ':' || result == ';' || result == '+' || result == '-' || result == '=' ||
                result == '/' || result == '@') {
                return result;
            }
        }
    } else {
        DEBUG("CW: Ismeretlen minta - treeIndex: %d\n", treeIndex_);
    }

    return '\0';
}
