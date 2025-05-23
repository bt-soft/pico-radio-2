#include "AmDisplay.h"

#include <Arduino.h>

/**
 * @brief Konstruktor az AmDisplay osztályhoz.
 * @param tft Referencia a TFT kijelző objektumra.
 * @param si4735 Referencia az SI4735 rádió chip objektumra.
 * @param band Referencia a Band objektumra.
 */
AmDisplay::AmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band) : DisplayBase(tft, si4735, band), pMiniAudioFft(nullptr) {

    DEBUG("AmDisplay::AmDisplay\n");

    // SMeter példányosítása
    pSMeter = new SMeter(tft, 0, 80);

    // Frekvencia kijelzés pédányosítása
    pSevenSegmentFreq = new SevenSegmentFreq(tft, rtv::freqDispX, rtv::freqDispY, band);

    // Függőleges gombok legyártása, nincs saját függőleges gombsor
    // A kötelező függőleges gombokat (Mute, Volum, AGC, Squel, Freq, Setup, Memo, FFT) a Base osztály kezeli.
    DisplayBase::buildVerticalScreenButtons(nullptr, 0);

    // Horizontális képernyőgombok definiálása
    // Hozzáadjuk az AM-specifikus gombokat ÉS a AFWdt, BFO gombokat.
    DisplayBase::BuildButtonData horizontalButtonsData[] = {
        {"BFO", TftButton::ButtonType::Toggleable, TftButton::ButtonState::Off},  // Beat Frequency Oscillator
        {"AFWdt", TftButton::ButtonType::Pushable},                               //
        {"SSTV", TftButton::ButtonType::Pushable},                                // SSTV GOMB
        {"AntC", TftButton::ButtonType::Pushable},                                //
    };

    // RTTY/CW módválasztó gomb létrehozása (nem a buildHorizontalScreenButtons része)
    // Ezt a gombot manuálisan hozzuk létre és pozicionáljuk a szövegterület mellé.
    // Az ID-jét a SCRN_HBTNS_ID_START + a horizontális gombok száma után adjuk meg.
    // A pontos ID-t a buildHorizontalScreenButtons hívása után tudjuk meghatározni.

    // MiniAudioFft és RTTY dekóder inicializálása
    // Az AudioProcessor a MiniAudioFftConfigAm gain config referenciát használja.
    // A targetSamplingFrequency 12000 Hz (2 * 6000 Hz) az AM FFT-hez és RTTY-hez.
    pAudioProcessor = new AudioProcessor(config.data.miniAudioFftConfigAm, AUDIO_INPUT_PIN, MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ * 2.0f);  // 12000 Hz
    pRttyDecoder = new RttyDecoder(*pAudioProcessor);  // RTTY dekóder inicializálása az AudioProcessorral

    // RTTY szövegterület koordinátái
    rttyTextAreaX = RTTY_TEXT_AREA_X_MARGIN;
    rttyTextAreaY = 150;  // RSSI/SNR alatt legyen
    rttyTextAreaW = 370;  // Szélesség
    rttyTextAreaH = 80;   // Magasság

    // Dekódolt szöveg pufferek inicializálása
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        rttyDisplayLines[i] = "";
    }
    rttyCurrentLineBuffer = "";

    // RTTY/CW módválasztó gomb létrehozása
    // A pontos pozíciót a drawScreen-ben számoljuk ki.
    uint16_t modeButtonWidth = SCRN_BTN_W / 2;                                                        // Kisebb gomb
    pRttyCwModeButton = new TftButton(SCRN_HBTNS_ID_START + ARRAY_ITEM_COUNT(horizontalButtonsData),  // ID a horizontális gombok után
                                      tft, 0, 0, modeButtonWidth, SCRN_BTN_H / 2,                     // Helyőrző pozíció és méret
                                      currentDecodeMode == DecodeMode::RTTY ? "RTTY" : "CW",          // Kezdeti felirat
                                      TftButton::ButtonType::Pushable);                               // Pushable, mert vált a módok között

    // Horizontális képernyőgombok legyártása:
    // Összefűzzük a kötelező gombokat az AM-specifikus (és AFWdt, BFO) gombokkal.
    // A pRttyCwModeButton nincs benne ebben a tömbben, manuálisan kezeljük.
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), true);  // isMandatoryNeed = true
}

/**
 * @brief Destruktor az AmDisplay osztályhoz.
 * Felszabadítja a dinamikusan allokált erőforrásokat.
 */
