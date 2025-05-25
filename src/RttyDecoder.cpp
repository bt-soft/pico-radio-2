/**
 * @file RttyDecoder.cpp
 * @brief RTTY dekóder implementáció a CwDecoder mintájára
 *
 * Az osztály RTTY jelek dekódolására szolgál dupla Goertzel algoritmus segítségével.
 * Automatikusan detektálja a shift frekvenciát (170Hz vagy 850Hz) és a baud rate-et (45-100 baud).
 * Mark és Space frekvenciák automatikus felismerése a defines.h-ban megadott értékek alapján.
 */

#include "RttyDecoder.h"

#include <cmath>

#include "defines.h"

// RTTY működés debug engedélyezése csak DEBUG módban
#ifdef __DEBUG
#define RTTY_DEBUG(fmt, ...) DEBUG(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define RTTY_DEBUG(fmt, ...)  // Üres makró, ha __DEBUG nincs definiálva
#endif

// Goertzel filter paraméter a SAMPLING_FREQ-hez
const unsigned long RttyDecoder::SAMPLING_PERIOD_US = static_cast<unsigned long>(1000000.0f / RttyDecoder::SAMPLING_FREQ);

// Goertzel konstansok Mark frekvenciához (2295Hz)
// K_MARK = round(84 * 2295 / 8400) = round(22.95) = 23
// Tényleges Mark frekvencia: (23 / 84) * 8400 = 2300 Hz
const short RttyDecoder::K_MARK = 23;
const float RttyDecoder::COEFF_MARK = -0.4067f;  // 2.0f * cos((2*PI*23)/84) = 2.0f * cos(1.719) = -0.4067

// Goertzel konstansok Space frekvenciához (2125Hz)
// K_SPACE = round(84 * 2125 / 8400) = round(21.25) = 21
// Tényleges Space frekvencia: (21 / 84) * 8400 = 2100 Hz
const short RttyDecoder::K_SPACE = 21;
const float RttyDecoder::COEFF_SPACE = 0.0000f;  // 2.0f * cos((2*PI*21)/84) = 2.0f * cos(π/2) = 0.0f

// Baudot LTRS (Letters) tábla - ITA2 standard
const char RttyDecoder::BAUDOT_LTRS_TABLE[32] = {
    '\0', 'E', '\n', 'A',  ' ', 'S', 'I', 'U',  // 0-7
    '\r', 'D', 'R',  'J',  'N', 'F', 'C', 'K',  // 8-15
    'T',  'Z', 'L',  'W',  'H', 'Y', 'P', 'Q',  // 16-23
    'O',  'B', 'G',  '\0', 'M', 'X', 'V', '\0'  // 24-31 (27=FIGS, 31=LTRS)
};

// Baudot FIGS (Figures) tábla - ITA2 standard
const char RttyDecoder::BAUDOT_FIGS_TABLE[32] = {
    '\0', '3', '\n', '-',  ' ', '\'', '8', '7',  // 0-7
    '\r', '$', '4',  '\a', ',', '!',  ':', '(',  // 8-15
    '5',  '+', ')',  '2',  '#', '6',  '0', '1',  // 16-23
    '9',  '?', '&',  '\0', '.', '/',  ';', '\0'  // 24-31 (27=FIGS, 31=LTRS)
};

/**
 * @brief RttyDecoder konstruktor
 * @param audioPin Az analóg bemenet pin száma, ahol az audio jel érkezik
 */
RttyDecoder::RttyDecoder(int audioPin) : audioInputPin_(audioPin) { initialize(); }

/**
 * @brief RttyDecoder destruktor
 */
RttyDecoder::~RttyDecoder() {}

/**
 * @brief Inicializálja az RTTY dekóder összes tagváltozóját alapértelmezett értékekre
 */
