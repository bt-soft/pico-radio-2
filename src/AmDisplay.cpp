#include "AmDisplay.h"

#include <Arduino.h>

#include "core_communication.h"  // Parancsok definíciója
#include "defines.h"             // DEBUG makróhoz
#include "pico/multicore.h"      // FIFO kommunikációhoz

/**
 * @brief Konstruktor az AmDisplay osztályhoz.
 * @param tft Referencia a TFT kijelző objektumra.
 * @param si4735 Referencia az SI4735 rádió chip objektumra.
 * @param band Referencia a Band objektumra.
 */
AmDisplay::AmDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band)
    : DisplayBase(tft, si4735, band), pMiniAudioFft(nullptr), decoderModeGroup(tft), currentDecodeMode(DecodeMode::OFF) {  // currentDecodeMode inicializálása OFF-ra

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
    uint8_t horizontalButtonCount = ARRAY_ITEM_COUNT(horizontalButtonsData);  // A targetSamplingFrequency 12000 Hz (2 * 6000 Hz) az AM FFT-hez, CW és RTTY dekódoláshoz.
    pAudioProcessor = new AudioProcessor(config.data.miniAudioFftConfigRtty, AUDIO_INPUT_PIN,
                                         MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ * 2.0f);  // 12000 Hz

    // Szövegterület és módváltó gombok pozícióinak kiszámítása
    decodedTextAreaX = DECODER_TEXT_AREA_X_START;
    decodedTextAreaY = 150;  // A S-Meter alatt
    decodedTextAreaW = 330;  // Széleség
    decodedTextAreaH = 80;   // Magasság

    // A jobb oldali fő függőleges gombsor X pozíciójának meghatározása
    TftButton *firstVerticalButton = DisplayBase::findButtonByLabel("Mute");
    uint16_t mainVerticalButtonsStartX = tft.width() - SCRN_BTN_W - SCREEN_VBTNS_X_MARGIN;
    if (firstVerticalButton) {
        mainVerticalButtonsStartX = firstVerticalButton->getX();
    }
    decodeModeButtonsX = mainVerticalButtonsStartX - (DECODER_MODE_BTN_GAP_X * 2) - DECODER_MODE_BTN_W;  // Visszaállítva az eredeti diff alapján.

    // Dekódolt szöveg pufferek inicializálása (CW és RTTY)
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        decodedTextDisplayLines[i] = "";
    }
    decodedTextCurrentLineBuffer = "";

    // Dekódolási módváltó gombok létrehozása
    uint8_t nextButtonId = SCRN_HBTNS_ID_START + horizontalButtonCount;
    decoderModeStartId_ = nextButtonId;

    // Karakter magasság inicializálása a dekóderhez
    tft.setFreeFont();
    tft.setTextSize(1);
    decoderCharHeight_ = tft.fontHeight();
    if (decoderCharHeight_ == 0) decoderCharHeight_ = 16;  // Alapértelmezett érték, ha a font magassága 0

    std::vector<String> decoderLabels = {"Off", "RTTY", "CW"};
    decoderModeGroup.createButtons(decoderLabels, nextButtonId);
    decoderModeGroup.selectButtonByIndex(0);

    // Horizontális képernyőgombok legyártása
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), true);
}

/**
 * @brief Destruktor az AmDisplay osztályhoz.
 * Felszabadítja a dinamikusan allokált erőforrásokat.
 */
AmDisplay::~AmDisplay() {
    DEBUG("AmDisplay::~AmDisplay\n");
    if (pSMeter) delete pSMeter;
    if (pSevenSegmentFreq) delete pSevenSegmentFreq;
    if (pMiniAudioFft) delete pMiniAudioFft;
    if (pAudioProcessor) delete pAudioProcessor;  // AudioProcessor törlése
}

/**
 * @brief Képernyő kirajzolása.
 */