AmDisplay::~AmDisplay() {
    DEBUG("AmDisplay::~AmDisplay\n");
    // SMeter trölése
    if (pSMeter) {
        delete pSMeter;
    }

    // Frekvencia kijelző törlése
    if (pSevenSegmentFreq) {
        delete pSevenSegmentFreq;
    }

    // MiniAudioFft törlése
    if (pMiniAudioFft) {
        delete pMiniAudioFft;
    }
    // RttyDecoder törlése
    if (pRttyDecoder) {
        delete pRttyDecoder;
    }
}

/**
 * @brief Képernyő kirajzolása.
 * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
 */
void AmDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    DisplayBase::dawStatusLine();

    // RSSI skála kirajzoltatása
    pSMeter->drawSmeterScale();

    // RSSI aktuális érték
    si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = si4735.getCurrentRSSI();
    uint8_t snr = si4735.getCurrentSNR();
    pSMeter->showRSSI(rssi, snr, band.getCurrentBand().varData.currMod == FM);

    // Frekvencia
    float currFreq = band.getCurrentBand().varData.currFreq;  // A Rotary változtatásakor már eltettük a Band táblába
    pSevenSegmentFreq->freqDispl(currFreq);

    // RTTY/CW módválasztó gomb pozicionálása és kirajzolása
    drawRttyCwModeButton();

    // RTTY szövegterület hátterének és tartalmának kirajzolása
    drawRttyTextAreaBackground();

    // Gombok kirajzolása
    DisplayBase::drawScreenButtons();

    // MiniAudioFft korábbi példányának törlése, ha létezik
    if (pMiniAudioFft != nullptr) {
        delete pMiniAudioFft;
        pMiniAudioFft = nullptr;
    }

    // MiniAudioFft kirajzolása (kezdeti)
    if (config.data.miniAudioFftConfigAm >= 0.0f) {  // Csak akkor példányosítjuk, ha engedélyezve van
        using namespace DisplayConstants;

        pMiniAudioFft = new MiniAudioFft(tft, mini_fft_x, mini_fft_y, mini_fft_w, mini_fft_h,
                                         MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ,  // AM módhoz 6kHz
                                         config.data.miniAudioFftModeAm, config.data.miniAudioFftConfigAm);
        // Beállítjuk a kezdeti módot a configból
        pMiniAudioFft->setInitialMode(static_cast<MiniAudioFft::DisplayMode>(config.data.miniAudioFftModeAm));
        pMiniAudioFft->forceRedraw();
    }

    // Kezdeti RTTY szöveg kijelzés (üres)
    updateRttyTextDisplay();
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void AmDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {

    if (STREQ("AntC", event.label)) {
        // If zero, the tuning capacitor value is selected automatically.
        // AM - the tuning capacitance is manually set as 95 fF x ANTCAP + 7 pF.  ANTCAP manual range is 1–6143;
        // FM - the valid range is 0 to 191.

        // Antenna kapacitás állítása
        int maxValue = band.getCurrentBand().varData.currMod == FM ? Si4735Utils::MAX_ANT_CAP_FM : Si4735Utils::MAX_ANT_CAP_AM;

        int antCapValue = band.getCurrentBand().varData.antCap;  // Az aktuális érték a Band táblából

        DisplayBase::pDialog =
            new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Antenna Tuning capacitor"), F("Capacitor value [pF]:"), &antCapValue, (int)0, (int)maxValue,
                                  (int)0,  // A rotary encoder értéke lesz a step
                                  [this](int newValue) {
                                      // Az új érték beállítása a Band táblába
                                      band.getCurrentBand().varData.antCap = newValue;

                                      // Az új érték beállítása a Si4735-be
                                      Si4735Utils::si4735.setTuneFrequencyAntennaCapacitor(newValue);

                                      // Frissítjük a státusvonalban a kiírást
                                      DisplayBase::drawAntCapStatus(true);
                                  });
    } else if (STREQ("SSTV", event.label)) {
        // Képernyő váltás SSTV módra
        ::newDisplay = DisplayBase::DisplayType::sstv;
    } else if (event.id == pRttyCwModeButton->getId()) {  // RTTY/CW módválasztó gomb
        toggleRttyCwMode();                               // Váltás a módok között
    }
}

/**
 * @brief Touch (nem képrnyő button) esemény lekezelése.
 * A további gui elemek vezérléséhez
 */