void RttyDecoder::initialize() {
    // Goertzel szűrő változók nullázása
    q0_mark = q1_mark = q2_mark = 0;
    q0_space = q1_space = q2_space = 0;
    memset(testData, 0, sizeof(testData));

    // RTTY állapotgép inicializálása
    currentState_ = IDLE;
    bitDurationMs_ = DEFAULT_BIT_DURATION_MS;
    lastBitTimeMs_ = 0;
    startBitTimeMs_ = 0;
    bitsReceived_ = 0;
    currentByte_ = 0;
    currentToneState_ = false;
    lastToneState_ = false;  // Automatikus detektálás
    autoDetectActive_ = true;
    autoDetectStartMs_ = millis();
    detectedMarkFreq_ = RTTY_MARKER_FREQUENCY;
    detectedSpaceFreq_ = RTTY_SPACE_FREQUENCY;
    detectedShiftFreq_ = RTTY_SHIFT_FREQUENCY;
    detectedBaudRate_ = MIN_BAUD_RATE;

    // Baudrate detektálás
    memset(transitionTimes_, 0, sizeof(transitionTimes_));
    transitionIndex_ = 0;
    lastTransitionMs_ = 0;

    // Karakter puffer
    memset(decodedCharBuffer_, 0, sizeof(decodedCharBuffer_));
    charBufferReadPos_ = 0;
    charBufferWritePos_ = 0;
    charBufferCount_ = 0;

    // Baudot dekódolás
    figsShift_ = false;  // Kezdeti állapot: LTRS mód

    RTTY_DEBUG("RTTY Decoder initialized. Mark: %.1f Hz, Space: %.1f Hz, Shift: %.1f Hz\n", detectedMarkFreq_, detectedSpaceFreq_, detectedShiftFreq_);
}

/**
 * @brief Visszaállítja az RTTY dekóder állapotát
 */
void RttyDecoder::resetDecoderState() {
    initialize();
    RTTY_DEBUG("RTTY Decoder state reset.\n");
}

/**
 * @brief Végrehajtja a Goertzel algoritmusokat Mark és Space frekvenciákra
 * @param isMarkStronger Kimenet: true ha Mark erősebb, false ha Space erősebb
 * @return true ha valamelyik jel erős enough, false egyébként
 */
bool RttyDecoder::processGoertzelFilters(bool& isMarkStronger) {
    unsigned long loopStartTimeMicros;

    // N_SAMPLES számú minta begyűjtése
    for (short index = 0; index < N_SAMPLES; index++) {
        loopStartTimeMicros = micros();
        int raw_adc = analogRead(audioInputPin_);
        testData[index] = raw_adc - 2048;  // DC eltolás eltávolítása (12-bit ADC)

        unsigned long processingTimeMicros = micros() - loopStartTimeMicros;
        if (processingTimeMicros < SAMPLING_PERIOD_US) {
            delayMicroseconds(SAMPLING_PERIOD_US - processingTimeMicros);
        }
    }

    // Mark frekvencia Goertzel szűrő
    q1_mark = 0;
    q2_mark = 0;
    for (short index = 0; index < N_SAMPLES; index++) {
        q0_mark = COEFF_MARK * q1_mark - q2_mark + static_cast<float>(testData[index]);
        q2_mark = q1_mark;
        q1_mark = q0_mark;
    }
    float markMagnitudeSquared = (q1_mark * q1_mark) + (q2_mark * q2_mark) - q1_mark * q2_mark * COEFF_MARK;
    float markMagnitude = sqrt(markMagnitudeSquared);

    // Space frekvencia Goertzel szűrő
    q1_space = 0;
    q2_space = 0;
    for (short index = 0; index < N_SAMPLES; index++) {
        q0_space = COEFF_SPACE * q1_space - q2_space + static_cast<float>(testData[index]);
        q2_space = q1_space;
        q1_space = q0_space;
    }
    float spaceMagnitudeSquared = (q1_space * q1_space) + (q2_space * q2_space) - q1_space * q2_space * COEFF_SPACE;
    float spaceMagnitude = sqrt(spaceMagnitudeSquared);

    // Jelerősség ellenőrzése
    float maxMagnitude = max(markMagnitude, spaceMagnitude);
    if (maxMagnitude < SIGNAL_THRESHOLD) {
        return false;  // Nincs elég erős jel
    }

    // Mark/Space arány ellenőrzése
    float ratio = (markMagnitude > spaceMagnitude) ? (markMagnitude / max(spaceMagnitude, 1.0f)) : (spaceMagnitude / max(markMagnitude, 1.0f));

    if (ratio < MARK_SPACE_RATIO_THRESHOLD) {
        return false;  // A két jel túl közel van egymáshoz
    }

    isMarkStronger = markMagnitude > spaceMagnitude;

    RTTY_DEBUG("RTTY Goertzel: Mark=%.1f, Space=%.1f, Ratio=%.2f, IsMarkStronger=%d\n", markMagnitude, spaceMagnitude, ratio, isMarkStronger);

    return true;
}

