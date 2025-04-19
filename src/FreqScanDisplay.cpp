#include "FreqScanDisplay.h"

#include <Arduino.h>

#include <cmath>  // std::pow, round, fmod használatához

/**
 * Konstruktor
 */
FreqScanDisplay::FreqScanDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band) {

    DEBUG("FreqScanDisplay::FreqScanDisplay\n");

    // Vektorok inicializálása a megfelelő méretre
    // Kezdetben a spektrum aljával (max Y) töltjük fel, ami a minimális jelet jelenti
    scanValueRSSI.resize(spectrumWidth, spectrumEndY);
    scanValueSNR.resize(spectrumWidth, 0);
    scanMark.resize(spectrumWidth, false);
    scanScaleLine.resize(spectrumWidth, 0);

    // Szkenneléshez releváns gombok definiálása
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"Start", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},  {"Stop", TftButton::ButtonType::Pushable, TftButton::ButtonState::Disabled},  // Kezdetben tiltva
        {"Pause", TftButton::ButtonType::Toggleable, TftButton::ButtonState::On},  // Kezdetben szünetel
        {"Scale", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},  {"Back", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
    };

    // Létrehozzuk a CSAK ehhez a képernyőhöz tartozó gombokat.
    // A kötelező gombokat (pl. Scan) a Base osztály kezeli, itt nem kellenek.
    buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), false);

    // Vertikális gombok (a Base osztályból jönnek a kötelezőek)
    buildVerticalScreenButtons(nullptr, 0, false);  // Nincs egyedi vertikális gombunk, nem kellenek a defaultak sem
}

/**
 * Destruktor
 */
FreqScanDisplay::~FreqScanDisplay() {
    DEBUG("FreqScanDisplay::~FreqScanDisplay\n");
    // A vektorok automatikusan felszabadulnak.
}

/**
 * Képernyő kirajzolása
 */
void FreqScanDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);
    tft.setTextFont(2);  // Vagy a használni kívánt font

    // Státuszsor kirajzolása (örökölt)
    dawStatusLine();

    // Gombok kirajzolása (örökölt)
    drawScreenButtons();

    // Szkenneléshez szükséges kezdeti értékek beállítása az aktuális sávból
    BandTable &currentBand = band.getCurrentBand();
    startFrequency = currentBand.pConstData->minimumFreq;
    endFrequency = currentBand.pConstData->maximumFreq;

    // --- MÓDOSÍTÁS KEZDETE: Teljes sáv és középre igazítás ---

    // 1. scanStep kiszámítása, hogy a teljes sáv elférjen
    //    (Figyelem: Ha a sáv túl széles, a scanStep túl kicsi lehet a minScanStep-hez képest)
    scanStep = static_cast<float>(endFrequency - startFrequency) / static_cast<float>(spectrumWidth);
    // Opcionális: Korlátozás a min/max értékekre, de ez megakadályozhatja a teljes sáv megjelenítését
    scanStep = std::max(minScanStep, std::min(maxScanStep, scanStep));

    // 2. Sáv közepének kiszámítása
    float bandCenterFreq = static_cast<float>(startFrequency) + (static_cast<float>(endFrequency - startFrequency) / 2.0f);

    // 3. Kezdő frekvencia (kurzor) és szkennelési frekvencia beállítása a sáv közepére
    currentFrequency = static_cast<uint16_t>(round(bandCenterFreq));
    currentFrequency = constrain(currentFrequency, startFrequency, endFrequency);  // Biztosítjuk a sávhatárokat
    posScanFreq = currentFrequency;                                                // A szkennelés is innen indulna, de ezt a startScan() felülírja

    // 4. deltaScanLine kiszámítása, hogy a bandCenterFreq a képernyő közepére kerüljön
    //    deltaScanLine = (freqAtCenter - startFrequency) / scanStep;
    if (scanStep != 0) {
        deltaScanLine = (bandCenterFreq - static_cast<float>(startFrequency)) / scanStep;
    } else {
        deltaScanLine = 0;
    }

    // 5. Kezdő kurzor pozíció a spektrum közepén
    currentScanLine = spectrumX + spectrumWidth / 2.0f;

    // --- MÓDOSÍTÁS VÉGE ---
    scanEmpty = true;
    scanPaused = true;  // Kezdetben szünetel
    scanning = false;   // Kezdetben nem szkennelünk
    prevTouchedX = -1;
    // prevRssiY = spectrumEndY; // Már nem használt

    // Spektrum alapjának és szövegeinek kirajzolása
    drawScanGraph(true);  // true = törölje a korábbi adatokat
    drawScanText(true);   // true = minden szöveget rajzoljon ki

    // Kurzor (kezdeti pozíció) - piros vonal, ha szünetel
    redrawCursors();  // Ez kiszámolja és kirajzolja a piros kurzort

    // Aktuális frekvencia kiírása (kezdeti)
    // drawScanText(false); // A drawScanText(true) már kiírta

    // Aktuális RSSI/SNR kiírása (kezdeti)
    displayScanSignal();
}

/**
 * Esemény nélküli display loop -> Szkennelés futtatása
 */