bool AmDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {

    // Ha nincs dialog és van MiniAudioFft, akkor a MiniAudioFft kezelje az érintést
    if (!DisplayBase::pDialog && pMiniAudioFft && pMiniAudioFft->handleTouch(touched, tx, ty)) {  // Ellenőrizzük, hogy létezik-e
        return true;
    }

    // RTTY/CW módválasztó gomb touch kezelése (manuálisan)
    if (pRttyCwModeButton && pRttyCwModeButton->handleTouch(touched, tx, ty)) {
        return true;  // Kezeltük a gombnyomást, a processScreenButtonTouchEvent fogja feldolgozni
    }

    // A frekvencia kijelző kezeli a touch eseményeket SSB/CW módban
    uint8_t currMod = band.getCurrentBand().varData.currMod;
    if (currMod == LSB or currMod == USB or currMod == CW) {
        bool handled = pSevenSegmentFreq->handleTouch(touched, tx, ty);

        if (handled) {
            DisplayBase::drawStepStatus();  // Frissítjük a státusvonalban a kiírást
        }

        return handled;
    }

    return false;
}

/**
 * @brief Rotary encoder esemény lekezelése.
 */
bool AmDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {

    BandTable &currentBand = band.getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;
    uint16_t currentFrequency = si4735.getFrequency();

    bool isSsbCwMode = (currMod == LSB || currMod == USB || currMod == CW);

    if (isSsbCwMode) {
        // Manuális BFO Logika
        if (rtv::bfoOn) {  // --- BFO Finomhangolás ---
            // BFO módban a manuális BFO offsetet állítjuk
            int16_t step = config.data.currentBFOStep;  // BFO lépésköz a configból

            // Hozzáadás/kivonás a lépésközhöz az irány alapján
            config.data.currentBFOmanu += (encoderState.direction == RotaryEncoder::Direction::Up) ? step : -step;

            // Korlátozás +/- 999 Hz között (vagy amilyen tartományt szeretnél)
            config.data.currentBFOmanu = constrain(config.data.currentBFOmanu, -999, 999);

            // Kijelző frissítés kérése (a BFO érték megjelenítéséhez) a végén van

        } else {  // --- Normál SSB/CW Durva Hangolás (BFO OFF) ---
            if (encoderState.direction == RotaryEncoder::Direction::Up) {
                // Felfelé hangolásnál
                rtv::freqDec = rtv::freqDec - rtv::freqstep;  // rtv::freqstep itt 1000, 100 vagy 10 lehet
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) + (rtv::freqDec * -1);
                if (freqTot > (uint32_t)(currentBand.pConstData->maximumFreq * 1000)) {
                    si4735.setFrequency(currentBand.pConstData->maximumFreq);
                    rtv::freqDec = 0;
                }

                if (rtv::freqDec <= -16000) {  // Felfelé átfordulás ága
                    rtv::freqDec = rtv::freqDec + 16000;
                    int16_t freqPlus16 = currentFrequency + 16;
                    Si4735Utils::hardwareAudioMuteOn();
                    si4735.setFrequency(freqPlus16);
                    delay(10);
                }

            } else {
                // Lefelé hangolásnál
                rtv::freqDec = rtv::freqDec + rtv::freqstep;
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) - rtv::freqDec;
                if (freqTot < (uint32_t)(currentBand.pConstData->minimumFreq * 1000)) {
                    si4735.setFrequency(currentBand.pConstData->minimumFreq);
                    rtv::freqDec = 0;
                }

                if (rtv::freqDec >= 16000) {  // Lefelé átfordulás ága
                    rtv::freqDec = rtv::freqDec - 16000;
                    int16_t freqMin16 = currentFrequency - 16;
                    Si4735Utils::hardwareAudioMuteOn();
                    si4735.setFrequency(freqMin16);
                    delay(10);  // fontos, mert az BFO 0 értéknél elcsúszhat a beállított ferekvenciától a kijelzett érték
                }
            }
            config.data.currentBFO = rtv::freqDec;                 // freqDec a durva hangolás mértéke
            currentBand.varData.lastBFO = config.data.currentBFO;  // Mentsük el a durva hangolást
        }

        // --- Közös BFO beállítás és AGC ellenőrzés SSB/CW módhoz ---
        const int16_t cwBaseOffset = (currMod == CW) ? CW_SHIFT_FREQUENCY : 0;
        int16_t bfoToSet = cwBaseOffset + config.data.currentBFO + config.data.currentBFOmanu;
        si4735.setSSBBfo(bfoToSet);
        checkAGC();  // AGC ellenőrzése BFO beállítás után

    } else {
        // AM - sima frekvencia léptetés sávhatár ellenőrzéssel
        // Használjuk a rotary encoder gyorsítását (encoderState.value)
        // és a SÁVHOZ BEÁLLÍTOTT LÉPÉSKÖZT (currentBand.varData.currStep)

        // 1. Lépésköz lekérdezése az aktuális sávból (ez már kHz-ben van)
        uint16_t configuredStep = currentBand.varData.currStep;

        // 2. Teljes frekvenciaváltozás kiszámítása
        //    encoderState.value: Hány "logikai" lépést tett az enkóder (gyorsítással)
        //    configuredStep: Az egy logikai lépéshez tartozó frekvenciaváltozás (pl. 9 kHz)
        //    Fontos: int32_t-t használunk a túlcsordulás elkerülésére a szorzásnál
        int32_t change = (int32_t)encoderState.value * configuredStep;

        // 3. Új frekvencia kiszámítása
        //    Szintén int32_t-t használunk, hogy a sávhatárokon kívüli értékeket is kezelni tudjuk
        int32_t newFrequency = (int32_t)currentFrequency + change;

        // 4. Sávhatárok lekérdezése
        uint16_t minFreq = currentBand.pConstData->minimumFreq;
        uint16_t maxFreq = currentBand.pConstData->maximumFreq;

        // 5. Ellenőrzés és korlátozás a sávhatárokra
        if (newFrequency < minFreq) {
            newFrequency = minFreq;
        } else if (newFrequency > maxFreq) {
            newFrequency = maxFreq;
        }

        // 6. Új frekvencia beállítása, csak ha változott
        //    Az összehasonlításhoz és a setFrequency híváshoz vissza kell kasztolni uint16_t-re
        if ((uint16_t)newFrequency != currentFrequency) {
            si4735.setFrequency((uint16_t)newFrequency);
            // Mivel setFrequency-t használunk, az AGC-t is ellenőrizni kellhet,
            // bár valószínűleg AM módban az automatikus AGC jól működik.
            Si4735Utils::checkAGC();  // Szükség esetén
        }
    }

    // Elmentjük a beállított frekvenciát a Band táblába
    currentBand.varData.currFreq = si4735.getFrequency();

    // Beállítjuk, hogy kell majd új frekvenciakijelzés
    DisplayBase::frequencyChanged = true;

    return true;
}