void AmDisplay::drawScreen() {
    tft.setFreeFont();
    tft.fillScreen(TFT_COLOR_BACKGROUND);
    DisplayBase::dawStatusLine();
    pSMeter->drawSmeterScale();
    si4735.getCurrentReceivedSignalQuality();
    uint8_t rssi = si4735.getCurrentRSSI();
    uint8_t snr = si4735.getCurrentSNR();
    pSMeter->showRSSI(rssi, snr, band.getCurrentBand().varData.currMod == FM);
    float currFreq = band.getCurrentBand().varData.currFreq;
    pSevenSegmentFreq->freqDispl(currFreq);
    drawDecodedTextAreaBackground();
    updateDecodedTextDisplay();
    drawDecodeModeButtons();
    DisplayBase::drawScreenButtons();

    if (pMiniAudioFft != nullptr) {
        delete pMiniAudioFft;
        pMiniAudioFft = nullptr;
    }
    if (config.data.miniAudioFftConfigAm >= 0.0f) {
        using namespace DisplayConstants;
        pMiniAudioFft = new MiniAudioFft(tft, mini_fft_x, mini_fft_y, mini_fft_w, mini_fft_h, MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ, config.data.miniAudioFftModeAm,
                                         config.data.miniAudioFftConfigAm);
        pMiniAudioFft->setInitialMode(static_cast<MiniAudioFft::DisplayMode>(config.data.miniAudioFftModeAm));
        pMiniAudioFft->forceRedraw();
    }
}

/**
 * Képernyő menügomb esemény feldolgozása
 */
void AmDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {
    if (STREQ("AntC", event.label)) {
        int maxValue = band.getCurrentBand().varData.currMod == FM ? Si4735Utils::MAX_ANT_CAP_FM : Si4735Utils::MAX_ANT_CAP_AM;
        int antCapValue = band.getCurrentBand().varData.antCap;
        DisplayBase::pDialog = new ValueChangeDialog(this, DisplayBase::tft, 270, 150, F("Antenna Tuning capacitor"), F("Capacitor value [pF]:"), &antCapValue, (int)0,
                                                     (int)maxValue, (int)0, [this](int newValue) {
                                                         band.getCurrentBand().varData.antCap = newValue;
                                                         Si4735Utils::si4735.setTuneFrequencyAntennaCapacitor(newValue);
                                                         DisplayBase::drawAntCapStatus(true);
                                                     });
    } else if (STREQ("SSTV", event.label)) {
        ::newDisplay = DisplayBase::DisplayType::sstv;
    }
}

/**
 * @brief Touch (nem képrnyő button) esemény lekezelése.
 */
