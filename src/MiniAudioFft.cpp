#include "MiniAudioFft.h"

#include "rtVars.h"  // For rtv::muteStat

MiniAudioFft::MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h)
    : tft(tft_ref),
      posX(x),
      posY(y),
      width(w),
      height(h),
      currentMode(1),  // Start with LowRes spectrum
                       //   audioMutedState(false),
      FFT(),
      highResOffset(0) {

    // Initialize wabuf with the actual component dimensions
    // Ensure height and width are positive before resizing
    if (this->height > 0 && this->width > 0) {
        wabuf.resize(this->height, std::vector<int>(this->width, 0));
    } else {
        // Handle error or set to a default small size if dimensions are invalid
        // For now, let's assume valid dimensions are passed.
        // If not, wabuf will be empty, and modes using it might fail.
        DEBUG("MiniAudioFft: Invalid dimensions w=%d, h=%d\n", this->width, this->height);
    }

    memset(Rpeak, 0, sizeof(Rpeak));
    // Initialize osciSamples to a baseline (e.g., ADC midpoint if raw values, or 0 if centered)
    for (int i = 0; i < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++i) {
        osciSamples[i] = 2048;  // ADC midpoint for raw samples
    }
}

void MiniAudioFft::cycleMode() {
    currentMode++;
    if (currentMode >= 6) {  // 0-5 modes (0=Off, 1=LowRes, 2=HighRes, 3=Scope, 4=Waterfall, 5=Envelope)
        currentMode = 0;
    }
    clearArea();
    drawModeIndicator();
    // Reset mode-specific buffers if necessary
    if (currentMode == 1 || (currentMode != 1 && Rpeak[0] != 0)) {
        memset(Rpeak, 0, sizeof(Rpeak));
    }
    if (currentMode == 3 || (currentMode != 3 && osciSamples[0] != 2048)) {
        for (int i = 0; i < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++i) osciSamples[i] = 2048;
    }
    if (currentMode == 4 || currentMode == 5 || (currentMode < 4 && !wabuf.empty() && !wabuf[0].empty() && wabuf[0][0] != 0)) {
        for (auto& row : wabuf) std::fill(row.begin(), row.end(), 0);
    }
    highResOffset = 0;
}

void MiniAudioFft::clearArea() { tft.fillRect(posX, posY, width, height, TFT_BLACK); }

void MiniAudioFft::drawModeIndicator() {
    if (width < 20 || height < 10) return;  // Not enough space

    tft.setFreeFont();  // Or a specific small font
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);  // Bottom-Center

    String modeText = "";
    switch (currentMode) {
        case 0:
            modeText = "Off";
            break;
        case 1:
            modeText = "FFT L";
            break;
        case 2:
            modeText = "FFT H";
            break;
        case 3:
            modeText = "Scope";
            break;
        case 4:
            modeText = "WFall";
            break;
        case 5:
            modeText = "Env";
            break;
        default:
            modeText = "Unk";
            break;
    }
    // Clear previous text (simple full width clear)
    // tft.fillRect(posX, posY + height - tft.fontHeight() - 1, width, tft.fontHeight() + 1, TFT_BLACK);
    // Draw new text at the bottom center of the component
    tft.drawString(modeText, posX + width / 2, posY + height - 1);
}

void MiniAudioFft::performFFT(bool collectOsciSamples) {
    using namespace MiniAudioFftConstants;
    int osci_sample_idx = 0;

    for (int i = 0; i < FFT_SAMPLES; i++) {
        uint32_t sum = 0;
        for (int j = 0; j < 4; j++) {  // Oversampling
            sum += analogRead(AUDIO_INPUT_PIN);
        }
        double averaged_sample = sum / 4.0;

        if (collectOsciSamples) {
            if (i % OSCI_SAMPLE_DECIMATION_FACTOR == 0 && osci_sample_idx < MAX_INTERNAL_WIDTH) {
                if (osci_sample_idx < sizeof(osciSamples) / sizeof(osciSamples[0])) {  // Bounds check
                    osciSamples[osci_sample_idx] = static_cast<int>(averaged_sample);
                    osci_sample_idx++;
                }
            }
        }
        vReal[i] = averaged_sample - 2048.0;  // DC offset
        vImag[i] = 0.0;
    }

    FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);

    for (int i = 0; i < FFT_SAMPLES; ++i) {
        RvReal[i] = vReal[i];
    }
}