void FreqScanDisplay::displayLoop() {
    // Ha van dialóg, nem csinálunk semmit
    if (pDialog != nullptr) {
        return;
    }

    if (scanning && !scanPaused) {
        // --- Szkennelési logika ---
        int d = 0;

        // Következő pozíció kiszámítása
        // A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
        // Ebből n-re rendezve: n = (F(n) - startFrequency)/scanStep + spectrumWidth/2 - deltaScanLine
        if (scanStep != 0) {
            posScan = static_cast<int>(round((static_cast<double>(posScanFreq) - static_cast<double>(startFrequency)) / static_cast<double>(scanStep) +
                                             (static_cast<double>(spectrumWidth) / 2.0) - deltaScanLine));
        } else {
            posScan = 0;  // Hiba eset
        }
        int xPos = spectrumX + posScan;

        // --- Ellenőrzés, hogy az első posScan kiesik-e a tartományból scanEmpty esetén ---
        if (scanEmpty && (posScan < 0 || posScan >= spectrumWidth)) {
            // Nem mérünk/rajzolunk, csak lépünk a következő frekvenciára
            freqUp();
            // scanEmpty igaz marad, a következő ciklusban újra próbálkozunk
        } else {
            // --- Határellenőrzés ---
            bool setf = false;
            if (!scanEmpty) {  // Csak akkor alkalmazzuk az átugrási logikát, ha már vannak adataink
                // A scanBeginBand és scanEndBand értékeket a drawScanLine számolja ki
                if (posScan >= spectrumWidth || posScan >= scanEndBand) {
                    posScan = scanBeginBand + 1;
                    if (posScan < 0) posScan = 0;  // Biztosítjuk, hogy ne legyen negatív
                    setf = true;
                } else if (posScan < 0 || posScan <= scanBeginBand) {
                    posScan = scanEndBand - 1;
                    if (posScan >= spectrumWidth) posScan = spectrumWidth - 1;  // Biztosítjuk, hogy a határon belül legyen
                    setf = true;
                }
            }
            // --- Határellenőrzés vége ---

            if (setf) {  // Ez az ág csak akkor fut le, ha !scanEmpty és határt léptünk
                // Újrahangolás ugrás miatt
                // A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
                if (scanStep != 0) {
                    posScanFreq =
                        static_cast<uint16_t>(round(static_cast<double>(startFrequency) +
                                                    (static_cast<double>(posScan) - (static_cast<double>(spectrumWidth) / 2.0) + deltaScanLine) * static_cast<double>(scanStep)));
                } else {
                    posScanFreq = startFrequency;  // Hiba eset
                }
                posScanFreq = constrain(posScanFreq, startFrequency, endFrequency);
                setFreq(posScanFreq);
                xPos = spectrumX + posScan;  // xPos frissítése
                // Az ugrás utáni első pontot nem mérjük/rajzoljuk ebben a ciklusban,
                // a következő ciklusban a frissített posScanFreq alapján fogunk mérni.
            } else {  // Ez az ág fut le, ha scanEmpty (és határon belül voltunk) VAGY ha !scanEmpty és nem léptünk határt
                // --- Jelerősség mérése ---
                int rssi_val = getSignal(true);
                int snr_val = getSignal(false);

                // Értékek tárolása a megfelelő indexen
                // Biztosítjuk, hogy posScan érvényes legyen a vektorokhoz
                if (posScan >= 0 && posScan < spectrumWidth) {
                    scanValueRSSI[posScan] = static_cast<uint8_t>(rssi_val);
                    scanValueSNR[posScan] = static_cast<uint8_t>(snr_val);
                    scanMark[posScan] = (scanValueSNR[posScan] >= scanMarkSNR);

                    // Ha ez az első érvényes adatpont, jelezzük, hogy a spektrum már nem üres
                    if (scanEmpty) {
                        scanEmpty = false;
                    }

                    // --- Rajzolás ---
                    // xPos itt már a helyes posScan alapján van
                    drawScanLine(xPos);  // Ez már a kurzor nélküli verzió

                } else {
                    DEBUG("Error: posScan (%d) invalid for vector access in displayLoop.\n", posScan);
                }

                drawScanText(false);  // Frekvencia frissítése

                // --- Következő frekvencia ---
                freqUp();

                posScanLast = posScan;
            }
        }  // --- else ág vége (azaz az első posScan rendben volt) ---
    }  // --- if (scanning && !scanPaused) vége ---
}  // --- displayLoop vége ---

/**
 * Rotary encoder esemény lekezelése
 */
bool FreqScanDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    // Ha szünetel a szkennelés, a forgatással a kiválasztott frekvenciát hangoljuk
    if (scanPaused && encoderState.direction != RotaryEncoder::Direction::None) {
        uint16_t step = band.getCurrentBand().varData.currStep;  // Aktuális sáv lépésköze
        if (encoderState.direction == RotaryEncoder::Direction::Up) {
            currentFrequency += step;
        } else {
            currentFrequency -= step;
        }
        // Határok ellenőrzése
        currentFrequency = constrain(currentFrequency, startFrequency, endFrequency);
        setFreq(currentFrequency);  // Rádió hangolása

        // Érintés állapot törlése és kurzor újrarajzolása
        int oldPrevTouchedX = prevTouchedX;
        prevTouchedX = -1;  // Forgatás törli az érintést
        if (oldPrevTouchedX != -1) {
            eraseCursor(oldPrevTouchedX);  // Régi sárga törlése
        }
        redrawCursors();  // Újrarajzolja a piros kurzort az új helyen

        // Szövegek és jel kiírása
        drawScanText(false);  // Csak az aktuális frekvenciát
        displayScanSignal();  // RSSI/SNR frissítése

        return true;  // Kezeltük az eseményt
    }
    return false;  // Nem kezeltük az eseményt
}

/**
 * Touch (nem képernyő button) esemény lekezelése
 */