bool AmDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    if (!DisplayBase::pDialog && pMiniAudioFft && pMiniAudioFft->handleTouch(touched, tx, ty)) {
        return true;
    }
    uint8_t pressedRadioBtnId = 0xFF;
    if (decoderModeGroup.handleTouch(touched, tx, ty, pressedRadioBtnId)) {
        if (pressedRadioBtnId != 0xFF) {
            setDecodeModeBasedOnButtonId(pressedRadioBtnId);
        }
        return true;
    }
    uint8_t currMod = band.getCurrentBand().varData.currMod;
    if (currMod == LSB || currMod == USB || currMod == CW) {
        bool handled = pSevenSegmentFreq->handleTouch(touched, tx, ty);
        if (handled) {
            DisplayBase::drawStepStatus();
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
        if (rtv::bfoOn) {
            int16_t step = config.data.currentBFOStep;
            config.data.currentBFOmanu += (encoderState.direction == RotaryEncoder::Direction::Up) ? step : -step;
            config.data.currentBFOmanu = constrain(config.data.currentBFOmanu, -999, 999);
        } else {
            if (encoderState.direction == RotaryEncoder::Direction::Up) {
                rtv::freqDec = rtv::freqDec - rtv::freqstep;
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) + (rtv::freqDec * -1);
                if (freqTot > (uint32_t)(currentBand.pConstData->maximumFreq * 1000)) {
                    si4735.setFrequency(currentBand.pConstData->maximumFreq);
                    rtv::freqDec = 0;
                }
                if (rtv::freqDec <= -16000) {
                    rtv::freqDec = rtv::freqDec + 16000;
                    int16_t freqPlus16 = currentFrequency + 16;
                    Si4735Utils::hardwareAudioMuteOn();
                    si4735.setFrequency(freqPlus16);
                    delay(10);
                }
            } else {
                rtv::freqDec = rtv::freqDec + rtv::freqstep;
                uint32_t freqTot = (uint32_t)(currentFrequency * 1000) - rtv::freqDec;
                if (freqTot < (uint32_t)(currentBand.pConstData->minimumFreq * 1000)) {
                    si4735.setFrequency(currentBand.pConstData->minimumFreq);
                    rtv::freqDec = 0;
                }
                if (rtv::freqDec >= 16000) {
                    rtv::freqDec = rtv::freqDec - 16000;
                    int16_t freqMin16 = currentFrequency - 16;
                    Si4735Utils::hardwareAudioMuteOn();
                    si4735.setFrequency(freqMin16);
                    delay(10);
                }
            }
            config.data.currentBFO = rtv::freqDec;
            currentBand.varData.lastBFO = config.data.currentBFO;
        }
        const int16_t cwBaseOffset = (currMod == CW) ? CW_SHIFT_FREQUENCY : 0;
        int16_t bfoToSet = cwBaseOffset + config.data.currentBFO + config.data.currentBFOmanu;
        si4735.setSSBBfo(bfoToSet);
        checkAGC();
    } else {
        uint16_t configuredStep = currentBand.varData.currStep;
        int32_t change = (int32_t)encoderState.value * configuredStep;
        int32_t newFrequency = (int32_t)currentFrequency + change;
        uint16_t minFreq = currentBand.pConstData->minimumFreq;
        uint16_t maxFreq = currentBand.pConstData->maximumFreq;
        if (newFrequency < minFreq)
            newFrequency = minFreq;
        else if (newFrequency > maxFreq)
            newFrequency = maxFreq;
        if ((uint16_t)newFrequency != currentFrequency) {
            si4735.setFrequency((uint16_t)newFrequency);
            Si4735Utils::checkAGC();
        }
    }
    currentBand.varData.currFreq = si4735.getFrequency();
    DisplayBase::frequencyChanged = true;
    return true;
}

/**
 * @brief Esemény nélküli display loop -> Adatok periódikus megjelenítése és dekódolás.
 */
void AmDisplay::displayLoop() {
    if (DisplayBase::pDialog != nullptr) {
        return;
    }
    BandTable &currentBand = band.getCurrentBand();
    static uint32_t elapsedTimedValues = 0;
    if ((millis() - elapsedTimedValues) >= SCREEN_COMPS_REFRESH_TIME_MSEC) {
        si4735.getCurrentReceivedSignalQuality();
        uint8_t rssi = si4735.getCurrentRSSI();
        uint8_t snr = si4735.getCurrentSNR();
        pSMeter->showRSSI(rssi, snr, currentBand.varData.currMod == FM);
        elapsedTimedValues = millis();
    }
    if (DisplayBase::frequencyChanged) {
        pSevenSegmentFreq->freqDispl(currentBand.varData.currFreq);
        DisplayBase::frequencyChanged = false;
    }
    if (pMiniAudioFft != nullptr) {
        pMiniAudioFft->loop();
    }

    // Csak ha nincs némítva akkor dekódolunk CW-t és RTTY-t
    if (!rtv::muteStat) {
        decodeCwAndRttyText();
    }
}

/**
 * @brief Kommunikál a Core1-gyel, hogy dekódolja a CW vagy RTTY szöveget.
 */