/**
 * @brief Esemény nélküli display loop -> Adatok periódikus megjelenítése és RTTY dekódolás.

 */
void AmDisplay::displayLoop() {

    // Ha van dialóg, akkor nem frissítjük a komponenseket
    if (DisplayBase::pDialog != nullptr) {
        return;
    }

    BandTable &currentBand = band.getCurrentBand();
    // uint8_t currMod = currentBand.varData.currMod;  // Aktuális mód lekérdezése

    // Néhány adatot csak ritkábban frissítünk
    static uint32_t elapsedTimedValues = 0;  // Kezdőérték nulla
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {

        // RSSI
        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();
        pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);

        // Frissítjük az időbélyeget
        elapsedTimedValues = millis();
    }

    // A Frekvenciát azonnal frissítjuk, de csak ha változott
    if (DisplayBase::frequencyChanged) {
        pSevenSegmentFreq->freqDispl(currentBand.varData.currFreq);
        DisplayBase::frequencyChanged = false;  // Reset
    }

    // MiniAudioFft ciklus futtatása
    if (pMiniAudioFft != nullptr) {  // Ellenőrizzük, hogy létezik-e és nem nullptr
        pMiniAudioFft->loop();
    }
    // RTTY dekódolás futtatása, ha az RTTY mód aktív
    if (currentDecodeMode == DecodeMode::RTTY && pRttyDecoder && !rtv::muteStat) {
        char decodedChar = pRttyDecoder->decodeNextCharacter();
        if (decodedChar != '\0') {
            appendRttyCharacter(decodedChar);
        }
    }

    // CW dekódolás futtatása, ha a CW mód aktív (TODO)
    if (currentDecodeMode == DecodeMode::MORSE) {
        // TODO: CW dekódolási logika
    }
}

/**
 * @brief Kirajzolja az RTTY/CW módválasztó gombot.
 * Pozicionálja a szövegterület mellett.
 */
void AmDisplay::drawRttyCwModeButton() {
    if (pRttyCwModeButton) {
        // A gomb X pozíciója a szövegterület jobb szélétől jobbra
        uint16_t buttonX = rttyTextAreaX + rttyTextAreaW + 5;  // 5px rés
        // A gomb Y pozíciója a szövegterület tetejéhez igazítva
        uint16_t buttonY = rttyTextAreaY;
        pRttyCwModeButton->setPosition(buttonX, buttonY);
        pRttyCwModeButton->draw();
    }
}