bool FreqScanDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {

    // --- Érintés a spektrumon kívül ---
    if (ty < spectrumY || ty > spectrumEndY || tx < spectrumX || tx > spectrumEndScanX) {
        if (!touched) {  // Érintés vége a spektrumon kívül
            if (prevTouchedX != -1) {
                int oldPrevTouchedX = prevTouchedX;
                prevTouchedX = -1;             // Reseteljük az érintést
                eraseCursor(oldPrevTouchedX);  // Töröljük a régi sárgát
                redrawCursors();               // Újrarajzoljuk a pirosat (ha kell)
            }
            // Húzás állapotának törlése, ha véletlenül aktív maradt
            isDragging = false;
            dragStartX = -1;
            lastDragX = -1;
        }
        return false;  // Nem a spektrumon történt az érintés
    }

    // --- Érintés a spektrumon belül ---

    // --- Szkennelés közbeni érintés (skála állítás) ---
    // (Ez a rész változatlan marad - signalScale állítás)
    if (scanning && !scanPaused) {
        if (touched && !scanEmpty) {
            Utils::beepTick();
            // ... (a signalScale számításának logikája) ...
            int d = 0;
            int tmpMax = spectrumY;
            float tmpMid = 0;
            int count = 0;
            for (int i = 0; i < spectrumWidth; i++) {
                if (scanValueRSSI[i] < spectrumEndY) {
                    tmpMid += (spectrumEndY - scanValueRSSI[i]);
                    if (scanValueRSSI[i] < tmpMax) tmpMax = scanValueRSSI[i];
                    count++;
                }
            }
            if (count > 0) {
                tmpMid = (spectrumHeight * 0.7f) / (tmpMid / count);
                if ((spectrumEndY - ((spectrumEndY - tmpMax) * tmpMid)) < (spectrumY + spectrumHeight * 0.1f)) {
                    tmpMid = (spectrumHeight * 0.9f) / float(spectrumEndY - tmpMax);
                }
                if (tmpMid > 0.1f && tmpMid < 10.0f) {
                    if ((signalScale * tmpMid) > 10.0f) tmpMid = 10.0f / signalScale;
                    if ((signalScale * tmpMid) < 0.1f) tmpMid = 0.1f / signalScale;
                    signalScale *= tmpMid;
                    for (int i = 0; i < spectrumWidth; i++) {
                        if (scanValueRSSI[i] < spectrumEndY) {
                            float original_rssi_like = (spectrumEndY - scanValueRSSI[i]) / (signalScale / tmpMid);
                            scanValueRSSI[i] = constrain(spectrumEndY - static_cast<int>(original_rssi_like * signalScale), spectrumY, spectrumEndY);
                        }
                    }
                    drawScanGraph(false);
                    drawScanText(true);
                }
            }
        }
        // Szkennelés közben nincs tap/drag, csak skála állítás
        isDragging = false;
        dragStartX = -1;
        lastDragX = -1;
        return true;  // Kezeltük az érintést (skála állítás)
    }

    // --- Szüneteltetett szkennelés közbeni érintés (Tap vagy Drag) ---
    if (scanPaused) {
        if (touched) {
            // --- Érintés kezdete vagy folytatása ---
            if (dragStartX == -1) {
                // Első érintési pont ebben a szekvenciában
                dragStartX = tx;
                lastDragX = tx;
                isDragging = false;  // Még nem húzás
                touchStartTime = millis();
                // Még nem csinálunk semmit, várunk mozgásra vagy felengedésre
            } else {
                // Érintés folytatása (mozgás ellenőrzése)
                int dx = tx - lastDragX;        // Elmozdulás az előző ponthoz képest
                int totalDx = tx - dragStartX;  // Teljes elmozdulás a kezdőponthoz képest

                // Húzásnak minősül, ha elmozdultunk legalább dragMinDistance pixelt VAGY ha már húzásban voltunk
                if (abs(totalDx) >= dragMinDistance || isDragging) {
                    if (!isDragging) {
                        // Most váltottunk húzás módba
                        isDragging = true;
                        // Ha volt sárga kurzor (előző tap miatt), töröljük
                        if (prevTouchedX != -1) {
                            int oldPrevTouchedX = prevTouchedX;
                            prevTouchedX = -1;             // Reseteljük az érintést
                            eraseCursor(oldPrevTouchedX);  // Töröljük a sárgát
                            // Piros kurzort nem rajzolunk, mert húzás van
                        }
                    }

                    // Csak akkor pásztázunk, ha ténylegesen volt elmozdulás (dx != 0)
                    if (dx != 0) {
                        // --- Pásztázás (Panning) ---
                        float oldDelta = deltaScanLine;  // <<<--- DEBUG: Régi érték
                        deltaScanLine -= static_cast<float>(dx);

                        // Opcionális: deltaScanLine korlátozása, hogy ne "fusson ki" a képből túlságosan
                        // float minDelta = ...; float maxDelta = ...;
                        // deltaScanLine = constrain(deltaScanLine, minDelta, maxDelta);

                        // Grafikon és szöveg újrarajzolása az új deltaScanLine értékkel
                        // A true paraméter fontos, mert a frekvenciák megváltoztak a pixeleken!
                        drawScanGraph(true);  // Újrarajzolás az új deltával (törli a régi adatokat is!)
                        drawScanText(true);   // Kezdő/vég frekvenciák frissítése

                        // Piros kurzor újrarajzolása az új helyére (a redrawCursors már kezeli ezt)
                        redrawCursors();

                        // RSSI/SNR kijelző frissítése (az aktuális kurzorpozícióhoz)
                        displayScanSignal();
                    }
                }
                // Utolsó X pozíció frissítése a következő eseményhez
                lastDragX = tx;
            }
        } else {
            // --- Érintés vége ---
            unsigned long touchDuration = millis() - touchStartTime;

            if (!isDragging && touchDuration <= tapMaxDuration && dragStartX != -1) {
                // --- TAP esemény ---
                // (Rövid érintés volt mozgás nélkül a spektrumon belül)
                DEBUG("Tap detected at x=%d.\n", dragStartX);

                int oldPrevTouchedX = prevTouchedX;
                int newTouchedX = dragStartX;  // A tap helye a kezdőpont

                // 1. Frekvencia számítása a tap helyén (dragStartX)
                int n = newTouchedX - spectrumX;
                // A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
                double touchedFreqDouble =
                    static_cast<double>(startFrequency) + (static_cast<double>(n) - (static_cast<double>(spectrumWidth) / 2.0) + deltaScanLine) * static_cast<double>(scanStep);
                uint16_t touchedFrequency = static_cast<uint16_t>(round(touchedFreqDouble));
                touchedFrequency = constrain(touchedFrequency, startFrequency, endFrequency);

                // 2. Frekvencia beállítása és rádió hangolása
                currentFrequency = touchedFrequency;
                setFreq(currentFrequency);  // Ez behangolja a rádiót

                // 3. Kurzor állapot frissítése
                prevTouchedX = newTouchedX;  // Új sárga pozíció

                // 4. Régi kurzorok törlése
                // Régi sárga kurzor törlése (ha volt és máshol)
                if (oldPrevTouchedX != -1 && oldPrevTouchedX != prevTouchedX) {
                    eraseCursor(oldPrevTouchedX);
                }
                // Piros kurzor törlése (ha máshol van, mint az új sárga)
                // Újraszámoljuk a piros kurzor helyét a currentFrequency alapján
                if (scanStep != 0) {
                    currentScanLine = spectrumX + (static_cast<double>(currentFrequency) - static_cast<double>(startFrequency)) / static_cast<double>(scanStep) +
                                      (static_cast<double>(spectrumWidth) / 2.0) - deltaScanLine;
                } else {
                    currentScanLine = spectrumX;  // Hiba eset
                }
                currentScanLine = constrain(currentScanLine, spectrumX, spectrumEndScanX - 1);
                int currentX = static_cast<int>(currentScanLine);
                if (currentX != prevTouchedX) {
                    eraseCursor(currentX);  // Töröljük a pirosat, ha nem ugyanott van, mint az új sárga
                }

                // 5. Új sárga kurzor kirajzolása
                drawYellowCursor(prevTouchedX);

                // 6. Kijelzők frissítése
                displayScanSignal();  // RSSI/SNR
                drawScanText(false);  // Aktuális frekvencia kiírása

            } else if (isDragging) {
                // --- Húzás vége ---
                DEBUG("Drag ended.\n");
                // A grafikon már frissítve lett húzás közben.
                // Biztosítjuk a kurzor helyes állapotát (piros kurzor a helyére).
                redrawCursors();
            } else if (prevTouchedX != -1) {
                // --- Egyéb érintés vége (pl. kívülről behúzva, nem tap, nem drag) ---
                DEBUG("Touch ended outside drag/tap conditions, removing yellow cursor.\n");
                int oldPrevTouchedX = prevTouchedX;
                prevTouchedX = -1;             // Reseteljük az érintést
                eraseCursor(oldPrevTouchedX);  // Töröljük a sárgát
                redrawCursors();               // Újrarajzoljuk a pirosat
            }

            // Húzás/érintés állapotának törlése mindenképp
            isDragging = false;
            dragStartX = -1;
            lastDragX = -1;
        }
        return true;  // Kezeltük az érintést (Tap vagy Drag)
    }

    return false;  // Nem kezeltük (pl. nem volt pause)
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void FreqScanDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {
    DEBUG("FreqScanDisplay::processScreenButtonTouchEvent() -> id: %d, label: %s, state: %s\n", event.id, event.label, TftButton::decodeState(event.state));

    if (STREQ("Start", event.label)) {
        startScan();
    } else if (STREQ("Stop", event.label)) {
        stopScan();
    } else if (STREQ("Pause", event.label)) {
        // A gomb állapota már beállt a handleTouch-ban, itt csak reagálunk rá
        scanPaused = (event.state == TftButton::ButtonState::On);
        pauseScan();  // Metódus hívása a szükséges műveletekhez
    } else if (STREQ("Scale", event.label)) {
        changeScanScale();
    } else if (STREQ("Back", event.label)) {
        stopScan();  // Leállítjuk a szkennelést, mielőtt visszalépünk
        // Visszalépés az előző képernyőre (FM vagy AM)
        ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayBase::DisplayType::fm : DisplayBase::DisplayType::am;
    }
}

// --- Private Helper Methods ---

/**
 * Szkennelés indítása
 */
void FreqScanDisplay::startScan() {
    if (scanning) return;  // Már fut

    DEBUG("Starting scan...\n");
    scanning = true;
    scanPaused = false;             // Indításkor nem szünetel
    scanEmpty = true;               // Új szkennelés, töröljük az adatokat
    scanAGC = config.data.agcGain;  // Mentsük el az AGC állapotát

    // Állítsuk be a "Pause" gombot Off állapotba
    TftButton *pauseButton = findButtonByLabel("Pause");
    if (pauseButton) pauseButton->setState(TftButton::ButtonState::Off);
    // Stop gomb engedélyezése, Start tiltása
    TftButton *startButton = findButtonByLabel("Start");
    if (startButton) startButton->setState(TftButton::ButtonState::Disabled);
    TftButton *stopButton = findButtonByLabel("Stop");
    if (stopButton) stopButton->setState(TftButton::ButtonState::Off);  // Off = enabled but not pushed

    // Szkennelés kezdése a bal szélről ---
    //  Kiszámítjuk a bal szélnek (n=0) megfelelő frekvenciát az AKTUÁLIS deltaScanLine és scanStep alapján
    //  F(n=0) = startFrequency + (0 - spectrumWidth/2 + deltaScanLine) * scanStep
    double startVisibleFreqDouble = static_cast<double>(startFrequency) + (0.0 - (static_cast<double>(spectrumWidth) / 2.0) + deltaScanLine) * static_cast<double>(scanStep);
    posScanFreq = static_cast<uint16_t>(round(startVisibleFreqDouble));
    posScanFreq = constrain(posScanFreq, startFrequency, endFrequency);  // Biztosítjuk a sávhatárokat

    posScan = 0;  // A szkennelési index 0-ról indul (bár a displayLoop újraszámolja)
    posScanLast = -1;
    signalScale = 1.5f;  // Alapértelmezett jelerősség skála

    // AGC kikapcsolása szkenneléshez (sample.cpp logika)
    config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off);
    checkAGC();

    // Spektrum törlése és újrarajzolása
    drawScanGraph(true);
    drawScanText(true);

    setFreq(posScanFreq);       // Első frekvencia beállítása
    si4735.setAudioMute(true);  // Némítás szkennelés alatt

    // Kurzor eltüntetése (ha volt)
    eraseCursor(static_cast<int>(currentScanLine));
    if (prevTouchedX != -1) {
        eraseCursor(prevTouchedX);
        prevTouchedX = -1;
    }

    // Gombok újrarajzolása a frissített állapotokkal
    drawScreenButtons();
}