void MiniAudioFft::loop() {
    if (rtv::muteStat) {
        // if (!audioMutedState) {
        //     audioMutedState = true;
        clearArea();
        if (width > 10 && height > 10) {  // Basic check for space
            tft.setCursor(posX + width / 2, posY + height / 2);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_YELLOW);
            tft.setTextSize(1);
            tft.print("MUTED");
        }
        // }
        return;
    } else {
        // if (audioMutedState) {
        //     audioMutedState = false;
        // clearArea(); // Clear "MUTED" text by redrawing the mode
        // }
    }

    if (currentMode == 0) {  // Off
                             // Ensure area is clear and mode indicator shows "Off"
                             // This might be redundant if cycleMode already cleared.
                             // static bool offDrawn = false;
                             // if (!offDrawn) {
        clearArea();
        drawModeIndicator();
        //     offDrawn = true;
        // }
        return;
    }
    // offDrawn = false;

    performFFT(currentMode == 3);  // Collect osci samples only for scope mode

    switch (currentMode) {
        case 1:
            drawSpectrumLowRes();
            break;
        case 2:
            drawSpectrumHighRes();
            break;
        case 3:
            drawOscilloscope();
            break;
        case 4:
            drawWaterfall();
            break;
        case 5:
            drawEnvelope();
            break;
    }
    drawModeIndicator();  // Redraw indicator as content might overwrite it
}

bool MiniAudioFft::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    if (touched && (tx >= posX && tx < (posX + width) && ty >= posY && ty < (posY + height))) {
        cycleMode();
        // Force immediate redraw after mode change
        // audioMutedState = true; // Force redraw even if not muted
        loop();
        return true;
    }
    return false;
}

void MiniAudioFft::forceRedraw() {
    clearArea();
    // audioMutedState = true; // Force redraw logic in loop
    loop();
}

// --- Drawing methods (adapted from MiniAudioDisplay.cpp) ---

void MiniAudioFft::drawSpectrumLowRes() {
    using namespace MiniAudioFftConstants;
    if (width == 0 || height == 0) return;

    int actual_low_res_peak_max_height = height - 1;  // Peak height relative to component height

    constexpr int band_width_pixels = 3;
    constexpr int band_gap_pixels = 2;
    constexpr int band_total_pixels = band_width_pixels + band_gap_pixels;

    int num_drawable_bands = width / band_total_pixels;
    int bands_to_process = std::min(LOW_RES_BANDS, num_drawable_bands);
    if (bands_to_process == 0 && LOW_RES_BANDS > 0) bands_to_process = 1;  // Draw at least one if possible

    int total_drawn_width = (bands_to_process * band_width_pixels) + (std::max(0, bands_to_process - 1) * band_gap_pixels);
    int x_offset = (width - total_drawn_width) / 2;
    int current_draw_x = posX + x_offset;

    // Clear previous peaks
    for (int band_idx = 0; band_idx < bands_to_process; band_idx++) {
        if (Rpeak[band_idx] > 0) {
            int xPos = current_draw_x + band_total_pixels * band_idx;
            int yPos = posY + height - Rpeak[band_idx];
            // Ensure erase is within bounds
            int erase_h = std::min(2, posY + height - yPos);
            if (yPos < posY + height && erase_h > 0) {
                tft.fillRect(xPos, yPos, band_width_pixels, erase_h, TFT_BLACK);
            }
        }
        if (Rpeak[band_idx] >= 1) Rpeak[band_idx] -= 1;
    }

    // Draw new spectrum
    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {
        if (RvReal[i] > (AMPLITUDE_SCALE / 10.0)) {
            byte band_idx = getBandVal(i);      // This maps FFT bin to 0-15 range
            if (band_idx < bands_to_process) {  // Only process bands that can be drawn
                displayBand(band_idx, (int)RvReal[i], current_draw_x, actual_low_res_peak_max_height);
            }
        }
    }
}

uint8_t MiniAudioFft::getBandVal(int fft_bin_index) {
    // Maps FFT bin index to one of LOW_RES_BANDS (0-15)
    // This logic might need adjustment based on FFT_SAMPLES and desired frequency distribution
    if (fft_bin_index < 2) return 0;
    // Simple linear mapping for now, assuming FFT_SAMPLES/2 bins are spread over LOW_RES_BANDS
    int effective_bins = MiniAudioFftConstants::FFT_SAMPLES / 2 - 2;
    if (effective_bins <= 0) return 0;
    return constrain((fft_bin_index - 2) * MiniAudioFftConstants::LOW_RES_BANDS / effective_bins, 0, MiniAudioFftConstants::LOW_RES_BANDS - 1);
}