void AmDisplay::decodeCwAndRttyText() {

    // Parancs küldése a Core1-nek, hogy adjon egy dekódolt karaktert
    uint32_t getCommand = currentDecodeMode == DecodeMode::MORSE ? CORE1_CMD_GET_CW_CHAR : CORE1_CMD_GET_RTTY_CHAR;
    if (!rp2040.fifo.push_nb(getCommand)) {
        Utils::beepError();
        DEBUG("Core0: getCommand NOT sent to Core1, FIFO full\n");
        return;  // Ha a FIFO tele van, akkor nem küldjük el a parancsot
    }

    // Várakozás a válaszra (dekódolt karakter) a Core1-től
    char decodedChar = '\0';  // Dekódolt karakter inicializálása
    bool char_popped = false;
    unsigned long startTimeWait = millis();
    while (millis() - startTimeWait < 5) {  // Timeout 5 ms. Érdemes lehet konstanssá tenni.
        if (rp2040.fifo.available() > 0) {
            uint32_t raw_command;
            if (rp2040.fifo.pop_nb(&raw_command)) {
                decodedChar = static_cast<char>(raw_command);
                // DEBUG("Core0: CW decodedChar received: '%c' (%d)\n", decodedChar, decodedChar);
                if (decodedChar != '\0') {
                    appendDecodedCharacter(decodedChar);
                }
                char_popped = true;
                break;  // Kilépés a ciklusból, ha sikeresen olvastunk
            }
        }
        // Ha nem volt elérhető karakter, a ciklus folytatódik a timeoutig.
        // Egy rövid várakozás itt csökkentheti a CPU terhelést.
        delayMicroseconds(50);  // Opcionális: rövid várakozás
    }
}

/**
 * @brief Kirajzolja a dekódolási módválasztó gombokat.
 */
void AmDisplay::drawDecodeModeButtons() {
    decoderModeGroup.setPositionsAndSize(decodeModeButtonsX, decodedTextAreaY, DECODER_MODE_BTN_W, DECODER_MODE_BTN_H, DECODER_MODE_BTN_GAP_Y);
    decoderModeGroup.draw();
}

/**
 * @brief Kirajzolja a dekódolt szövegterület hátterét és keretét.
 */
void AmDisplay::drawDecodedTextAreaBackground() {
    tft.fillRect(decodedTextAreaX, decodedTextAreaY, decodedTextAreaW, decodedTextAreaH, TFT_BLACK);
    tft.drawRect(decodedTextAreaX - 1, decodedTextAreaY - 1, decodedTextAreaW + 2, decodedTextAreaH + 2, TFT_DARKGREY);
}

/**
 * @brief Csak az aktuális beviteli sort rajzolja újra a dekódolt szöveg területén.
 * Minimalizálja a villogást karakterenkénti hozzáfűzéskor.
 */
void AmDisplay::redrawCurrentInputLine() {
    // decoderCharHeight_ már tagváltozó és inicializálva van

    // Aktuális sor Y pozíciójának kiszámítása
    // Biztosítjuk, hogy a decodedTextCurrentLineIndex érvényes legyen a rajzoláshoz
    if (decodedTextCurrentLineIndex >= RTTY_MAX_TEXT_LINES) {
        // Ez az eset ideális esetben nem fordulhat elő a sortörési logika miatt.
        // Visszaesésként teljes frissítést végzünk.
        updateDecodedTextDisplay();
        return;
    }
    uint16_t yPos = decodedTextAreaY + 2 + decodedTextCurrentLineIndex * (decoderCharHeight_ + DECODER_LINE_GAP);

    // Ellenőrizzük, hogy a rajzolás a területen belül marad-e
    if (yPos + decoderCharHeight_ > decodedTextAreaY + decodedTextAreaH) {
        updateDecodedTextDisplay();  // Visszaesés, ha kívül esne
        return;
    }

    // Csak az aktuális sor hátterének törlése
    tft.fillRect(decodedTextAreaX + 2, yPos, decodedTextAreaW - 4, decoderCharHeight_, TFT_BLACK);

    // Aktuális beviteli puffer string kirajzolása
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont();  // Biztosítjuk a helyes fontot
    tft.setTextSize(1);
    if (!decodedTextCurrentLineBuffer.isEmpty()) {
        tft.drawString(decodedTextCurrentLineBuffer, decodedTextAreaX + 2, yPos);
    }
}

/**
 * @brief Hozzáfűz egy karaktert a dekódolt szöveg kijelző pufferéhez és frissíti a kijelzőt.
 */