/**
 * Szkennelés leállítása
 */
void FreqScanDisplay::stopScan() {
    if (!scanning) return;  // Már le van állítva

    scanning = false;
    scanPaused = true;

    // Állítsuk be a "Pause" gombot On állapotba (szünetel)
    TftButton *pauseButton = DisplayBase::findButtonByLabel("Pause");
    if (pauseButton) pauseButton->setState(TftButton::ButtonState::On);
    // Stop gomb tiltása, Start engedélyezése
    TftButton *startButton = DisplayBase::findButtonByLabel("Start");
    if (startButton) startButton->setState(TftButton::ButtonState::Off);  // Off = enabled
    TftButton *stopButton = DisplayBase::findButtonByLabel("Stop");
    if (stopButton) stopButton->setState(TftButton::ButtonState::Disabled);

    // AGC visszaállítása
    config.data.agcGain = scanAGC;
    Si4735Utils::checkAGC();

    // Hang visszaállítása (ha némítva volt)
    si4735.setAudioMute(rtv::muteStat);  // Vissza az eredeti némítási állapotba

    // Utolsó ismert frekvencia beállítása (ahol a kurzor van)
    setFreq(currentFrequency);

    // Piros kurzor kirajzolása az aktuális frekvenciára
    redrawCursors();      // Ez kiszámolja és kirajzolja a piros kurzort
    displayScanSignal();  // RSSI/SNR frissítése

    DEBUG("Scan stopped at %d kHz\n", currentFrequency);

    // Gombok újrarajzolása a frissített állapotokkal
    DisplayBase::drawScreenButtons();
}

/**
 * Szkennelés szüneteltetése/folytatása
 */
void FreqScanDisplay::pauseScan() {
    int currentX = static_cast<int>(currentScanLine);  // Piros kurzor X pozíciója

    if (scanPaused) {  // Most lett szüneteltetve
        // AGC visszaállítása, hang vissza, step vissza...
        config.data.agcGain = scanAGC;
        Si4735Utils::checkAGC();
        si4735.setAudioMute(rtv::muteStat);
        uint8_t step = band.getCurrentBand().varData.currStep;
        si4735.setFrequencyStep(step);

        // Frekvencia beállítása a kurzor pozíciójára
        setFreq(currentFrequency);

        // Megfelelő kurzor kirajzolása
        redrawCursors();      // Ez kirajzolja a sárgát ha kell, vagy a pirosat
        displayScanSignal();  // RSSI/SNR frissítése

    } else {  // Most folytatódik
        // AGC ki, hang némít, scan step beállít...
        scanAGC = config.data.agcGain;
        config.data.agcGain = static_cast<uint8_t>(Si4735Utils::AgcGainMode::Off);
        Si4735Utils::checkAGC();
        si4735.setAudioMute(true);

        // Frekvencia beállítása a következő szkennelési pontra
        setFreq(posScanFreq);

        // Aktuális kurzor (piros vagy sárga) eltüntetése
        if (prevTouchedX != -1) {
            eraseCursor(prevTouchedX);
            prevTouchedX = -1;  // Folytatáskor töröljük az érintés állapotát
        } else {
            eraseCursor(currentX);  // Piros kurzor törlése
        }
    }
}

