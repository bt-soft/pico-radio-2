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

    // A targetSamplingFrequency 12000 Hz (2 * 6000 Hz) az AM FFT-hez és RTTY-hez.
    pAudioProcessor = new AudioProcessor(config.data.miniAudioFftConfigRtty, AUDIO_INPUT_PIN, MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_AM_HZ * 2.0f);  // 12000 Hz

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
            multicore_fifo_push_blocking(CORE1_CMD_GET_RTTY_CHAR);
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

        // Parancs küldése a Core1-nek, hogy adjon egy dekódolt CW karaktert
        if (!rp2040.fifo.push_nb(CORE1_CMD_GET_CW_CHAR)) {
            Utils::beepError();
            DEBUG("Core0: CW command NOT sent to Core1, FIFO full\n");
            return;  // Ha a FIFO tele van, akkor nem küldjük el a parancsot
        }

        // Várakozás a válaszra (dekódolt karakter) a Core1-től
        char decodedChar = '\0';  // Dekódolt karakter inicializálása
        unsigned long startTimeWait = millis();
        while (millis() - startTimeWait < 5) {  // Timeout 5 ms
            if (rp2040.fifo.available() > 0) {

                uint32_t raw_command;
                if (rp2040.fifo.pop_nb(&raw_command)) {
                    decodedChar = static_cast<char>(raw_command);
                    // DEBUG("Core0: CW decodedChar received: '%c' (%d)\n", decodedChar, decodedChar);
                    if (decodedChar != '\0') {
                        appendRttyCharacter(decodedChar);
                    }
                }
            }
            break;
        }
        delayMicroseconds(100);  // Rövid várakozás a CPU tehermentesítésére
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

    // Először adjuk hozzá a karaktert a bufferhez, ha nyomtatható és nem '\n'
    if (c >= 32 && c <= 126 && c != '\n') {
        rttyCurrentLineBuffer += c;
    }

    // Ezután ellenőrizzük, hogy sort kell-e váltani
    if (c == '\n' || rttyCurrentLineBuffer.length() >= RTTY_LINE_BUFFER_SIZE - 1) {
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
        return;  // Ha a FIFO tele van, akkor nem küldjük el a parancsot
    }

    // Ha kikapcsoljuk a módot, akkor nem töröljük a területet
    if (core1_cmd_set_mode != CORE1_CMD_SET_MODE_OFF) {
        clearRttyTextBufferAndDisplay();
    }
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
