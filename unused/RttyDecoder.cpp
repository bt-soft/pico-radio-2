#include "RttyDecoder.h"

#include <algorithm>  // std::sort, std::max, std::min
#include <cmath>      // abs, round, constrain

RttyDecoder::RttyDecoder(AudioProcessor& audioProcessor)
    : audioProcessor(audioProcessor),
      figsShift_(false),
      rttyMarkFreqHz_(2295.0f),  // Alapértelmezett Mark (gyakran a magasabb)
      rttySpaceFreqHz_(2125.0f)  // Alapértelmezett Space (gyakran az alacsonyabb)
{
    initBaudotToAscii();
}

RttyDecoder::~RttyDecoder() {}

char RttyDecoder::decodeNextCharacter() {
    if (!&audioProcessor) return '\0';

    // Audio feldolgozás (FFT)
    audioProcessor.process(false);  // Nincs szükség oszcilloszkóp mintákra
    const double* magnitudeData = audioProcessor.getMagnitudeData();
    float binWidthHz = audioProcessor.getBinWidthHz();
    if (binWidthHz == 0) return '\0';  // Hiba elkerülése
    if (!magnitudeData) return '\0';   // Biztonsági ellenőrzés (bár process() után elvileg nem lehet null)

    if (autoDetectModeActive_) {
        if (attemptAutoDetectFrequencies()) {
            DEBUG("RTTY: Auto-detect successful. Mark: %.1f Hz, Space: %.1f Hz\n", rttyMarkFreqHz_, rttySpaceFreqHz_);
            autoDetectModeActive_ = false;
            // Reset RTTY state machine as frequencies changed
            currentState = IDLE;
            figsShift_ = false;  // Reset figs shift
            return '\0';         // Don't decode a char immediately after successful detection
        } else if (millis() - autoDetectStartTime_ > AUTO_DETECT_DURATION_MS) {
            DEBUG("RTTY: Auto-detect timed out. Using default/previous frequencies (Mark: %.1f, Space: %.1f).\n", rttyMarkFreqHz_, rttySpaceFreqHz_);
            autoDetectModeActive_ = false;
            // Optionally, revert to default frequencies if they were changed optimistically
            // For now, we keep whatever was last set or the defaults.
        } else {
            return '\0';  // Still trying to auto-detect
        }
    }

    // Mark és Space frekvenciákhoz tartozó FFT bin indexek kiszámítása
    // Ezeket a getSignalState már használja a frissített rttyMarkFreqHz_ és rttySpaceFreqHz_ alapján.
    // Itt közvetlenül nem használjuk őket, a getSignalState szolgáltatja az eredményt.

    // RTTY dekódolási állapotgép (nagyon egyszerűsített)
    unsigned long currentTime = millis();
    float bitDurationMs = 1000.0f / RTTY_BAUD_RATE;
    float sampleOffsetMs = bitDurationMs / 2.0f;  // Mintavételezési offset a bit közepére (kb. fél bit idő)

    bool isSignalPresent;
    bool isMarkTone;  // True if Mark, false if Space (érvényes, ha isSignalPresent true)
    getSignalState(isSignalPresent, isMarkTone);

    switch (currentState) {
        case IDLE:
            // Várakozás egy potenciális start bitre (Space jel)
            if (isSignalPresent && !isMarkTone) {  // Ha van jel ÉS az Space
                // Potenciális start bit detektálva, átmenet a várakozó állapotba
                currentState = WAITING_FOR_START_BIT;
                lastBitTime = currentTime;
                DEBUG("RTTY: Potential start bit detected.\n");
            } else if (!isSignalPresent) {
                // Nincs jel, maradjunk IDLE állapotban, vagy resetelhetnénk, ha szükséges
            }
            break;

        case WAITING_FOR_START_BIT:
            // Ellenőrizzük, hogy a jel továbbra is Space ÉS jelen van-e
            // Itt újra lekérdezzük a jel állapotát, hogy a sampleOffsetMs idő alatti változást is figyelembe vegyük
            getSignalState(isSignalPresent, isMarkTone);  // Friss állapot lekérdezése
            if (isSignalPresent && !isMarkTone) {
                if (currentTime - lastBitTime >= sampleOffsetMs) {
                    // Start bit megerősítve, felkészülés az adatbitek vételére
                    currentState = RECEIVING_DATA_BIT;
                    bitsReceived = 0;
                    currentByte = 0;
                    lastBitTime = currentTime;  // Időzítő resetelése az első adatbithez
                    DEBUG("RTTY: Start bit confirmed. Receiving data bits.\n");
                }
                // Ha az idő még nem telt el, nem csinálunk semmit, várunk a következő ciklusra.
            } else {
                // A jel megváltozott Mark-ra, vagy eltűnt. Téves indítás, visszatérés IDLE állapotba.
                currentState = IDLE;
                DEBUG("RTTY: False start bit or signal lost (during wait), returning to IDLE.\n");
            }
            break;

        case RECEIVING_DATA_BIT:
            // Ellenőrizzük, hogy ideje-e mintavételezni a következő adatbitet
            if (currentTime - lastBitTime >= bitDurationMs) {
                lastBitTime += bitDurationMs;  // Időzítő előre léptetése a következő bithez

                // Újra lekérdezzük a jel állapotát minden bit vétele előtt
                bool currentBitSignalPresent;
                bool currentBitIsMark;
                getSignalState(currentBitSignalPresent, currentBitIsMark);

                if (!currentBitSignalPresent) {  // Ha a jel elveszett a bit vétele közben
                    DEBUG("RTTY: Signal lost during data bit %d. Resetting.\n", bitsReceived);
                    currentState = IDLE;
                    return '\0';
                }
                uint8_t bit = currentBitIsMark ? 1 : 0;

                // Bit tárolása az aktuális bájtban (Baudot LSB először)
                currentByte |= (bit << bitsReceived);
                bitsReceived++;

                DEBUG("RTTY: Received bit %d: %d (currentByte: %02X)\n", bitsReceived, bit, currentByte);

                if (bitsReceived == 5) {
                    // Mind az 5 adatbitet megkaptuk, átlépés a stop bit állapotba
                    currentState = RECEIVING_STOP_BIT;  // Nincs teendő, a lastBitTime már jó
                    // lastBitTime már be van állítva a stop bit időtartamának kezdetére
                    DEBUG("RTTY: Received 5 data bits. Waiting for stop bit.\n");
                }
            }
            // Egy igazi dekódernek folyamatosan figyelnie kellene a jelet és a bitidőzítést.
            break;

        case RECEIVING_STOP_BIT:
            // Ellenőrizzük, hogy eltelt-e a szükséges stop bit időtartam (1.5 vagy 2 bit idő)
            // Standard RTTY 1.5 stop bitet használ. Ellenőrizzük legalább 1.5 bit időtartamot.
            if (currentTime - lastBitTime >= bitDurationMs * 1.5f) {
                // Újra lekérdezzük a jel állapotát a stop bit ellenőrzéséhez
                bool currentStopBitSignalPresent;
                bool currentStopBitIsMark;
                getSignalState(currentStopBitSignalPresent, currentStopBitIsMark);

                if (!currentStopBitSignalPresent) {  // Ha a jel eltűnt a stop bit alatt
                    currentState = IDLE;
                    DEBUG("RTTY: Signal lost during stop bit. Resetting.\n");
                    return '\0';
                }

                if (currentStopBitIsMark) {  // Stop bit Mark (ahogy várható)
                    // Stop bit megérkezett, dekódoljuk a bájtot
                    char decodedChar = decodeBaudot(currentByte, figsShift_);

                    // Visszaállítás IDLE állapotba a következő karakterhez
                    currentState = IDLE;
                    DEBUG("RTTY: Stop bit received. Decoded char: '%c' (0x%02X). Returning to IDLE.\n", decodedChar, currentByte);
                    return decodedChar;
                } else {
                    // Stop bit nem volt Mark, potenciális hiba vagy zaj, visszaállítás
                    DEBUG("RTTY: Stop bit not detected (not Mark). Resetting to IDLE.\n");
                    currentState = IDLE;
                }
            }
            break;
    }
    return '\0';  // Nincs kész karakter
}