/**
 * @brief Detektálja az aktuális hangállapotot (Mark/Space)
 * @return true ha érvényes hangot detektált, false egyébként
 */
bool RttyDecoder::detectToneState() {
    bool isMarkStronger;
    if (!processGoertzelFilters(isMarkStronger)) {
        return false;  // Nincs érvényes jel
    }

    currentToneState_ = isMarkStronger;  // true = Mark, false = Space
    return true;
}

/**
 * @brief Frissíti a baud rate detektálást az állapotváltások alapján
 */
void RttyDecoder::updateBaudRateDetection() {
    unsigned long currentTime = millis();

    // Állapotváltás detektálása
    if (currentToneState_ != lastToneState_) {
        // Új állapotváltás
        if (lastTransitionMs_ > 0) {
            // Előző állapotváltás óta eltelt idő
            unsigned long timeSinceLastTransition = currentTime - lastTransitionMs_;

            // Tárolás a transition times tömbben
            transitionTimes_[transitionIndex_] = timeSinceLastTransition;
            transitionIndex_ = (transitionIndex_ + 1) % 10;
        }

        lastTransitionMs_ = currentTime;
        lastToneState_ = currentToneState_;

        RTTY_DEBUG("RTTY Transition: %s->%s at %lu ms\n", lastToneState_ ? "Space" : "Mark", currentToneState_ ? "Mark" : "Space", currentTime);
    }
}

/**
 * @brief Megbecsüli a baud rate-et a tárolt állapotváltások alapján
 * @return Becsült baud rate
 */
unsigned long RttyDecoder::estimateBaudRate() {
    unsigned long totalTime = 0;
    short validSamples = 0;

    for (short i = 0; i < 10; i++) {
        if (transitionTimes_[i] > 0 && transitionTimes_[i] >= MIN_BIT_DURATION_MS && transitionTimes_[i] <= MAX_BIT_DURATION_MS * 3) {
            totalTime += transitionTimes_[i];
            validSamples++;
        }
    }

    if (validSamples < 3) {
        return MIN_BAUD_RATE;  // Nincs elég adat, alapértelmezett érték
    }

    unsigned long averageBitTime = totalTime / validSamples;
    unsigned long baudRate = 1000 / averageBitTime;

    // Korlátok között tartás
    baudRate = constrain(baudRate, MIN_BAUD_RATE, MAX_BAUD_RATE);

    RTTY_DEBUG("RTTY Baud estimation: %lu samples, avg bit time: %lu ms, baud: %lu\n", validSamples, averageBitTime, baudRate);

    return baudRate;
}

/**
 * @brief Detektálja a shift frekvenciát
 * @return true ha sikeres a detektálás, false egyébként
 */
bool RttyDecoder::detectShiftFrequency() {
    float actualShift = abs(detectedMarkFreq_ - detectedSpaceFreq_);

    // 850Hz shift ellenőrzése
    if (abs(actualShift - TARGET_SHIFT_850HZ) <= SHIFT_TOLERANCE) {
        detectedShiftFreq_ = TARGET_SHIFT_850HZ;
        RTTY_DEBUG("RTTY Shift detected: 850Hz (actual: %.1f Hz)\n", actualShift);
        return true;
    }

    // 170Hz shift ellenőrzése (defines.h-ből)
    if (abs(actualShift - TARGET_SHIFT_170HZ) <= SHIFT_TOLERANCE) {
        detectedShiftFreq_ = TARGET_SHIFT_170HZ;
        RTTY_DEBUG("RTTY Shift detected: 170Hz (actual: %.1f Hz)\n", actualShift);
        return true;
    }

    RTTY_DEBUG("RTTY Shift not recognized: %.1f Hz\n", actualShift);
    return false;
}

