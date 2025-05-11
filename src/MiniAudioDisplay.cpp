#include "MiniAudioDisplay.h"

#include "rtVars.h"  // ::newDisplay

// Színprofilok
namespace FftDisplayConstants {
extern const uint16_t colors0[16];  // Cold
extern const uint16_t colors1[16];  // Hot
};  // namespace FftDisplayConstants

/**
 *
 */
MiniAudioDisplay::MiniAudioDisplay(TFT_eSPI& tft_ref, SI4735& si4735_ref, Band& band_ref)
    : DisplayBase(tft_ref, si4735_ref, band_ref),
      currentMiniWindowMode(1),  // Kezdő mód: alacsony felbontású spektrum
      audioMutedState(false),    // Kezdetben nincs némítva
      FFT(),                     // FFT objektum inicializálása
      highResOffset(0) {
    DEBUG("MiniAudioDisplay::MiniAudioDisplay\n");
    memset(Rpeak, 0, sizeof(Rpeak));
    memset(osciSamples, 0, sizeof(osciSamples));  // Oszcilloszkóp buffer inicializálása
    memset(wabuf, 0, sizeof(wabuf));
    // A gombokat a drawScreen-ben hozzuk létre
}

MiniAudioDisplay::~MiniAudioDisplay() { DEBUG("MiniAudioDisplay::~MiniAudioDisplay\n"); }

/**
 *
 */
void MiniAudioDisplay::drawScreen() {
    tft.fillScreen(TFT_COLOR_BACKGROUND);
    dawStatusLine();  // Státuszsor

    // Gombok: "Mode" és "Exit"
    DisplayBase::BuildButtonData screenButtonsData[] = {
        {"Mode", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
        {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
    };
    // Minden gomb létrehozása egyszerre
    buildHorizontalScreenButtons(screenButtonsData, ARRAY_ITEM_COUNT(screenButtonsData), false);

    // "Exit" gomb megkeresése és jobbra igazítása
    TftButton* exitButton = findButtonByLabel("Exit");
    if (exitButton != nullptr) {
        uint16_t exitButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;                                                       // Jobbra igazítás
        uint16_t exitButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, ARRAY_ITEM_COUNT(screenButtonsData) - 1, false);  // Y pozíció az utolsó gombhoz
        exitButton->setPosition(exitButtonX, exitButtonY);
    }

    drawScreenButtons();  // Minden horizontális gomb kirajzolása

    // Mini kijelző területének körvonala
    tft.drawRect(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X - 1, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y - 1, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W + 2,
                 MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);
    clearMiniDisplayArea();
    drawModeIndicator();
}

void MiniAudioDisplay::clearMiniDisplayArea() {
    tft.fillRect(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W,
                 MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H, TFT_NAVY);  // Sötétkék háttér, mint az FFT.ino-ban
}

void MiniAudioDisplay::drawModeIndicator() {
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    uint16_t indicatorX = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X + MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W / 2;
    uint16_t indicatorY = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y - tft.fontHeight() - 2;
    if (indicatorY < DisplayConstants::StatusLineHeight + 2) indicatorY = DisplayConstants::StatusLineHeight + 2;  // Ne lógjon a státuszsorra

    String modeText = "Mode: ";
    switch (currentMiniWindowMode) {
        case 0:
            modeText += "Off";
            break;
        case 1:
            modeText += "Spectrum (LowRes)";
            break;
        case 2:
            modeText += "Spectrum (HighRes)";
            break;
        case 3:
            modeText += "Oscilloscope";
            break;
        case 4:
            modeText += "Waterfall";
            break;
        case 5:
            modeText += "Envelope";
            break;
        default:
            modeText += "Unknown";
            break;
    }
    // Előző felirat törlése
    tft.fillRect(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X, indicatorY, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W, tft.fontHeight(), TFT_COLOR_BACKGROUND);
    tft.drawString(modeText, indicatorX, indicatorY);
}

void MiniAudioDisplay::cycleMiniWindowMode() {
    currentMiniWindowMode++;
    if (currentMiniWindowMode >= 6) {  // 0-5 módok
        currentMiniWindowMode = 0;
    }
    clearMiniDisplayArea();  // Töröljük a területet módváltáskor
    drawModeIndicator();     // Új mód kiírása
    // Reset Rpeak for mode 1 if switching to it or away from it
    if (currentMiniWindowMode == 1 || (currentMiniWindowMode != 1 && Rpeak[0] != 0)) {
        memset(Rpeak, 0, sizeof(Rpeak));
    }
    // Oszcilloszkóp buffer törlése/resetelése módváltáskor
    if (currentMiniWindowMode == 3 || (currentMiniWindowMode != 3 && osciSamples[0] != 2048)) {  // Középpontra állítás
        for (int i = 0; i < MiniAudioDisplayConstants::MINI_OSCI_SAMPLES_TO_DRAW; ++i) osciSamples[i] = 2048;
    }
    // Reset wabuf if switching to mode 4/5 or away
    if (currentMiniWindowMode == 4 || currentMiniWindowMode == 5 || (currentMiniWindowMode < 4 && wabuf[0][0] != 0)) {
        memset(wabuf, 0, sizeof(wabuf));
    }
    highResOffset = 0;  // Reset high-res offset
}

void MiniAudioDisplay::FFTSampleMini(bool collectOsciSamples) {
    using namespace MiniAudioDisplayConstants;
    // Az FFT.ino FFTSample logikája alapján
    for (int i = 0; i < FFT_SAMPLES; i++) {
        uint32_t sum = 0;
        // Túlmintavételezés, mint az FFT.ino-ban (dly=0 eset)
        for (int j = 0; j < 4; j++) {
            sum += analogRead(AUDIO_INPUT_PIN);
        }

        double averaged_sample = sum / 4.0;
        if (collectOsciSamples && i < MINI_OSCI_SAMPLES_TO_DRAW) {
            // Nyers (átlagolt) minták mentése az oszcilloszkóphoz
            osciSamples[i] = static_cast<int>(averaged_sample);
        }
        vReal[i] = averaged_sample - 2048.0;  // DC eltolás az FFT-hez
        vImag[i] = 0.0;
    }

    FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);  // Az eredmény a vReal-be kerül

    // Átmásoljuk az RvReal-be, ha az FFT.ino struktúráját követjük
    for (int i = 0; i < FFT_SAMPLES; ++i) {
        RvReal[i] = vReal[i];
    }
}

