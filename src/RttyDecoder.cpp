#include "RttyDecoder.h"

#include <cmath>  // abs, round, constrain

RttyDecoder::RttyDecoder(AudioProcessor& audioProcessor) : audioProcessor(audioProcessor), figsShift(false) { initBaudotToAscii(); }

RttyDecoder::~RttyDecoder() {}

char RttyDecoder::decodeNextCharacter() {
    if (!&audioProcessor) return '\0';

    // Audio feldolgozás (FFT)
    audioProcessor.process(false);  // Nincs szükség oszcilloszkóp mintákra
    const double* magnitudeData = audioProcessor.getMagnitudeData();
    float binWidthHz = audioProcessor.getBinWidthHz();
    if (binWidthHz == 0) return '\0';  // Hiba elkerülése
    if (!magnitudeData) return '\0';   // Biztonsági ellenőrzés

    // Mark és Space frekvenciákhoz tartozó FFT bin indexek kiszámítása
    int markBin = static_cast<int>(round(RTTY_MARK_FREQ_HZ / binWidthHz));
    int spaceBin = static_cast<int>(round(RTTY_SPACE_FREQ_HZ / binWidthHz));

    // Biztosítjuk, hogy a bin indexek a magnitúdó tömb határain belül legyenek
    markBin = constrain(markBin, 0, AudioProcessorConstants::FFT_SAMPLES / 2 - 1);
    spaceBin = constrain(spaceBin, 0, AudioProcessorConstants::FFT_SAMPLES / 2 - 1);

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
                lastBitTime = currentTime;  // Rögzítjük a potenciális start bit kezdetének idejét
                DEBUG("RTTY: Potential start bit detected.\n");
            } else if (!isSignalPresent) {
                // Nincs jel, maradjunk IDLE állapotban, vagy resetelhetnénk, ha szükséges
            }
            break;

        case WAITING_FOR_START_BIT:
            // Ellenőrizzük, hogy a jel továbbra is Space ÉS jelen van-e
            if (isSignalPresent && !isMarkTone) {
                if (currentTime - lastBitTime >= sampleOffsetMs) {
                    // Start bit megerősítve, felkészülés az adatbitek vételére
                    currentState = RECEIVING_DATA_BIT;
                    bitsReceived = 0;
                    currentByte = 0;
                    lastBitTime = currentTime;  // Időzítő resetelése az első adatbithez
                    DEBUG("RTTY: Start bit confirmed. Receiving data bits.\n");
                }
            } else {
                // A jel megváltozott Mark-ra, vagy eltűnt. Téves indítás, visszatérés IDLE állapotba.
                currentState = IDLE;
                DEBUG("RTTY: False start bit or signal lost, returning to IDLE.\n");
            }
            break;

        case RECEIVING_DATA_BIT:
            // Ellenőrizzük, hogy ideje-e mintavételezni a következő adatbitet
            if (currentTime - lastBitTime >= bitDurationMs) {
                lastBitTime += bitDurationMs;  // Időzítő előre léptetése a következő bithez

                // Mintavételezés (ellenőrizzük, hogy Mark-e)
                // Ha a jel eltűnt volna az adatbitek közben, isSignalPresent false lenne.
                // Ebben az egyszerűsített dekóderben feltételezzük, hogy a jel stabil marad a bájt végéig.
                // Egy robusztusabb dekóder itt is ellenőrizné az isSignalPresent-et és hibát jelezne/resetelne.
                // Most az utolsó ismert isMarkTone-t használjuk, ha a jel esetleg gyenge/zajos lenne.
                // Ideális esetben minden bit mintavételezés előtt frissítenénk az isSignalPresent és isMarkTone értékeket.
                // De az audioProcessor.process() hívása minden bitnél túl lassú lehet.
                uint8_t bit = isMarkTone ? 1 : 0;

                // Bit tárolása az aktuális bájtban (Baudot LSB először)
                currentByte |= (bit << bitsReceived);
                bitsReceived++;

                DEBUG("RTTY: Received bit %d: %d (currentByte: %02X)\n", bitsReceived, bit, currentByte);

                if (bitsReceived == 5) {
                    // Mind az 5 adatbitet megkaptuk, átlépés a stop bit állapotba
                    currentState = RECEIVING_STOP_BIT;
                    // lastBitTime már be van állítva a stop bit időtartamának kezdetére
                    DEBUG("RTTY: Received 5 data bits. Waiting for stop bit.\n");
                }
            }
            // Ha a jel eltűnik az adatbitek vétele közben:
            if (!isSignalPresent && bitsReceived > 0 && bitsReceived < 5) {
                currentState = IDLE;
                DEBUG("RTTY: Signal lost during data bits. Resetting.\n");
                return '\0';
            }
            // Egy igazi dekódernek folyamatosan figyelnie kellene a jelet és a bitidőzítést.
            break;

        case RECEIVING_STOP_BIT:
            // Ellenőrizzük, hogy eltelt-e a szükséges stop bit időtartam (1.5 vagy 2 bit idő)
            // Standard RTTY 1.5 stop bitet használ. Ellenőrizzük legalább 1.5 bit időtartamot.
            if (currentTime - lastBitTime >= bitDurationMs * 1.5f) {
                // Ellenőrizzük, hogy a jel Mark-e a stop bit időtartama alatt (opcionális, de jó gyakorlat)
                // Ez az egyszerű ellenőrzés csak az állapotot nézi az időtartam végén.
                // Itt is az utolsó ismert isMarkTone-t használjuk.
                bool stopBitIsMark = isMarkTone;

                if (!isSignalPresent) {  // Ha a jel eltűnt a stop bit alatt
                    currentState = IDLE;
                    DEBUG("RTTY: Signal lost during stop bit. Resetting.\n");
                    return '\0';
                }

                if (stopBitIsMark) {  // Stop bit Mark (ahogy várható)
                    // Stop bit megérkezett, dekódoljuk a bájtot
                    char decodedChar = decodeBaudot(currentByte, figsShift);

                    // Visszaállítás IDLE állapotba a következő karakterhez
                    currentState = IDLE;
                    DEBUG("RTTY: Stop bit received. Decoded char: '%c' (0x%02X). Returning to IDLE.\n", decodedChar, currentByte);
                    return decodedChar;
                } else {
                    // Stop bit nem volt Mark, potenciális hiba vagy zaj, visszaállítás
                    currentState = IDLE;
                    DEBUG("RTTY: Stop bit not detected (not Mark). Resetting to IDLE.\n");
                }
            }
            // Megjegyzés: Itt sem ellenőrizzük a signalPresent-et.
            break;
    }
    return '\0';  // Nincs kész karakter
}