/**
 * @brief Kirajzolja az RTTY szövegterület hátterét és keretét.
 */
void AmDisplay::drawRttyTextAreaBackground() {
    tft.fillRect(rttyTextAreaX, rttyTextAreaY, rttyTextAreaW, rttyTextAreaH, TFT_BLACK);
    tft.drawRect(rttyTextAreaX - 1, rttyTextAreaY - 1, rttyTextAreaW + 2, rttyTextAreaH + 2, TFT_DARKGREY);
}

/**
 * @brief Hozzáfűz egy karaktert az RTTY kijelző pufferéhez és frissíti a kijelzőt.
 * @param c A hozzáfűzendő karakter.
 */
void AmDisplay::appendRttyCharacter(char c) {
    if (c == '\n' || rttyCurrentLineBuffer.length() >= RTTY_LINE_BUFFER_SIZE - 1) {
        if (rttyCurrentLineIndex >= RTTY_MAX_TEXT_LINES - 1) {
            // Görgetés
            for (int i = 0; i < RTTY_MAX_TEXT_LINES - 1; ++i) {
                rttyDisplayLines[i] = rttyDisplayLines[i + 1];
            }
            rttyDisplayLines[RTTY_MAX_TEXT_LINES - 1] = rttyCurrentLineBuffer;
        } else {
            rttyDisplayLines[rttyCurrentLineIndex] = rttyCurrentLineBuffer;
            rttyCurrentLineIndex++;
        }
        rttyCurrentLineBuffer = "";
        if (c != '\n') {  // Ha sortörés miatt volt, de nem explicit '\n'
            rttyCurrentLineBuffer += c;
        }
    } else if (c >= 32 && c <= 126) {  // Csak nyomtatható karakterek
        rttyCurrentLineBuffer += c;
    }

    updateRttyTextDisplay();
}

/**
 * @brief Frissíti az RTTY szövegterület tartalmát a puffer alapján.
 */
void AmDisplay::updateRttyTextDisplay() {
    drawRttyTextAreaBackground();                  // Töröljük a területet
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);  // Jól olvasható szín
    tft.setTextDatum(TL_DATUM);                    // Bal felső igazítás
    tft.setFreeFont();                             // Standard font
    tft.setTextSize(1);                            // Vagy 2, ha nagyobb betűk kellenek

    uint16_t charHeight = tft.fontHeight();  // Betűmagasság lekérdezése
    if (charHeight == 0) charHeight = 16;    // Alapértelmezett, ha a fontHeight 0-t adna

    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        tft.drawString(rttyDisplayLines[i], rttyTextAreaX + 2, rttyTextAreaY + 2 + i * charHeight);
    }
    // Aktuális, még be nem fejezett sor kirajzolása
    tft.drawString(rttyCurrentLineBuffer, rttyTextAreaX + 2, rttyTextAreaY + 2 + rttyCurrentLineIndex * charHeight);
}

/**
 * @brief Vált az RTTY és CW dekódolási módok között.
 */
void AmDisplay::toggleRttyCwMode() {
    if (currentDecodeMode == DecodeMode::RTTY) {
        currentDecodeMode = DecodeMode::MORSE;
        if (pRttyCwModeButton) pRttyCwModeButton->setLabel("CW");
        // TODO: RTTY dekóder resetelése/leállítása
        // TODO: CW dekóder inicializálása/indítása
    } else {  // MORSE
        currentDecodeMode = DecodeMode::RTTY;
        if (pRttyCwModeButton) pRttyCwModeButton->setLabel("RTTY");
        // TODO: CW dekóder resetelése/leállítása
        // TODO: RTTY dekóder inicializálása/indítása
    }
    // Módválasztó gomb újrarajzolása
    if (pRttyCwModeButton) pRttyCwModeButton->draw();

    // Szövegterület törlése és frissítése az új módhoz
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        rttyDisplayLines[i] = "";
    }
    rttyCurrentLineBuffer = "";
    rttyCurrentLineIndex = 0;
    updateRttyTextDisplay();

    // Opcionális: Kiírhatjuk a módváltást a szövegterületre
    const char *modeMsg = (currentDecodeMode == DecodeMode::RTTY) ? "--- RTTY Mode ---\n" : "--- CW Mode ---\n";
    for (int i = 0; modeMsg[i] != '\0'; ++i) {
        appendRttyCharacter(modeMsg[i]);
    }
}