/**
 * Szkennelési skála (lépésköz) váltása
 */
void FreqScanDisplay::changeScanScale() {
    bool was_paused = scanPaused;
    // Ha futott a szkennelés, ideiglenesen szüneteltetjük logikailag is
    if (scanning && !was_paused) {
        scanPaused = true;
    }

    float oldScanStep = scanStep;

    // Új scanStep kiszámítása
    scanStep *= 2.0f;
    if (scanStep > maxScanStep) scanStep = minScanStep;
    if (scanStep < minScanStep) scanStep = minScanStep;

    // deltaScanLine újraszámítása, hogy a képernyő közepe ugyanaz a frekvencia maradjon
    // A currentFrequency-t használjuk középpontnak, ha szünetelt, egyébként a posScanFreq-et
    float freqAtCenter = was_paused ? static_cast<float>(currentFrequency) : static_cast<float>(posScanFreq);

    // deltaScanLine Számítás
    if (scanStep != 0) {  // Osztás nullával elkerülése
        // deltaScanLine = (freqAtCenter - startFrequency) / scanStep; // Régi
        // Új: deltaScanLine azt mutatja meg, hány 'scanStep' lépésre van a freqAtCenter a startFrequency-től.
        // A képernyő közepének (spectrumWidth / 2) frekvenciája: F(center) = startFrequency + (deltaScanLine) * scanStep
        deltaScanLine = (freqAtCenter - static_cast<float>(startFrequency)) / scanStep;
    } else {
        deltaScanLine = 0;  // Hiba vagy alapértelmezett eset
    }

    // Grafikon és szöveg újrarajzolása...
    // prevRssiY = spectrumEndY; // Már nem használt
    drawScanGraph(true);  // Törli a régi adatokat is
    drawScanText(true);

    // --- Szkennelés folytatása vagy kurzor újrarajzolása ---
    if (scanning && !was_paused) {
        // Ha a szkennelés futott, a folytatáshoz a LÁTHATÓ tartomány elejére ugrunk

        // Szkennelés újraindítása a bal szélről ---
        //  Kiszámítjuk a bal szélnek (n=0) megfelelő frekvenciát az ÚJ skála és delta alapján
        //  A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
        double startVisibleFreqDouble = static_cast<double>(startFrequency) + (0.0 - (static_cast<double>(spectrumWidth) / 2.0) + deltaScanLine) * static_cast<double>(scanStep);
        posScanFreq = static_cast<uint16_t>(round(startVisibleFreqDouble));
        posScanFreq = constrain(posScanFreq, startFrequency, endFrequency);  // Biztosítjuk a sávhatárokat

        posScan = 0;  // Kezdő index
        posScanLast = -1;
        scanEmpty = true;      // Az új nézetben még nincs adatunk
        setFreq(posScanFreq);  // Rádiót a kezdő frekvenciára hangoljuk

        // Folytatás előkészítése
        scanPaused = false;  // Visszaállítjuk a logikai állapotot

    } else {
        // Ha szünetelt, a kurzort újra ki kell rajzolni pirossal az új helyen
        setFreq(currentFrequency);  // Biztosítjuk, hogy a rádió a kurzor frekvenciáján legyen

        // Kurzor újrarajzolása
        redrawCursors();
        displayScanSignal();  // RSSI/SNR kijelző frissítése
    }
}

/**
 * Spektrum alapjának és skálájának rajzolása
 * @param erase Törölje a korábbi adatokat?
 */
void FreqScanDisplay::drawScanGraph(bool erase) {
    int d = 0;  // screenV itt nem releváns

    if (erase) {
        tft.fillRect(spectrumX, spectrumY, spectrumWidth, spectrumHeight, TFT_BLACK);  // Háttér törlése
        scanEmpty = true;
        // Vektorok törlése vagy nullázása
        std::fill(scanValueRSSI.begin(), scanValueRSSI.end(), spectrumEndY);  // Max Y érték = min jel
        std::fill(scanValueSNR.begin(), scanValueSNR.end(), 0);
        std::fill(scanMark.begin(), scanMark.end(), false);
        std::fill(scanScaleLine.begin(), scanScaleLine.end(), 0);
        // prevRssiY = spectrumEndY; // Már nem használt
    }

    scanBeginBand = -1;
    scanEndBand = spectrumWidth;
    prevScaleLine = false;

    // Vonalak újrarajzolása (ha nem töröltünk) vagy alap skála rajzolása
    for (int n = 0; n < spectrumWidth; n++) {
        // Az erase logikát most már a drawScanLine kezeli
        drawScanLine(spectrumX + n);
    }

    // --- Sávhatár jelző vonalak rajzolása ---
    if (scanBeginBand > 0 && scanBeginBand < spectrumWidth) {                               // Ha a kezdő határ látható
        tft.drawFastVLine(spectrumX + scanBeginBand, spectrumEndY - 10, 10, TFT_DARKGREY);  // Csak alul egy 10px magas vonal
    }
    if (scanEndBand >= 0 && scanEndBand < spectrumWidth - 1) {                            // Ha a vég határ látható
        tft.drawFastVLine(spectrumX + scanEndBand, spectrumEndY - 10, 10, TFT_DARKGREY);  // Csak alul egy 10px magas vonal
    }
    // ------------------------------------------

    // --- SNR Limit Vonal (Vizuális Jelző) ---
    uint16_t snrLimitLineY = spectrumEndY - 10;  // Példa: 10 pixelre az aljától
    tft.drawFastHLine(spectrumX, snrLimitLineY, spectrumWidth, TFT_DARKGREY);
    // --- Vonal VÉGE ---

    // Keret újrarajzolása
    tft.drawRect(spectrumX - 1, spectrumY - 1, spectrumWidth + 2, spectrumHeight + 2, TFT_WHITE);
}

/**
 * Egy spektrumvonal/oszlop rajzolása a megadott X pozícióra
 * (Kurzor rajzolása NÉLKÜL)
 * @param xPos Az X koordináta a képernyőn
 */