// Ez a függvény frissíti az isSignalPresent és isMarkTone tagváltozókat
// a jelenlegi audio adatok alapján.
void RttyDecoder::getSignalState(bool& outIsSignalPresent, bool& outIsMarkTone) {
    // Az audioProcessor.process() már lefutott a decodeNextCharacter() elején.
    const double* magnitudeData = audioProcessor.getMagnitudeData();
    float binWidthHz = audioProcessor.getBinWidthHz();
    if (binWidthHz == 0 || !magnitudeData) {
        outIsSignalPresent = false;
        outIsMarkTone = false;  // Irreleváns, de legyen definiált
        return;
    }

    // Mark és Space frekvenciákhoz tartozó FFT bin indexek kiszámítása
    int markBin = static_cast<int>(round(RTTY_MARK_FREQ_HZ / binWidthHz));
    int spaceBin = static_cast<int>(round(RTTY_SPACE_FREQ_HZ / binWidthHz));

    // Biztosítjuk, hogy a bin indexek a magnitúdó tömb határain belül legyenek
    markBin = constrain(markBin, 0, AudioProcessorConstants::FFT_SAMPLES / 2 - 1);
    spaceBin = constrain(spaceBin, 0, AudioProcessorConstants::FFT_SAMPLES / 2 - 1);

    // Mark és Space energiák összehasonlítása (egyszerű megközelítés)
    // Egy robusztusabb dekóder itt valószínűleg szűrné vagy átlagolná az értékeket.
    double currentMarkMagnitude = magnitudeData[markBin];
    double currentSpaceMagnitude = magnitudeData[spaceBin];

    // Meghatározzuk, hogy Mark vagy Space a domináns jel
    outIsMarkTone = currentMarkMagnitude > currentSpaceMagnitude;

    // Egyszerű jel jelenlét ellenőrzés (elkerülendő a zaj dekódolását)
    // Állítsuk be ezt a küszöböt a tesztelés alapján.
    const double SIGNAL_THRESHOLD = 100.0;  // Példa küszöb, finomhangolást igényel
    outIsSignalPresent = (currentMarkMagnitude > SIGNAL_THRESHOLD || currentSpaceMagnitude > SIGNAL_THRESHOLD);

    if (!outIsSignalPresent) {
        outIsMarkTone = false;  // Ha nincs jel, a Mark/Space állapot irreleváns vagy legyen default Space
    }
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