// --- Kirajzoló függvények implementációi ---
void MiniAudioDisplay::drawMiniSpectrumLowRes() {
    using namespace MiniAudioDisplayConstants;

    constexpr int total_width_low_res = (LOW_RES_BANDS * 3) + ((LOW_RES_BANDS - 1) * 2);
    int x_pixel_offset_low_res = 0;
    if (MINI_DISPLAY_AREA_W > total_width_low_res) {
        x_pixel_offset_low_res = (MINI_DISPLAY_AREA_W - total_width_low_res) / 2;
    }
    int actual_start_x_low_res = MINI_DISPLAY_AREA_X + x_pixel_offset_low_res;

    tft.drawRect(actual_start_x_low_res - 1, MINI_DISPLAY_AREA_Y - 1, total_width_low_res + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    for (byte band_idx = 0; band_idx <= LOW_RES_BANDS; band_idx++) {
        if (Rpeak[band_idx] > 0) {
            int xPos = actual_start_x_low_res + 5 * band_idx;
            int yPos = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - Rpeak[band_idx];
            tft.fillRect(xPos, yPos, 3, 2, TFT_NAVY);
        }
        if (Rpeak[band_idx] >= 1) {
            Rpeak[band_idx] -= 1;
        }
    }

    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {
        if (RvReal[i] > (MINI_AMPLITUDE_SCALE / 10.0)) {
            byte band_idx = getBandValMini(i);
            displayBandMini(band_idx, (int)RvReal[i], actual_start_x_low_res);
        }
    }
}

uint8_t MiniAudioDisplay::getBandValMini(int fft_bin_index) {
    if (fft_bin_index < 2) return 0;
    return constrain((fft_bin_index - 2) / 8, 0, MiniAudioDisplayConstants::LOW_RES_BANDS - 1);
}

void MiniAudioDisplay::displayBandMini(int band_idx, int magnitude, int actual_start_x) {
    using namespace MiniAudioDisplayConstants;

    int dsize = magnitude / MINI_AMPLITUDE_SCALE;
    dsize = constrain(dsize, 0, MINI_DISPLAY_AREA_H - 1);

    int xPos = actual_start_x + 5 * band_idx;
    if (xPos + 2 >= actual_start_x + (LOW_RES_BANDS * 5)) return;

    if (dsize > 0) {
        tft.fillRect(xPos, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - dsize, 3, dsize, TFT_YELLOW);
    }

    if (dsize > Rpeak[band_idx]) {
        Rpeak[band_idx] = dsize;
    }
}

void MiniAudioDisplay::drawMiniSpectrumHighRes() {
    using namespace MiniAudioDisplayConstants;

    int x_pixel_offset = 0;
    if (MINI_DISPLAY_AREA_W > HIGH_RES_BINS_TO_DISPLAY) {
        x_pixel_offset = (MINI_DISPLAY_AREA_W - HIGH_RES_BINS_TO_DISPLAY) / 2;
    }
    int actual_start_x_high_res = MINI_DISPLAY_AREA_X + x_pixel_offset;

    tft.drawRect(actual_start_x_high_res - 1, MINI_DISPLAY_AREA_Y - 1, HIGH_RES_BINS_TO_DISPLAY + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    for (int i = 2; i < HIGH_RES_BINS_TO_DISPLAY + 2; i++) {
        if (i >= FFT_SAMPLES / 2) break;

        int screen_x = actual_start_x_high_res + (i - 2);
        if (screen_x >= actual_start_x_high_res + HIGH_RES_BINS_TO_DISPLAY) continue;

        tft.drawFastVLine(screen_x, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_H, TFT_NAVY);

        int scaled_magnitude = (int)(RvReal[i] / MINI_AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, MINI_DISPLAY_AREA_H - 1);

        int y_bar_start = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1 - scaled_magnitude + (highResOffset % (MINI_DISPLAY_AREA_H / 10));
        y_bar_start = constrain(y_bar_start, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1);
        int bar_actual_height = (MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1) - y_bar_start + 1;
        if (bar_actual_height > 0 && scaled_magnitude > 0) {
            tft.drawFastVLine(screen_x, y_bar_start, bar_actual_height, TFT_SKYBLUE);
        }
    }
    highResOffset++;
}

void MiniAudioDisplay::drawMiniOscilloscope() {
    using namespace MiniAudioDisplayConstants;

    clearMiniDisplayArea();  // Terület törlése minden rajzolás előtt

    int x_pixel_offset_osci = 0;
    if (MINI_DISPLAY_AREA_W > MINI_OSCI_SAMPLES_TO_DRAW) {
        x_pixel_offset_osci = (MINI_DISPLAY_AREA_W - MINI_OSCI_SAMPLES_TO_DRAW) / 2;
    }
    int actual_start_x_osci = MINI_DISPLAY_AREA_X + x_pixel_offset_osci;

    tft.drawRect(actual_start_x_osci - 1, MINI_DISPLAY_AREA_Y - 1, MINI_OSCI_SAMPLES_TO_DRAW + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    int prev_x = -1, prev_y = -1;

    for (int i = 0; i < MINI_OSCI_SAMPLES_TO_DRAW; i++) {
        int raw_sample = osciSamples[i];
        double centered_sample = static_cast<double>(raw_sample) - 2048.0;
        double scaled_to_half_height = centered_sample * (static_cast<double>(MINI_DISPLAY_AREA_H) / 2.0 - 2.0) / 2048.0;

        int y_pos = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H / 2 - static_cast<int>(round(scaled_to_half_height));
        y_pos = constrain(y_pos, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1);

        int x_pos = actual_start_x_osci + i;

        if (prev_x != -1) {
            tft.drawLine(prev_x, prev_y, x_pos, y_pos, TFT_GREEN);
        } else {
            tft.drawPixel(x_pos, y_pos, TFT_GREEN);
        }
        prev_x = x_pos;
        prev_y = y_pos;
    }
}

void MiniAudioDisplay::drawMiniWaterfall() { /* TODO */ }

void MiniAudioDisplay::drawMiniEnvelope() { /* TODO */ }

uint16_t MiniAudioDisplay::valueToMiniWaterfallColor(int scaled_value) {
    float normalized = (float)scaled_value / (float)(MiniAudioDisplayConstants::MINI_WF_GRADIENT * MiniAudioDisplayConstants::MINI_WF_HEIGHT);
    normalized = constrain(normalized, 0.0f, 1.0f);

    const uint16_t* colors = FftDisplayConstants::colors0;
    byte color_size = 16;

    int index = (int)(normalized * (color_size - 1));
    index = constrain(index, 0, color_size - 1);
    return colors[index];
}

void MiniAudioDisplay::displayLoop() {
    if (pDialog != nullptr) return;

    if (rtv::muteStat) {
        if (!audioMutedState) {
            audioMutedState = true;
            clearMiniDisplayArea();
            tft.setCursor(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X + MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W / 2,
                          MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y + MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H / 2);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_YELLOW);
            tft.print("MUTED");
        }
        return;
    } else {
        if (audioMutedState) {
            audioMutedState = false;
            clearMiniDisplayArea();
        }
    }

    if (currentMiniWindowMode == 0) {
        return;
    }

    if (currentMiniWindowMode == 3) {
        FFTSampleMini(true);
        drawMiniOscilloscope();
    } else {
        FFTSampleMini(false);
        switch (currentMiniWindowMode) {
            case 1:
                drawMiniSpectrumLowRes();
                break;
            case 2:
                drawMiniSpectrumHighRes();
                break;
            case 4:
                drawMiniWaterfall();
                break;
            case 5:
                drawMiniEnvelope();
                break;
        }
    }
}

bool MiniAudioDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    if (pDialog) return false;
    if (encoderState.buttonState == RotaryEncoder::ButtonState::Clicked) {
        cycleMiniWindowMode();
        return true;
    }
    return false;
}

bool MiniAudioDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; }

void MiniAudioDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) {
    if (STREQ("Exit", event.label)) {
        ::newDisplay = prevDisplay;
        if (prevDisplay == DisplayType::none) {
            ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayType::fm : DisplayType::am;
        }
    } else if (STREQ("Mode", event.label)) {
        cycleMiniWindowMode();
    }
}