void FreqScanDisplay::drawScanLine(int xPos) {
    int n = xPos - spectrumX;
    if (n < 0 || n >= spectrumWidth) return;

    // Aktuális frekvencia kiszámítása (precízebben)
    // A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
    double freq_double =
        static_cast<double>(startFrequency) + (static_cast<double>(n) - (static_cast<double>(spectrumWidth) / 2.0) + deltaScanLine) * static_cast<double>(scanStep);
    uint16_t frq = static_cast<uint16_t>(round(freq_double));

    int16_t colf = TFT_NAVY;
    int16_t colb = TFT_BLACK;

    // --- Skálavonal típusának meghatározása ---
    if (!scanScaleLine[n] || scanEmpty) {
        scanScaleLine[n] = 0;
        double stepThreshold = scanStep > 0 ? static_cast<double>(scanStep) * 0.5 : 0.5;  // Tolerancia a lépésköz fele
        if (scanStep <= 100) {
            if (fmod(freq_double, 100.0) < stepThreshold || fmod(freq_double, 100.0) > (100.0 - stepThreshold)) scanScaleLine[n] = 2;
        }
        if (!scanScaleLine[n] && scanStep <= 50) {
            if (fmod(freq_double, 50.0) < stepThreshold || fmod(freq_double, 50.0) > (50.0 - stepThreshold)) scanScaleLine[n] = 3;
        }
        if (!scanScaleLine[n] && scanStep <= 10) {
            if (fmod(freq_double, 10.0) < stepThreshold || fmod(freq_double, 10.0) > (10.0 - stepThreshold)) scanScaleLine[n] = 4;
        }
        if (!scanScaleLine[n]) scanScaleLine[n] = 1;
    }

    // Színek beállítása a skálavonal típusa alapján
    if (scanScaleLine[n] == 2)
        colb = TFT_OLIVE;
    else if (scanScaleLine[n] == 3)
        colb = TFT_DARKGREY;
    else if (scanScaleLine[n] == 4)
        colb = TFT_DARKGREY;

    // --- Szín az SNR alapján ---
    if (scanValueSNR[n] > 0 && !scanEmpty) {
        colf = TFT_NAVY + 0x8000;
        if (scanValueSNR[n] < 16)
            colf += (scanValueSNR[n] * 2048);
        else {
            colf = 0xFBE0;  // Sárga
            if (scanValueSNR[n] < 24)
                colf += ((scanValueSNR[n] - 16) * 4);
            else
                colf = TFT_RED;
        }
    }

    // --- Sávon kívüli terület indexének jelölése ---
    const double freqTolerance = scanStep * 0.1;
    if (freq_double > (static_cast<double>(endFrequency) + freqTolerance)) {
        if (scanEndBand > n) scanEndBand = n;
    } else {
        if (n >= scanEndBand) scanEndBand = spectrumWidth;
    }
    if (freq_double < (static_cast<double>(startFrequency) - freqTolerance)) {
        if (scanBeginBand < n) scanBeginBand = n;
    } else {
        if (n <= scanBeginBand) scanBeginBand = -1;
    }

    // --- Rajzolás ---
    int currentRssiY = scanValueRSSI[n];
    currentRssiY = constrain(currentRssiY, spectrumY, spectrumEndY);

    // 1. Teljes oszlop törlése feketével (mindig)
    tft.drawFastVLine(xPos, spectrumY, spectrumHeight, TFT_BLACK);

    // 2. Skálavonal rajzolása (ha van)
    if (colb != TFT_BLACK) {
        if (scanScaleLine[n] == 2)
            tft.drawFastVLine(xPos, spectrumY, spectrumHeight, colb);
        else if (scanScaleLine[n] == 3)
            tft.drawFastVLine(xPos, spectrumY + spectrumHeight / 2, spectrumHeight / 2, colb);
        else if (scanScaleLine[n] == 4)
            tft.drawFastVLine(xPos, spectrumY + spectrumHeight * 3 / 4, spectrumHeight / 4, colb);
    }

    // 3. Jelszint oszlop rajzolása (ha van jel)
    if (currentRssiY < spectrumEndY && !scanEmpty) {
        tft.drawFastVLine(xPos, currentRssiY, spectrumEndY - currentRssiY, colf);
    }

    // 4. Fő jelvonal (összekötve az előző ponttal)
    if (!scanEmpty) {
        if (n > 0) {
            int prevY = (n - 1 >= 0) ? constrain(scanValueRSSI[n - 1], spectrumY, spectrumEndY) : spectrumEndY;
            tft.drawLine(xPos - 1, prevY, xPos, currentRssiY, TFT_SILVER);
        } else {
            tft.drawPixel(xPos, currentRssiY, TFT_SILVER);
        }
    }

    // 5. Jelölő (scanMark) kirajzolása
    if (scanMark[n] && !scanEmpty) {
        tft.fillRect(xPos - 1, spectrumY + 5, 3, 5, TFT_YELLOW);
    }
}

/**
 * Frekvencia címkék és egyéb szövegek rajzolása
 * @param all Minden szöveget rajzoljon újra? (true = igen, false = csak a változókat)
 */