/**
 * @brief Visszaállítja az RTTY állapotgépet
 */
void RttyDecoder::resetRttyStateMachine() {
    currentState_ = IDLE;
    bitsReceived_ = 0;
    currentByte_ = 0;
}

/**
 * @brief Dekódol egy Baudot karaktert ASCII-ra
 * @param baudotCode 5-bites Baudot kód
 * @return Dekódolt ASCII karakter vagy '\0' ha érvénytelen
 */
char RttyDecoder::decodeBaudotCharacter(uint8_t baudotCode) {
    if (baudotCode >= 32) {
        return '\0';  // Érvénytelen 5-bit kód
    }

    // Shift karakterek kezelése
    if (baudotCode == 27) {  // FIGS shift
        figsShift_ = true;
        RTTY_DEBUG("RTTY: Switched to FIGS mode\n");
        return '\0';
    }

    if (baudotCode == 31) {  // LTRS shift
        figsShift_ = false;
        RTTY_DEBUG("RTTY: Switched to LTRS mode\n");
        return '\0';
    }

    // Karakter dekódolása
    char result = figsShift_ ? BAUDOT_FIGS_TABLE[baudotCode] : BAUDOT_LTRS_TABLE[baudotCode];

    RTTY_DEBUG("RTTY: Decoded 0x%02X -> '%c' (%s mode)\n", baudotCode, result, figsShift_ ? "FIGS" : "LTRS");

    return result;
}

/**
 * @brief Hozzáad egy karaktert a dekódolt karakterek pufferéhez
 * @param c A hozzáadandó karakter
 */
void RttyDecoder::addToBuffer(char c) {
    if (c == '\0') {
        return;  // Üres karaktert nem teszünk a pufferbe
    }

    decodedCharBuffer_[charBufferWritePos_] = c;
    charBufferWritePos_ = (charBufferWritePos_ + 1) % DECODED_CHAR_BUFFER_SIZE;

    if (charBufferCount_ < DECODED_CHAR_BUFFER_SIZE) {
        charBufferCount_++;
    } else {
        // Puffer tele, legrégebbi elem felülírása
        charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    }

    RTTY_DEBUG("RTTY: Buffer add: '%c'. Count: %d, R:%d, W:%d\n", c, charBufferCount_, charBufferReadPos_, charBufferWritePos_);
}

/**
 * @brief Kivesz egy karaktert a dekódolt karakterek pufferéből
 * @return A legrégebbi dekódolt karakter vagy '\0' ha a puffer üres
 */
char RttyDecoder::getCharacterFromBuffer() {
    if (charBufferCount_ == 0) {
        return '\0';
    }

    char c = decodedCharBuffer_[charBufferReadPos_];
    charBufferReadPos_ = (charBufferReadPos_ + 1) % DECODED_CHAR_BUFFER_SIZE;
    charBufferCount_--;

    RTTY_DEBUG("RTTY: Buffer read: '%c' Count: %d, R:%d, W:%d\n", c, charBufferCount_, charBufferReadPos_, charBufferWritePos_);

    return c;
}

/**
 * @brief Az RTTY dekóder fő ciklikus feldolgozó függvénye
 *
 * Ez a Core1-en futó main loop, amely folyamatosan:
 * - Mintavételezi az audio jelet és detektálja az RTTY hangokat
 * - Automatikusan detektálja a shift frekvenciát és baud rate-et
 * - Dekódolja az RTTY karaktereket Baudot kódból ASCII-ra
 * - Kezeli a LTRS/FIGS shift állapotokat
 * - Puffereli a dekódolt karaktereket a Core0 számára
 */