void MiniAudioFft::displayBand(int band_idx, int magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode) {
    using namespace MiniAudioFftConstants;
    if (height == 0) return;

    int dsize = magnitude / AMPLITUDE_SCALE;
    dsize = constrain(dsize, 0, peak_max_height_for_mode);

    constexpr int band_width_pixels = 3;
    constexpr int band_gap_pixels = 2;
    constexpr int band_total_pixels = band_width_pixels + band_gap_pixels;

    int xPos = actual_start_x_on_screen + band_total_pixels * band_idx;

    // Check if the band is within the drawable area of the component
    if (xPos + band_width_pixels > posX + width || xPos < posX) return;

    if (dsize > 0) {
        // Ensure drawing is within component's Y bounds
        int y_start_bar = posY + height - dsize;
        int bar_h = dsize;
        if (y_start_bar < posY) {
            bar_h -= (posY - y_start_bar);
            y_start_bar = posY;
        }
        if (bar_h > 0) {
            tft.fillRect(xPos, y_start_bar, band_width_pixels, bar_h, TFT_YELLOW);
        }
    }

    if (dsize > Rpeak[band_idx]) {
        Rpeak[band_idx] = dsize;
    }
}

void MiniAudioFft::drawSpectrumHighRes() {
    using namespace MiniAudioFftConstants;
    if (width == 0 || height == 0) return;

    int actual_high_res_bins_to_display = width;  // Draw one VLine per pixel of component width

    for (int i = 0; i < actual_high_res_bins_to_display; i++) {
        // Map screen pixel 'i' to an FFT bin.
        // Start from FFT bin 2. Max FFT bin is FFT_SAMPLES/2 - 1.
        int fft_bin_index = 2 + (i * (FFT_SAMPLES / 2 - 2)) / actual_high_res_bins_to_display;
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        int screen_x = posX + i;

        tft.drawFastVLine(screen_x, posY, height, TFT_BLACK);  // Clear column

        int scaled_magnitude = (int)(RvReal[fft_bin_index] / AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, height - 1);

        if (scaled_magnitude > 0) {
            // Original FFT.ino had an 'offset' for a moving effect, simplified here
            // int y_bar_start = posY + height - 1 - scaled_magnitude + (highResOffset % (height / 10));
            int y_bar_start = posY + height - 1 - scaled_magnitude;
            y_bar_start = constrain(y_bar_start, posY, posY + height - 1);
            int bar_actual_height = (posY + height - 1) - y_bar_start + 1;

            if (bar_actual_height > 0) {
                tft.drawFastVLine(screen_x, y_bar_start, bar_actual_height, TFT_SKYBLUE);
            }
        }
    }
    // highResOffset++; // If moving effect is desired
}

void MiniAudioFft::drawOscilloscope() {
    using namespace MiniAudioFftConstants;
    if (width == 0 || height == 0) return;

    int actual_osci_samples_to_draw = width;  // Draw one point/line segment per pixel of component width

    tft.fillRect(posX, posY, width, height, TFT_BLACK);  // Clear area

    int prev_x = -1, prev_y = -1;

    for (int i = 0; i < actual_osci_samples_to_draw; i++) {
        // Map screen pixel 'i' to an osciSample index
        // This assumes osciSamples array is filled with enough data
        // and MAX_INTERNAL_WIDTH is >= actual_osci_samples_to_draw
        int sample_idx = (i * (sizeof(osciSamples) / sizeof(osciSamples[0]) - 1)) / (actual_osci_samples_to_draw - 1);
        if (sample_idx >= sizeof(osciSamples) / sizeof(osciSamples[0])) sample_idx = sizeof(osciSamples) / sizeof(osciSamples[0]) - 1;

        int raw_sample = osciSamples[sample_idx];
        double centered_sample = static_cast<double>(raw_sample) - 2048.0;
        centered_sample *= OSCI_SENSITIVITY_FACTOR;

        // Scale to component's half height, with a small margin
        double scaled_to_half_height = centered_sample * (static_cast<double>(height) / 2.0 - 1.0) / 2048.0;

        int y_pos = posY + height / 2 - static_cast<int>(round(scaled_to_half_height));
        y_pos = constrain(y_pos, posY, posY + height - 1);

        int x_pos = posX + i;

        if (prev_x != -1) {
            tft.drawLine(prev_x, prev_y, x_pos, y_pos, TFT_GREEN);
        } else {
            tft.drawPixel(x_pos, y_pos, TFT_GREEN);
        }
        prev_x = x_pos;
        prev_y = y_pos;
    }
}

void MiniAudioFft::drawWaterfall() {
    using namespace MiniAudioFftConstants;
    if (width == 0 || height == 0 || wabuf.empty() || wabuf[0].empty()) return;

    // 1. Shift data left
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Load new data into the rightmost column
    for (int r = 0; r < height; ++r) {
        // Map row 'r' to an FFT bin.
        // Start from FFT bin 2. Max FFT bin is FFT_SAMPLES/2 - 1.
        // The number of FFT bins to display is 'height'.
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        double scaled_fft_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;
        wabuf[r][width - 1] = static_cast<int>(constrain(scaled_fft_val * 50.0, 0.0, 255.0));
    }

    // 3. Draw pixels
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width; ++c) {
            uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r][c]);
            // Y is inverted for waterfall: lower frequencies (lower r) at the bottom
            tft.drawPixel(posX + c, posY + (height - 1 - r), color);
        }
    }
}

