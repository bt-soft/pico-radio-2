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
    uint8_t horizontalButtonCount = ARRAY_ITEM_COUNT(horizontalButtonsData);

    // MiniAudioFft és RTTY dekóder inicializálása
    // A targetSamplingFrequency 12000 Hz (2 * 6000 Hz) az AM FFT-hez és RTTY-hez.
    // Az RTTY dekóderhez a config.data.miniAudioFftConfigRtty erősítési beállítást használjuk.
    // Ezek a példányok Core0-n maradnak, de a feldolgozást Core1-re delegáljuk.
    pAudioProcessor = new AudioProcessor(config.data.miniAudioFftConfigRtty, AUDIO_INPUT_PIN, MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ * 2.0f);  // 12000 Hz
    pRttyDecoder = new RttyDecoder(*pAudioProcessor);  // RTTY dekóder inicializálása az AudioProcessorral
    pCwDecoder = new CwDecoder(AUDIO_INPUT_PIN);       // CW dekóder inicializálása

    // Szövegterület és módváltó gombok pozícióinak kiszámítása
    rttyTextAreaX = DECODER_TEXT_AREA_X_START;
    rttyTextAreaY = 150;
    rttyTextAreaW = 330;
    rttyTextAreaH = 80;  // Kezdeti magasság

    // A jobb oldali fő függőleges gombsor X pozíciójának meghatározása
    TftButton *firstVerticalButton = DisplayBase::findButtonByLabel("Mute");
    uint16_t mainVerticalButtonsStartX = tft.width() - SCRN_BTN_W - SCREEN_VBTNS_X_MARGIN;
    if (firstVerticalButton) {
        mainVerticalButtonsStartX = firstVerticalButton->getX();
    }
    decodeModeButtonsX = mainVerticalButtonsStartX - DECODER_MODE_BTN_GAP_X - DECODER_MODE_BTN_W;

    // Dekódolt szöveg pufferek inicializálása
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        rttyDisplayLines[i] = "";
    }
    rttyCurrentLineBuffer = "";

    // Dekódolási módváltó gombok létrehozása
    uint8_t nextButtonId = SCRN_HBTNS_ID_START + horizontalButtonCount;
    decoderModeStartId_ = nextButtonId;

    std::vector<String> decoderLabels = {"Off", "RTTY", "CW"};
    decoderModeGroup.createButtons(decoderLabels, nextButtonId);
    decoderModeGroup.selectButtonByIndex(0);

    // Horizontális képernyőgombok legyártása
    DisplayBase::buildHorizontalScreenButtons(horizontalButtonsData, ARRAY_ITEM_COUNT(horizontalButtonsData), true);

    clearRttyTextBufferAndDisplay();
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
    if (pRttyDecoder) delete pRttyDecoder;
    if (pCwDecoder) delete pCwDecoder;
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
    drawRttyTextAreaBackground();
    updateRttyTextDisplay();
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

    // RTTY dekódolás Core1-re delegálva
    if (currentDecodeMode == DecodeMode::RTTY && !rtv::muteStat) {
        if (multicore_fifo_wready()) {
            DEBUG("Core0: Sending CORE1_CMD_PROCESS_AUDIO_RTTY\n");
            multicore_fifo_push_blocking(CORE1_CMD_PROCESS_AUDIO_RTTY);
        }
        if (multicore_fifo_rvalid()) {
            char decodedChar = static_cast<char>(multicore_fifo_pop_blocking());
            DEBUG("Core0: RTTY decodedChar received: '%c' (%d)\n", decodedChar, decodedChar);
            if (decodedChar != '\0') {
                appendRttyCharacter(decodedChar);
            }
        }
    }
    
    // CW dekódolás Core1-re delegálva.  A Core0 vár a Core1 válaszára
    if (currentDecodeMode == DecodeMode::MORSE && !rtv::muteStat) {
        // A multicore_fifo_push_blocking() maga kezeli a várakozást, ha a FIFO tele van.
        // A wready() ellenőrzés itt felesleges lehet.

        DEBUG("Core0: Sending CORE1_CMD_PROCESS_AUDIO_CW to Core1\n");
        multicore_fifo_push_blocking(CORE1_CMD_PROCESS_AUDIO_CW);

        // Várakozás a válaszra (dekódolt karakter) a Core1-től
        unsigned long startTimeWait = millis();
        bool received = false;
        while (millis() - startTimeWait < 20) {  // Timeout növelve 20ms-ra teszteléshez
            if (multicore_fifo_rvalid()) {
                char decodedChar = static_cast<char>(multicore_fifo_pop_blocking());
                DEBUG("Core0: CW decodedChar received from Core1: '%c' (%d)\n", decodedChar, decodedChar);
                if (decodedChar != '\0') {
                    appendRttyCharacter(decodedChar);
                }
                received = true;
                break;
            }
            delayMicroseconds(100);  // Rövid várakozás a CPU tehermentesítésére
        }
        if (!received) {
            DEBUG("Core0: Timeout waiting for CW char from Core1.\n");
        }
    }
}

/**
 * @brief Kirajzolja a dekódolási módválasztó gombokat.
 */