// Automatikus frekvencia detektálás indítása
void RttyDecoder::startAutoDetect() {
    autoDetectModeActive_ = true;
    autoDetectStartTime_ = millis();
    currentState = IDLE;  // Reset state machine
    figsShift_ = false;
    DEBUG("RTTY: Starting automatic frequency detection...\n");
}

void RttyDecoder::setMarkFrequencyIsLower(bool isLower) {
    markFrequencyIsLower_ = isLower;
    DEBUG("RTTY: Mark frequency set to be %s.\n", isLower ? "lower" : "higher");
}

bool RttyDecoder::attemptAutoDetectFrequencies() {
    const double* magnitudeData = audioProcessor.getMagnitudeData();
    float binWidthHz = audioProcessor.getBinWidthHz();

    if (binWidthHz == 0 || !magnitudeData) return false;
    std::vector<PeakInfo> peaks;
    int min_bin = static_cast<int>(round(MIN_RTTY_FREQ_HZ_FOR_DETECT / binWidthHz));
    int max_bin = static_cast<int>(round(MAX_RTTY_FREQ_HZ_FOR_DETECT / binWidthHz));
    int fftSize = audioProcessor.getFftSize();
    min_bin = constrain(min_bin, 1, fftSize / 2 - 2);
    max_bin = constrain(max_bin, min_bin, fftSize / 2 - 2);

    for (int i = min_bin; i <= max_bin; ++i) {
        if (magnitudeData[i] > magnitudeData[i - 1] && magnitudeData[i] > magnitudeData[i + 1] && magnitudeData[i] > AUTO_DETECT_MIN_PEAK_MAGNITUDE) {
            peaks.push_back({i * binWidthHz, magnitudeData[i]});
        }
    }

    if (peaks.size() < 2) return false;

    // Csúcsok rendezése magnitúdó szerint csökkenő sorrendben
    std::sort(peaks.begin(), peaks.end(), [](const PeakInfo& a, const PeakInfo& b) { return a.mag > b.mag; });

    // Legfeljebb az 5 legerősebb csúcsot vizsgáljuk páronként
    int peaks_to_check = std::min((int)peaks.size(), 5);
    PeakInfo best_peak1 = {0, 0}, best_peak2 = {0, 0};
    double max_combined_mag = 0;
    bool found_pair = false;

    for (int i = 0; i < peaks_to_check; ++i) {
        for (int j = i + 1; j < peaks_to_check; ++j) {
            float freq_diff = std::abs(peaks[i].freq - peaks[j].freq);
            // DEBUG("RTTY_AD: Comparing %.1fHz (%.1f) & %.1fHz (%.1f), diff=%.1fHz. TargetShift=%.1f, Tol=%.1f\n",
            //       peaks[i].freq, peaks[i].mag, peaks[j].freq, peaks[j].mag, freq_diff, RTTY_TARGET_SHIFT_HZ, AUTO_DETECT_SHIFT_TOLERANCE_HZ);
            if (std::abs(freq_diff - RTTY_TARGET_SHIFT_HZ) < AUTO_DETECT_SHIFT_TOLERANCE_HZ) {
                // Új feltétel: a két csúcs magnitúdója ne legyen túlságosan eltérő
                double mag_ratio = (peaks[i].mag > peaks[j].mag) ? (peaks[i].mag / std::max(0.001, peaks[j].mag)) : (peaks[j].mag / std::max(0.001, peaks[i].mag));
                if (mag_ratio < AUTO_DETECT_MAX_MAG_RATIO) {
                    double combined_mag = peaks[i].mag + peaks[j].mag;
                    if (combined_mag > max_combined_mag) {
                        max_combined_mag = combined_mag;
                        best_peak1 = peaks[i];
                        best_peak2 = peaks[j];
                        found_pair = true;
                        // DEBUG("RTTY_AD:   New best pair (ratio %.1f): %.1f Hz & %.1f Hz, combined_mag=%.1f\n", mag_ratio, best_peak1.freq, best_peak2.freq, max_combined_mag);
                    }
                } else {
                    // DEBUG("RTTY_AD:   Pair rejected due to high mag ratio: %.1f (%.1fHz vs %.1fHz)\n", mag_ratio, peaks[i].freq, peaks[j].freq);
                }
            }
        }
    }

    if (found_pair) {
        if (markFrequencyIsLower_) {
            rttyMarkFreqHz_ = std::min(best_peak1.freq, best_peak2.freq);
            rttySpaceFreqHz_ = std::max(best_peak1.freq, best_peak2.freq);
        } else {
            rttyMarkFreqHz_ = std::max(best_peak1.freq, best_peak2.freq);
            rttySpaceFreqHz_ = std::min(best_peak1.freq, best_peak2.freq);
        }
        return true;
    }

    return false;
}