uint16_t MiniAudioFft::valueToWaterfallColor(int scaled_value) {
    using namespace MiniAudioFftConstants;
    float normalized = static_cast<float>(constrain(scaled_value, 0, MAX_WATERFALL_COLOR_INPUT_VALUE)) / static_cast<float>(MAX_WATERFALL_COLOR_INPUT_VALUE);

    byte color_size = sizeof(WATERFALL_COLORS) / sizeof(WATERFALL_COLORS[0]);
    int index = (int)(normalized * (color_size - 1));
    index = constrain(index, 0, color_size - 1);
    return WATERFALL_COLORS[index];
}

void MiniAudioFft::drawEnvelope() {
    using namespace MiniAudioFftConstants;
    if (width == 0 || height == 0 || wabuf.empty() || wabuf[0].empty()) return;

    // 1. Shift data left (same as waterfall)
    for (int r = 0; r < height; ++r) {
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Load new data (same as waterfall, but uses ENVELOPE_INPUT_GAIN)
    for (int r = 0; r < height; ++r) {
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        double scaled_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;
        double gained_val = scaled_val * ENVELOPE_INPUT_GAIN;
        wabuf[r][width - 1] = static_cast<int>(constrain(gained_val, 0.0, 255.0));
    }

    // 3. Draw envelope
    const int half_h_component = height / 2;
    // static int prev_smoothed_env_idx_map[MiniAudioFftConstants::MAX_INTERNAL_WIDTH] = {0};  // Unused variable
    int prev_smoothed_env_idx_local = half_h_component;                                     // Simpler: reset per frame for this component

    for (int c = 0; c < width; ++c) {  // Iterate through columns (component width)
        int env_idx_for_col = half_h_component;
        int max_val_in_col = 0;
        bool column_has_signal = false;

        for (int r = 0; r < height; ++r) {  // Iterate through rows (component height, maps to FFT bins)
            if (wabuf[r][c] > 0) column_has_signal = true;
            if (wabuf[r][c] > max_val_in_col) {
                max_val_in_col = wabuf[r][c];
                env_idx_for_col = r;  // 'r' is the bin index relative to component height
            }
        }

        int smoothed_env_idx = static_cast<int>(MiniAudioFftConstants::ENVELOPE_SMOOTH_FACTOR * prev_smoothed_env_idx_local + (1.0f - MiniAudioFftConstants::ENVELOPE_SMOOTH_FACTOR) * env_idx_for_col);
        prev_smoothed_env_idx_local = smoothed_env_idx;

        int col_x_on_screen = posX + c;
        tft.drawFastVLine(col_x_on_screen, posY, height, TFT_BLACK);  // Clear column

        if (column_has_signal) {
            // smoothed_env_idx is now 0 to (height-1)
            // The thickness loop should iterate based on this index, scaled by ENVELOPE_THICKNESS_SCALER
            // The original 'i' in MiniAudioDisplay went up to smoothed_env_idx, where smoothed_env_idx was an FFT bin index.
            // Here, smoothed_env_idx is already scaled to component height.
            // The visual thickness is determined by how many pixels we draw around the center line.
            // Let's make the thickness proportional to smoothed_env_idx itself, but scaled.

            // 'smoothed_env_idx' here represents the dominant bin's position (0 to height-1)
            // The visual "thickness" or amplitude of the envelope should be derived from this.
            // Let's use 'smoothed_env_idx' to determine the vertical spread from the center.
            // The original code's 'i' loop went up to 'smoothed_env_idx'.
            // Here, 'smoothed_env_idx' is the y-position of the peak.
            // We want the envelope to spread from center based on this peak's y-position.
            // A simpler interpretation: the thickness is proportional to the dominant bin's mapped value (0 to height-1)

            for (int i = 0; i <= smoothed_env_idx; ++i) {  // 'i' iterates up to the dominant bin's mapped row
                int y_offset = static_cast<int>(static_cast<float>(i) * ENVELOPE_THICKNESS_SCALER);
                y_offset = std::min(y_offset, half_h_component - 1);  // Prevent overdraw

                int yUpper = posY + half_h_component - y_offset;
                int yLower = posY + half_h_component + y_offset;

                yUpper = constrain(yUpper, posY, posY + height - 1);
                yLower = constrain(yLower, posY, posY + height - 1);

                if (yUpper <= yLower) {
                    tft.drawPixel(col_x_on_screen, yUpper, TFT_WHITE);
                    if (yUpper != yLower) {
                        tft.drawPixel(col_x_on_screen, yLower, TFT_WHITE);
                    }
                }
            }
        }
    }
}