void AmDisplay::drawDecodeModeButtons() {
    decoderModeGroup.setPositionsAndSize(decodeModeButtonsX, rttyTextAreaY, DECODER_MODE_BTN_W, DECODER_MODE_BTN_H, DECODER_MODE_BTN_GAP_Y);
    decoderModeGroup.draw();
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
 */
void AmDisplay::appendRttyCharacter(char c) {
    DEBUG("AmDisplay::appendRttyCharacter: kapott karakter: '%c' (%d), buffer: '%s'\n", c, c, rttyCurrentLineBuffer.c_str());
    // Először adjuk hozzá a karaktert a bufferhez, ha nyomtatható és nem '\n'
    if (c >= 32 && c <= 126 && c != '\n') {
        rttyCurrentLineBuffer += c;
        DEBUG("AmDisplay::appendRttyCharacter: buffer after append: '%s'\n", rttyCurrentLineBuffer.c_str());
    }
    // Ezután ellenőrizzük, hogy sort kell-e váltani
    if (c == '\n' || rttyCurrentLineBuffer.length() >= RTTY_LINE_BUFFER_SIZE - 1) {
        DEBUG("AmDisplay::appendRttyCharacter: line break, index=%d, buffer='%s'\n", rttyCurrentLineIndex, rttyCurrentLineBuffer.c_str());
        if (rttyCurrentLineIndex >= RTTY_MAX_TEXT_LINES - 1) {
            for (int i = 0; i < RTTY_MAX_TEXT_LINES - 1; ++i) {
                rttyDisplayLines[i] = rttyDisplayLines[i + 1];
            }
            rttyDisplayLines[RTTY_MAX_TEXT_LINES - 1] = rttyCurrentLineBuffer;
        } else {
            rttyDisplayLines[rttyCurrentLineIndex] = rttyCurrentLineBuffer;
            rttyCurrentLineIndex++;
        }
        rttyCurrentLineBuffer = "";
    }
    updateRttyTextDisplay();
}

/**
 * @brief Frissíti az RTTY szövegterület tartalmát a puffer alapján.
 */
void AmDisplay::updateRttyTextDisplay() {
    DEBUG("AmDisplay::updateRttyTextDisplay: index=%d, buffer='%s'\n", rttyCurrentLineIndex, rttyCurrentLineBuffer.c_str());
    drawRttyTextAreaBackground();
    tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont();
    tft.setTextSize(1);
    uint16_t charHeight = tft.fontHeight();
    if (charHeight == 0) charHeight = 16;
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        if (i == rttyCurrentLineIndex) {
            if (!rttyCurrentLineBuffer.isEmpty()) {
                tft.drawString(rttyCurrentLineBuffer, rttyTextAreaX + 2, rttyTextAreaY + 2 + i * charHeight);
            }
        } else {
            if (!rttyDisplayLines[i].isEmpty()) {
                tft.drawString(rttyDisplayLines[i], rttyTextAreaX + 2, rttyTextAreaY + 2 + i * charHeight);
            }
        }
    }
}

/**
 * @brief Beállítja a dekódolási módot és frissíti a gombok állapotát.
 */
void AmDisplay::setDecodeMode(DecodeMode newMode) {
    if (currentDecodeMode == newMode && newMode != DecodeMode::RTTY) {  // RTTY-nél mindig fusson le az auto-detect miatt
                                                                        // Ha RTTY-re váltunk, akkor is le kell futnia, hogy az auto-detect elinduljon
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
            if (pRttyDecoder) {                   // Csak akkor, ha létezik
                pRttyDecoder->startAutoDetect();  // Automatikus frekvencia detektálás indítása
            }
            break;
        case DecodeMode::MORSE:
            decoderModeGroup.selectButtonByIndex(2);
            core1_cmd_set_mode = CORE1_CMD_SET_MODE_CW;
            if (pCwDecoder) {
                pCwDecoder->resetDecoderState();
            }
            break;
        default:
            break;
    }

    // Parancs küldése Core1-nek a módváltásról
    DEBUG("Core0: Sending mode change command to Core1: 0x%lX\n", static_cast<uint32_t>(core1_cmd_set_mode));
    multicore_fifo_push_blocking(static_cast<uint32_t>(core1_cmd_set_mode));

    clearRttyTextBufferAndDisplay();

    // const char *modeMsg = nullptr;
    // if (currentDecodeMode == DecodeMode::RTTY)
    //     modeMsg = "--- RTTY Mode ---\n";
    // else if (currentDecodeMode == DecodeMode::MORSE)
    //     modeMsg = "--- CW Mode ---\n";
    // // else if (currentDecodeMode == DecodeMode::OFF) modeMsg = "--- Decoder Off ---\n"; // Opcionális

    // if (modeMsg) {
    //     for (int i = 0; modeMsg[i] != '\0'; ++i) {
    //         // Itt az appendRttyCharacter már nem frissít minden karakter után,
    //         // ezért a ciklus után kell egy updateRttyTextDisplay()
    //         // De mivel az appendRttyCharacter most már frissít, ez így jó.
    //         appendRttyCharacter(modeMsg[i]);
    //     }
    //     // updateRttyTextDisplay(); // Ha az appendRttyCharacter nem frissítene
    // }
}

/**
 * @brief Törli az RTTY szöveg puffereit és frissíti a kijelzőt.
 */
void AmDisplay::clearRttyTextBufferAndDisplay() {
    for (int i = 0; i < RTTY_MAX_TEXT_LINES; ++i) {
        rttyDisplayLines[i] = "";
    }
    rttyCurrentLineBuffer = "";
    rttyCurrentLineIndex = 0;
    updateRttyTextDisplay();
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