// Ez a függvény frissíti az isSignalPresent és isMarkTone tagváltozókat
// a jelenlegi audio adatok alapján.
void RttyDecoder::getSignalState(bool& outIsSignalPresent, bool& outIsMarkTone) {
    // Az audioProcessor.process() már lefutott a decodeNextCharacter() elején.
    const double* magnitudeData = audioProcessor.getMagnitudeData();
    float binWidthHz = audioProcessor.getBinWidthHz();

    outIsSignalPresent = false;  // Alapértelmezetten nincs jel
    outIsMarkTone = false;       // Alapértelmezetten Space, ha a jel nem egyértelmű

    if (binWidthHz == 0 || !magnitudeData) {
        DEBUG("RTTY_AD: No binWidth (%.2f) or magnitudeData (%p)\n", binWidthHz, magnitudeData);
        return;
    }

    // Mark és Space frekvenciákhoz tartozó FFT bin indexek kiszámítása
    int markCenterBin = static_cast<int>(round(rttyMarkFreqHz_ / binWidthHz));
    int spaceCenterBin = static_cast<int>(round(rttySpaceFreqHz_ / binWidthHz));

    // Helper lambda a magnitúdók átlagolásához NUM_BINS_TO_AVERAGE szélességben
    auto getAverageMagnitude = [&](int centerBin) {
        double sumMag = 0;
        int count = 0;
        int halfWindow = (NUM_BINS_TO_AVERAGE - 1) / 2;
        for (int offset = -halfWindow; offset <= halfWindow; ++offset) {
            int bin = centerBin + offset;
            // Biztosítjuk, hogy a bin indexek a magnitúdó tömb határain belül legyenek
            int fftSize = audioProcessor.getFftSize();
            bin = constrain(bin, 0, fftSize / 2 - 1);
            sumMag += magnitudeData[bin];
            count++;
        }
        return (count > 0) ? (sumMag / count) : 0.0;
    };

    double avgMarkMagnitude = getAverageMagnitude(markCenterBin);
    double avgSpaceMagnitude = getAverageMagnitude(spaceCenterBin);

    bool markStrongEnough = avgMarkMagnitude > MIN_TONE_MAGNITUDE_THRESHOLD;
    bool spaceStrongEnough = avgSpaceMagnitude > MIN_TONE_MAGNITUDE_THRESHOLD;
    bool significantDifference = std::abs(avgMarkMagnitude - avgSpaceMagnitude) > TONE_DIFFERENCE_THRESHOLD;

    if ((markStrongEnough || spaceStrongEnough) && significantDifference) {
        outIsSignalPresent = true;
        outIsMarkTone = avgMarkMagnitude > avgSpaceMagnitude;
    }
    DEBUG("RTTY SignalState: MarkAvg=%.1f, SpaceAvg=%.1f, Present=%d, IsMark=%d (M:%.1f, S:%.1f)\n", avgMarkMagnitude, avgSpaceMagnitude, outIsSignalPresent, outIsMarkTone,
          rttyMarkFreqHz_, rttySpaceFreqHz_);
}