void RttyDecoder::updateDecoder() {
    unsigned long currentTime = millis();

    // Automatikus detektálás időtúllépés ellenőrzése
    if (autoDetectActive_ && (currentTime - autoDetectStartMs_) > AUTO_DETECT_TIMEOUT_MS) {
        autoDetectActive_ = false;
        RTTY_DEBUG("RTTY: Auto-detect timeout, using default settings\n");
    }

    // Hangállapot detektálása
    if (!detectToneState()) {
        return;  // Nincs érvényes jel
    }

    // Baud rate detektálás frissítése
    if (autoDetectActive_) {
        updateBaudRateDetection();

        // Baud rate becslés frissítése
        if ((currentTime - autoDetectStartMs_) > 2000) {  // 2 sec után
            unsigned long estimatedBaud = estimateBaudRate();
            if (estimatedBaud != detectedBaudRate_) {
                detectedBaudRate_ = estimatedBaud;
                bitDurationMs_ = 1000 / detectedBaudRate_;
                RTTY_DEBUG("RTTY: Baud rate updated to %lu (bit duration: %lu ms)\n", detectedBaudRate_, bitDurationMs_);
            }
        }
    }

    // RTTY állapotgép időzítési logikája
    switch (currentState_) {
        case IDLE:
            // Start bit (Space) keresése
            if (!currentToneState_) {  // Space jel
                currentState_ = WAITING_FOR_START_BIT;
                startBitTimeMs_ = currentTime;
                RTTY_DEBUG("RTTY: Potential start bit detected at %lu ms\n", currentTime);
            }
            break;

        case WAITING_FOR_START_BIT:
            // Start bit megerősítése (fél bit idő után)
            if (currentTime - startBitTimeMs_ >= (bitDurationMs_ / 2)) {
                if (!currentToneState_) {  // Még mindig Space
                    currentState_ = RECEIVING_DATA_BITS;
                    bitsReceived_ = 0;
                    currentByte_ = 0;
                    lastBitTimeMs_ = startBitTimeMs_ + bitDurationMs_;  // Első data bit időpontja
                    RTTY_DEBUG("RTTY: Start bit confirmed, first data bit at %lu ms\n", lastBitTimeMs_);
                } else {
                    // Téves start bit
                    currentState_ = IDLE;
                    RTTY_DEBUG("RTTY: False start bit, back to IDLE\n");
                }
            }
            break;

        case RECEIVING_DATA_BITS:
            // Data bit mintavételezése a bit közepén
            if (currentTime >= lastBitTimeMs_) {
                // Bit mintavételezése
                if (currentToneState_) {  // Mark = 1
                    currentByte_ |= (1 << bitsReceived_);
                }
                bitsReceived_++;

                RTTY_DEBUG("RTTY: Data bit %d: %s (byte: 0x%02X) at %lu ms\n", bitsReceived_, currentToneState_ ? "Mark" : "Space", currentByte_, currentTime);

                if (bitsReceived_ >= 5) {  // 5 data bits received
                    currentState_ = RECEIVING_STOP_BIT;
                    lastBitTimeMs_ += bitDurationMs_;  // Stop bit időpontja
                    RTTY_DEBUG("RTTY: 5 data bits received, stop bit expected at %lu ms\n", lastBitTimeMs_);
                } else {
                    lastBitTimeMs_ += bitDurationMs_;  // Következő data bit időpontja
                }
            }
            break;

        case RECEIVING_STOP_BIT: {
            // 5 minta a stop bit periódusán belül, többségi szavazás
            int markCount = 0;
            const int sampleCount = 5;
            // Fél bit periódus várakozás, hogy középre kerüljön az első minta
            delay(bitDurationMs_ / (sampleCount + 1));
            for (int i = 0; i < sampleCount; ++i) {
                if (detectToneState() && currentToneState_) markCount++;
                if (i < sampleCount - 1) {
                    delay(bitDurationMs_ / (sampleCount + 1));  // Egyenletesen elosztva
                }
            }
            if (markCount >= 3) {  // Legalább 3 Mark: érvényes stop bit
                char decodedChar = decodeBaudotCharacter(currentByte_);
                if (decodedChar != '\0') {
                    addToBuffer(decodedChar);
                    RTTY_DEBUG("RTTY: Character decoded: '%c' (0x%02X) at %lu ms [5-sample stop bit]\n", decodedChar, currentByte_, millis());
                }
            } else {
                RTTY_DEBUG("RTTY: Invalid stop bit (Space, majority, 5-sample) at %lu ms, discarding character 0x%02X\n", millis(), currentByte_);
            }
            // Reset for next character
            resetRttyStateMachine();
            break;
        }
    }
}