void AmDisplay::appendDecodedCharacter(char c) {
    bool needs_full_redraw = false;
    bool char_appended = false;

    if (c == '\n') {  // Explicit sortörés
        needs_full_redraw = true;
    } else if (c >= 32 && c <= 126) {  // Nyomtatható karakter
        if (decodedTextCurrentLineBuffer.length() < RTTY_LINE_BUFFER_SIZE - 1) {
            decodedTextCurrentLineBuffer += c;
            char_appended = true;
            // Ellenőrizzük, hogy a puffer most telt-e meg
            if (decodedTextCurrentLineBuffer.length() >= RTTY_LINE_BUFFER_SIZE - 1) {
                needs_full_redraw = true;  // Sortörésként kezeljük
                char_appended = false;     // A teljes újrarajzolás kezeli
            }
        } else {  // A puffer már tele volt, ez a karakter nem fér bele, sortörésként kezeljük
            needs_full_redraw = true;
        }
    } else {
        // Nem nyomtatható karakter (és nem '\n'), nem csinálunk semmit
        return;
    }

    if (needs_full_redraw) {
        // Sortörés vagy puffer megtelt: véglegesítjük az aktuális sort
        if (decodedTextCurrentLineIndex >= RTTY_MAX_TEXT_LINES - 1) {  // Görgetés szükséges
            for (int i = 0; i < RTTY_MAX_TEXT_LINES - 1; ++i) {
                decodedTextDisplayLines[i] = decodedTextDisplayLines[i + 1];
            }
            decodedTextDisplayLines[RTTY_MAX_TEXT_LINES - 1] = decodedTextCurrentLineBuffer;
        } else {  // Nincs görgetés, csak a következő sorra lépünk
            decodedTextDisplayLines[decodedTextCurrentLineIndex] = decodedTextCurrentLineBuffer;
            decodedTextCurrentLineIndex++;
        }
        decodedTextCurrentLineBuffer = "";
        updateDecodedTextDisplay();  // Teljes újrarajzolás
    } else if (char_appended) {
        // Karakter hozzáfűzve, nincs sortörés
        redrawCurrentInputLine();  // Csak az aktuális sor újrarajzolása
    }
}

/**
 * @brief Frissíti a dekódolt szövegterület tartalmát a puffer alapján.
 */
void AmDisplay::updateDecodedTextDisplay() {
    drawDecodedTextAreaBackground();  // Teljes háttér törlése

    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont();
    tft.setTextSize(1);
    // decoderCharHeight_ tagváltozót használunk

    // Az összes "lezárt" sor kirajzolása a decodedTextDisplayLines tömbből
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        uint16_t line_y_start = decodedTextAreaY + 2 + i * (decoderCharHeight_ + DECODER_LINE_GAP);
        // Ellenőrizzük, hogy a sor a megjelenítési területen belül van-e
        if (line_y_start + decoderCharHeight_ <= decodedTextAreaY + decodedTextAreaH) {
            // Ha az 'i' index kisebb, mint az aktuális sor indexe, akkor az egy korábbi, lezárt sor.
            // Vagy ha az 'i' index megegyezik az aktuális sor indexével, de a decodedTextDisplayLines[i] nem üres
            // (ez akkor fordulhat elő, ha pl. egy karakter hozzáfűzése után azonnal teljes frissítés történik,
            // bár ez a jelenlegi logikával nem valószínű).
            // A legegyszerűbb, ha minden sort kirajzolunk a decodedTextDisplayLines-ból, ami nem üres,
            // és nem az aktuális szerkesztés alatt álló sor.
            if (i != decodedTextCurrentLineIndex && !decodedTextDisplayLines[i].isEmpty()) {
                tft.drawString(decodedTextDisplayLines[i], decodedTextAreaX + 2, line_y_start);
            }
        }
    }

    // Az aktuálisan szerkesztett sor (decodedTextCurrentLineBuffer) kirajzolása
    // a decodedTextCurrentLineIndex által mutatott pozícióba.
    uint16_t currentLineYPos = decodedTextAreaY + 2 + decodedTextCurrentLineIndex * (decoderCharHeight_ + DECODER_LINE_GAP);
    if (currentLineYPos + decoderCharHeight_ <= decodedTextAreaY + decodedTextAreaH) {  // Határellenőrzés
        if (!decodedTextCurrentLineBuffer.isEmpty()) {
            tft.drawString(decodedTextCurrentLineBuffer, decodedTextAreaX + 2, currentLineYPos);
        }
        // Ha a decodedTextCurrentLineBuffer üres (pl. sortörés után), akkor a hátteret a drawDecodedTextAreaBackground már törölte.
    }
}