// Baudot -> ASCII tábla inicializálása
void RttyDecoder::initBaudotToAscii() {
    // LTRS (Letters) Shift
    baudotToAscii[0][0] = '\0';  // Null (ITA2 Blank / CCITT2 #0)
    baudotToAscii[0][1] = 'E';
    baudotToAscii[0][2] = '\n';  // Line Feed
    baudotToAscii[0][3] = 'A';
    baudotToAscii[0][4] = ' ';  // Space
    baudotToAscii[0][5] = 'S';
    baudotToAscii[0][6] = 'I';
    baudotToAscii[0][7] = 'U';
    baudotToAscii[0][8] = '\r';  // Carriage Return
    baudotToAscii[0][9] = 'D';
    baudotToAscii[0][10] = 'R';
    baudotToAscii[0][11] = 'J';
    baudotToAscii[0][12] = 'N';
    baudotToAscii[0][13] = 'F';
    baudotToAscii[0][14] = 'C';
    baudotToAscii[0][15] = 'K';
    baudotToAscii[0][16] = 'T';
    baudotToAscii[0][17] = 'Z';
    baudotToAscii[0][18] = 'L';
    baudotToAscii[0][19] = 'W';
    baudotToAscii[0][20] = 'H';
    baudotToAscii[0][21] = 'Y';
    baudotToAscii[0][22] = 'P';
    baudotToAscii[0][23] = 'Q';
    baudotToAscii[0][24] = 'O';
    baudotToAscii[0][25] = 'B';
    baudotToAscii[0][26] = 'G';
    baudotToAscii[0][27] = '\0';  // FIGS (Shift to Figures)
    baudotToAscii[0][28] = 'M';
    baudotToAscii[0][29] = 'X';
    baudotToAscii[0][30] = 'V';
    baudotToAscii[0][31] = '\0';  // LTRS (Shift to Letters)

    // FIGS (Figures) Shift
    baudotToAscii[1][0] = '\0';  // Null (ITA2 Blank / CCITT2 #0)
    baudotToAscii[1][1] = '3';
    baudotToAscii[1][2] = '\n';  // Line Feed
    baudotToAscii[1][3] = '-';
    baudotToAscii[1][4] = ' ';   // Space
    baudotToAscii[1][5] = '\'';  // Apostrophe (US) or BELL (ITA2)
    baudotToAscii[1][6] = '8';
    baudotToAscii[1][7] = '7';
    baudotToAscii[1][8] = '\r';  // Carriage Return
    baudotToAscii[1][9] = '$';   // Dollar (US) or Who Are You? (WRU) / Bell (CCITT2 #9)
    baudotToAscii[1][10] = '4';
    baudotToAscii[1][11] = '\a';  // Bell (US) or "'" (CCITT2 #11)
    baudotToAscii[1][12] = ',';
    baudotToAscii[1][13] = '!';  // Exclamation mark (US) or "(" (CCITT2 #13)
    baudotToAscii[1][14] = ':';  // Colon (US) or ")" (CCITT2 #14)
    baudotToAscii[1][15] = '(';  // Parenthesis (US) or "." (CCITT2 #15)
    baudotToAscii[1][16] = '5';
    baudotToAscii[1][17] = '+';  // Plus (US) or "=" (CCITT2 #17)
    baudotToAscii[1][18] = '.';  // Period (US) or "/" (CCITT2 #18)
    baudotToAscii[1][19] = '2';
    baudotToAscii[1][20] = '#';  // Hash (US) or "'" (CCITT2 #20)
    baudotToAscii[1][21] = '6';
    baudotToAscii[1][22] = '0';
    baudotToAscii[1][23] = '1';
    baudotToAscii[1][24] = '9';
    baudotToAscii[1][25] = '?';
    baudotToAscii[1][26] = '&';   // Ampersand (US) or "+" (CCITT2 #26)
    baudotToAscii[1][27] = '\0';  // FIGS (Shift to Figures)
    baudotToAscii[1][28] = '.';   // Period (US) or "," (CCITT2 #28)
    baudotToAscii[1][29] = '/';   // Slash (US) or ";" (CCITT2 #29)
    baudotToAscii[1][30] = ';';   // Semicolon (US) or "?" (CCITT2 #30)
    baudotToAscii[1][31] = '\0';  // LTRS (Shift to Letters)
}

char RttyDecoder::decodeBaudot(uint8_t byte, bool& currentFigsShift) {
    // Baudot kód 5 bit, így a bájt értéke 0-31.
    if (byte >= 32) return '\0';  // Nem fordulhatna elő 5 bittel, de biztonsági ellenőrzés

    // Shift karakterek kezelése
    if (byte == 27) {  // FIGS (Shift to Figures) - ITA2 #27
        currentFigsShift = true;
        return '\0';  // A shift karakterek nem kerülnek kinyomtatásra
    }
    if (byte == 31) {  // LTRS (Shift to Letters) - ITA2 #31
        currentFigsShift = false;
        return '\0';  // A shift karakterek nem kerülnek kinyomtatásra
    }

    // Dekódolás az aktuális shift állapot alapján
    return baudotToAscii[currentFigsShift ? 1 : 0][byte];
}