void FreqScanDisplay::drawScanText(bool all) {

    // --- A többi szöveg rajzolása (pl. sáv eleje/vége) ---
    tft.setTextFont(1);  // Kisebb font a többi szöveghez
    tft.setTextSize(1);
    uint8_t fontHeight = tft.fontHeight();         // Font magasság
    uint16_t textY = spectrumY - 2;                // Szöveg aljának pozíciója
    uint16_t clearY = spectrumY - fontHeight - 3;  // Törlés teteje

    // Robusztusabb törlés
    if (all) {
        // Mindig töröljük a lehetséges területeket, ha 'all' igaz
        // BEGIN terület (bal)
        tft.fillRect(spectrumX, clearY, 60, fontHeight + 2, TFT_BLACK);  // Szélesebb törlés
        // END terület (jobb)
        tft.fillRect(spectrumEndScanX - 60, clearY, 60, fontHeight + 2, TFT_BLACK);  // Szélesebb törlés
    }

    // END felirat rajzolása, ha kell
    if (all || (scanEndBand < (spectrumWidth - 5))) {
        // Csak akkor törlünk külön, ha 'all' hamis (mert 'all=true' esetén már töröltünk)
        if (!all) tft.fillRect(spectrumX + scanEndBand + 3, clearY, 40, fontHeight + 2, TFT_BLACK);
        if (scanEndBand < (spectrumWidth - 5)) {
            tft.setTextColor(TFT_SILVER, TFT_BLACK);                    // Szín beállítása
            tft.setTextDatum(BL_DATUM);                                 // Bottom-Left igazítás
            tft.drawString("END", spectrumX + scanEndBand + 5, textY);  // textY használata
        }
    }
    // BEGIN felirat rajzolása, ha kell
    if (all || (scanBeginBand > 5)) {
        // Csak akkor törlünk külön, ha 'all' hamis
        if (!all) tft.fillRect(spectrumX + scanBeginBand - 43, clearY, 40, fontHeight + 2, TFT_BLACK);
        if (scanBeginBand > 5) {
            tft.setTextColor(TFT_SILVER, TFT_BLACK);                        // Szín beállítása
            tft.setTextDatum(BR_DATUM);                                     // Bottom-Right igazítás
            tft.drawString("BEGIN", spectrumX + scanBeginBand - 5, textY);  // textY használata
        }
    }

    // Skála kezdő és vég frekvenciájának kiírása...
    if (all) {
        // 1. Számítsd ki a képernyő közepén lévő frekvenciát (freqAtCenter)
        //    A frekvencia képlete: F(center) = startFrequency + deltaScanLine * scanStep
        double centerFreqDouble = static_cast<double>(startFrequency) + deltaScanLine * static_cast<double>(scanStep);

        // 2. Számítsd ki a látható kezdő és vég frekvenciát a középhez képest
        double halfWidthFreqSpan = (static_cast<double>(spectrumWidth) / 2.0) * static_cast<double>(scanStep);
        double startFreqDouble = centerFreqDouble - halfWidthFreqSpan;
        double endFreqDouble = centerFreqDouble + halfWidthFreqSpan;

        uint16_t freqStartVisible = static_cast<uint16_t>(round(startFreqDouble));
        uint16_t freqEndVisible = static_cast<uint16_t>(round(endFreqDouble));

        // Korlátozás a teljes sávhatárokra
        freqStartVisible = constrain(freqStartVisible, startFrequency, endFrequency);
        freqEndVisible = constrain(freqEndVisible, startFrequency, endFrequency);

        // --- Kisebb betűméret beállítása ---
        tft.setTextFont(1);  // Győződjünk meg róla, hogy a kisebb font van beállítva
        tft.setTextSize(1);  // Kisebb betűméret (1-es)
        // --- Betűméret beállítás vége ---

        // Kezdő frekvencia kirajzolása
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(BL_DATUM);
        // Biztosabb törlés: Y+3 kezdés, 15 magas (lefedi a 15-ös Y rajzolást)
        tft.fillRect(spectrumX, spectrumEndY + 3, 100, 15, TFT_BLACK);
        tft.drawString(String(freqStartVisible), spectrumX, spectrumEndY + 15);  // Új érték (kisebb betűvel)

        // Vég frekvencia kirajzolása
        tft.setTextDatum(BR_DATUM);
        // Biztosabb törlés
        tft.fillRect(spectrumEndScanX - 100, spectrumEndY + 3, 100, 15, TFT_BLACK);
        tft.drawString(String(freqEndVisible), spectrumEndScanX, spectrumEndY + 15);  // Új érték (kisebb betűvel)

        // Lépésköz kiírása...
        tft.setTextDatum(BC_DATUM);
        // Biztosabb törlés
        tft.fillRect(spectrumX + spectrumWidth / 2 - 50, spectrumEndY + 3, 100, 15, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        // Új lépésköz kirajzolása az AKTUÁLIS scanStep alapján (kisebb betűvel)
        tft.drawString("Step: " + String(scanStep, scanStep < 1.0f ? 3 : 1) + " kHz", spectrumX + spectrumWidth / 2, spectrumEndY + 15);
    }

    // --- Aktuális frekvencia kiírása ---
    // Ha itt nagyobb font kell, akkor az előző blokk végén vissza kell állítani!
    uint16_t freqToDisplayRaw = (scanning && !scanPaused) ? posScanFreq : currentFrequency;
    String freqStr;

    // Mértékegység és formázás meghatározása
    if (band.getCurrentBandType() == FM_BAND_TYPE) {
        float freqMHz = freqToDisplayRaw / 100.0f;
        freqStr = String(freqMHz, 2);  // FM: MHz, 2 tizedesjegy
    } else {
        freqStr = String(freqToDisplayRaw);  // AM/SW/LW: kHz, egész szám
    }

    // Nagyobb font visszaállítása a fő frekvenciához (ha szükséges volt a kisebbítés)
    tft.setTextFont(2);          // Nagyobb font a fő frekvenciához
    tft.setTextSize(1);          // Alapértelmezett méret a font 2-höz
    tft.setTextDatum(TL_DATUM);  // Bal felső igazítás

    // "Freq: " címke kiírása
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Freq: ", 5, 25);  // Pozíció a bal felső sarokban

    // Frekvencia érték helyének törlése
    uint16_t valueStartX = 5 + tft.textWidth("Freq: ");
    tft.fillRect(valueStartX, 20, 100, 20, TFT_BLACK);

    // Új frekvencia érték kiírása
    tft.drawString(freqStr, valueStartX, 25);
}

/**
 * Aktuális RSSI/SNR kiírása a spektrum fölé
 */
void FreqScanDisplay::displayScanSignal() {
    int d = 0;  // screenV nem releváns
    int xPos = static_cast<int>(currentScanLine);
    int n = xPos - spectrumX;

    // Csak akkor írunk ki, ha a kurzor a spektrumon belül van
    bool cursorVisible = (xPos >= spectrumX && xPos < spectrumEndScanX);

    tft.setTextFont(1);                     // Explicit kisebb font beállítása
    tft.setTextSize(1);                     // Méret beállítása a font után
    uint8_t fontHeight = tft.fontHeight();  // Tényleges font magasság lekérdezése
    tft.setTextDatum(BC_DATUM);             // Bottom-Center igazítás

    // Y pozíciók módosítása ---
    // Törlési és rajzolási Y koordináták feljebb tolása
    uint16_t clearY = spectrumY - fontHeight - 3;  // Törlési téglalap teteje (biztonsági ráhagyással)
    uint16_t textY = spectrumY - 2;                // Szöveg aljának pozíciója (biztonsággal a spektrum felett)

    // Középen fent töröljük a régi értéket a számított magassággal
    tft.fillRect(spectrumX + spectrumWidth / 2 - 60, clearY, 120, fontHeight + 2, TFT_BLACK);  // Szélesebb törlés

    if (scanPaused && cursorVisible) {  // Ha szünetel, az aktuális mérést írjuk ki
        si4735.getCurrentReceivedSignalQuality();
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("RSSI:" + String(si4735.getCurrentRSSI()), spectrumX + spectrumWidth / 2 - 30, textY);  // textY használata
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("SNR:" + String(si4735.getCurrentSNR()), spectrumX + spectrumWidth / 2 + 30, textY);  // textY használata
    } else if (!scanEmpty && cursorVisible && n >= 0 && n < spectrumWidth) {                                 // Ha fut és van adat, a tárolt értéket írjuk ki
        // Az RSSI érték visszaalakítása a skálázott Y koordinátából
        int displayed_rssi = 0;
        if (signalScale != 0) {  // Osztás nullával elkerülése
            displayed_rssi = static_cast<int>((spectrumEndY - scanValueRSSI[n]) / signalScale);
        }

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("RSSI:" + String(displayed_rssi), spectrumX + spectrumWidth / 2 - 30, textY);  // textY használata
        tft.setTextColor(TFT_ORANGE, TFT_BLACK);
        tft.drawString("SNR:" + String(scanValueSNR[n]), spectrumX + spectrumWidth / 2 + 30, textY);  // textY használata
    }
}

/**
 * Jelerősség (RSSI vagy SNR) lekérése (átlagolással)
 * @param rssi True esetén RSSI-t, false esetén SNR-t ad vissza.
 * @return Az átlagolt jelerősség (RSSI esetén már Y koordinátává alakítva).
 */
int FreqScanDisplay::getSignal(bool rssi) {
    int res = 0;
    for (int i = 0; i < countScanSignal; i++) {
        si4735.getCurrentReceivedSignalQuality();  // Új mérés kérése
        if (rssi)
            res += si4735.getCurrentRSSI();
        else
            res += si4735.getCurrentSNR();
        // Rövid várakozás lehet szükséges a mérések között?
    }
    res /= countScanSignal;  // Átlagolás

    // Ha RSSI-t kértünk, alakítsuk át Y koordinátává a sample.cpp logika szerint
    if (rssi) {
        res = spectrumEndY - static_cast<int>(static_cast<float>(res) * signalScale);
        res = constrain(res, spectrumY, spectrumEndY);  // Korlátok közé szorítás (Y koordináta!)
    }

    return res;
}

/**
 * Frekvencia beállítása és kapcsolódó műveletek
 * @param f A beállítandó frekvencia (kHz).
 */
void FreqScanDisplay::setFreq(uint16_t f) {
    // Nem engedjük a sávon kívülre állítani
    f = constrain(f, startFrequency, endFrequency);

    posScanFreq = f;  // Tároljuk a szkennelési frekvenciát is
    if (scanPaused) {
        currentFrequency = f;  // Ha szünetel, az aktuális frekvencia is ez lesz
    }

    si4735.setFrequency(f);
    // Az AGC-t csak akkor állítjuk, ha szkennelünk és nem szünetelünk
    if (scanning && !scanPaused) {
        // AGC letiltása (1 = disabled)
        si4735.setAutomaticGainControl(1, 0);  // Explicit letiltás
    }
}

/**
 * Frekvencia léptetése felfelé (a scanStep alapján)
 */
void FreqScanDisplay::freqUp() {
    // A scanStep lehet float is!
    double nextFreqDouble = static_cast<double>(posScanFreq) + static_cast<double>(scanStep);

    if (nextFreqDouble > static_cast<double>(endFrequency)) {
        posScanFreq = startFrequency;  // Túlcsordulás esetén vissza az elejére
    } else {
        posScanFreq = static_cast<uint16_t>(round(nextFreqDouble));
    }
    setFreq(posScanFreq);  // Beállítjuk az új frekvenciát
}

/**
 * Visszarajzolja az alap grafikát (skála, jel) az adott X pozícióban,
 * eltüntetve ezzel az ott lévő kurzort.
 * @param xPos Az X koordináta
 */
void FreqScanDisplay::eraseCursor(int xPos) {
    if (xPos >= spectrumX && xPos < spectrumEndScanX) {
        drawScanLine(xPos);  // Meghívja az új, kurzor nélküli drawScanLine-t
    }
}

/**
 * Kirajzolja a sárga érintésjelző kurzort az adott X pozícióba.
 * @param xPos Az X koordináta
 */
void FreqScanDisplay::drawYellowCursor(int xPos) {
    // Csak akkor rajzolunk, ha szünetel és a pozíció érvényes
    if (scanPaused && xPos >= spectrumX && xPos < spectrumEndScanX) {
        tft.drawFastVLine(xPos, spectrumY, spectrumHeight, TFT_YELLOW);
    }
}

/**
 * Kirajzolja a piros frekvenciajelző kurzort az adott X pozícióba.
 * @param xPos Az X koordináta
 */
void FreqScanDisplay::drawRedCursor(int xPos) {
    // Csak akkor rajzolunk, ha szünetel és a pozíció érvényes
    if (scanPaused && xPos >= spectrumX && xPos < spectrumEndScanX) {
        tft.drawFastVLine(xPos, spectrumY, spectrumHeight, TFT_RED);
    }
}

/**
 * Újraszámolja a piros kurzor pozícióját a currentFrequency alapján,
 * és kirajzolja a megfelelő kurzort (sárgát, ha van érintés, pirosat, ha nincs).
 * Előtte eltünteti a kurzor(ok) előző helyét.
 */
void FreqScanDisplay::redrawCursors() {
    if (!scanPaused) return;  // Csak szüneteltetve van értelme

    // Piros kurzor X pozíciójának kiszámítása az aktuális frekvencia alapján
    // A frekvencia képlete: F(n) = startFrequency + (n - spectrumWidth/2 + deltaScanLine) * scanStep
    // Ebből n-re rendezve: n = (F(n) - startFrequency)/scanStep + spectrumWidth/2 - deltaScanLine
    if (scanStep != 0) {
        currentScanLine = spectrumX + (static_cast<double>(currentFrequency) - static_cast<double>(startFrequency)) / static_cast<double>(scanStep) +
                          (static_cast<double>(spectrumWidth) / 2.0) - deltaScanLine;
    } else {
        currentScanLine = spectrumX;  // Hiba eset
    }
    currentScanLine = constrain(currentScanLine, spectrumX, spectrumEndScanX - 1);
    int currentX = static_cast<int>(currentScanLine);

    // 1. Eltüntetjük a kurzort az AKTUÁLIS piros kurzor helyéről
    //    (Ez akkor is kell, ha sárga lesz, hogy a piros eltűnjön alóla)
    eraseCursor(currentX);

    // 2. Eltüntetjük a kurzort az ELŐZŐ sárga kurzor helyéről (ha volt és máshol van)
    if (prevTouchedX != -1 && prevTouchedX != currentX) {
        eraseCursor(prevTouchedX);
    }

    // 3. Kirajzoljuk a megfelelő kurzort az új helyére(i)re
    if (prevTouchedX != -1) {
        // Ha van aktív érintés (sárga kurzor)
        drawYellowCursor(prevTouchedX);
    } else {
        // Ha nincs aktív érintés, a piros kurzort rajzoljuk
        drawRedCursor(currentX);
    }
}