/**
 * @brief Törli a dekódolt szöveg pufferét, de nem frissíti a kijelzőt.
 */
void AmDisplay::clearDecodedTextBufferOnly() {
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        decodedTextDisplayLines[i] = "";
    }
    decodedTextCurrentLineBuffer = "";
    decodedTextCurrentLineIndex = 0;
}

/**
 * @brief Beállítja a dekódolási módot és frissíti a gombok állapotát.
 */
void AmDisplay::setDecodeMode(DecodeMode newMode) {
    if (currentDecodeMode == newMode && newMode != DecodeMode::RTTY) {
        if (newMode != DecodeMode::RTTY) return;
    }

    currentDecodeMode = newMode;
    Core1Command core1_cmd_set_mode = CORE1_CMD_SET_MODE_OFF;  // Alapértelmezett

    switch (newMode) {
        case DecodeMode::OFF:
            decoderModeGroup.selectButtonByIndex(0);
            core1_cmd_set_mode = CORE1_CMD_SET_MODE_OFF;
            break;

        case DecodeMode::RTTY:
            decoderModeGroup.selectButtonByIndex(1);
            core1_cmd_set_mode = CORE1_CMD_SET_MODE_RTTY;
            break;

        case DecodeMode::MORSE:
            decoderModeGroup.selectButtonByIndex(2);
            core1_cmd_set_mode = CORE1_CMD_SET_MODE_CW;
            break;
        default:
            break;
    }

    // Parancs küldése Core1-nek a módváltásról
    if (!rp2040.fifo.push_nb(static_cast<uint32_t>(core1_cmd_set_mode))) {
        Utils::beepError();
        DEBUG("Core0: Command NOT sent to Core1, FIFO full\n");
        return;  // Ha a FIFO tele van, akkor nem küldjük el a parancsot    } // Ez a zárójel itt hibásnak tűnik

        // Ha kikapcsoljuk a módot, akkor nem töröljük a területet
        if (core1_cmd_set_mode != CORE1_CMD_SET_MODE_OFF) {  // Ezt a blokkot a FIFO push elé kellene vinni, vagy a return után nem fut le.
                                                             // De a logikája az, hogy csak akkor töröljön, ha sikeres volt a parancsküldés.
                                                             // A jelenlegi formában ez a blokk sosem fut le, ha a FIFO push sikertelen.
                                                             // Jobb lenne a clearDecodedTextBufferAndDisplay()-t a sikeres parancsküldés utánra tenni.
            clearDecodedTextBufferAndDisplay();
        }
    }  // Itt kellene lennie a zárójelnek a FIFO push feltételhez.
       // A javított verzió:
    else {  // Ha a parancsküldés sikeres volt
        if (core1_cmd_set_mode != CORE1_CMD_SET_MODE_OFF) {
            clearDecodedTextBufferAndDisplay();
        }
    }
}

/**
 * @brief Törli a dekódolt szöveg puffereit és frissíti a kijelzőt.
 */
void AmDisplay::clearDecodedTextBufferAndDisplay() {
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        decodedTextDisplayLines[i] = "";
    }
    decodedTextCurrentLineBuffer = "";
    decodedTextCurrentLineIndex = 0;
    updateDecodedTextDisplay();
}

/**
 * @brief Beállítja a dekódolási módot egy RadioButton ID alapján.
 */
void AmDisplay::setDecodeModeBasedOnButtonId(uint8_t buttonId) {
    if (buttonId == decoderModeStartId_ + 0)
        setDecodeMode(DecodeMode::OFF);
    else if (buttonId == decoderModeStartId_ + 1)
        setDecodeMode(DecodeMode::RTTY);
    else if (buttonId == decoderModeStartId_ + 2)
        setDecodeMode(DecodeMode::MORSE);
}
