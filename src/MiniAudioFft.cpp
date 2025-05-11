#include "MiniAudioFft.h"

#include "rtVars.h"  // rtv::muteStat eléréséhez

/**
 * @brief A MiniAudioFft komponens konstruktora.
 *
 * Inicializálja a komponenst a megadott pozícióval, méretekkel és a TFT objektummal.
 * Beállítja a kezdeti megjelenítési módot és inicializálja a belső puffereket.
 *
 * @param tft_ref Referencia a TFT_eSPI objektumra, amire a komponens rajzolni fog.
 * @param x A komponens bal felső sarkának X koordinátája a képernyőn.
 * @param y A komponens bal felső sarkának Y koordinátája a képernyőn.
 * @param w A komponens szélessége pixelekben.
 * @param h A komponens magassága pixelekben.
 */
MiniAudioFft::MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h)
    : tft(tft_ref),
      posX(x),
      posY(y),
      width(w),
      height(h),
      currentMode(1),                // Kezdő mód: Alacsony felbontású spektrum
      prevMuteState(rtv::muteStat),  // Némítás előző állapotának inicializálása
      FFT(),                         // FFT objektum inicializálása
      highResOffset(0) {
    // A `wabuf` (vízesés és burkológörbe buffer) inicializálása a komponens tényleges méreteivel.
    // Biztosítjuk, hogy a magasság és szélesség pozitív legyen az átméretezés előtt.
    if (this->height > 0 && this->width > 0) {
        wabuf.resize(this->height, std::vector<int>(this->width, 0));
    } else {
        DEBUG("MiniAudioFft: Invalid dimensions w=%d, h=%d\n", this->width, this->height);
    }

    // Pufferek nullázása
    memset(Rpeak, 0, sizeof(Rpeak));
    // Oszcilloszkóp minták inicializálása középpontra (ADC nyers érték)
    for (int i = 0; i < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++i) {
        osciSamples[i] = 2048;
    }
}

/**
 * @brief Vált a következő megjelenítési módra.
 *
 * Körbejár a rendelkezésre álló módok között (0-tól 5-ig).
 * Módváltáskor törli a komponens területét, kirajzolja az új mód nevét,
 * és reseteli a mód-specifikus puffereket.
 */
void MiniAudioFft::cycleMode() {
    currentMode++;
    if (currentMode >= 6) {  // 0-5 módok (0=Ki, 1=AlacsonyFelb, 2=MagasFelb, 3=Szkóp, 4=Vízesés, 5=Burkoló)
        currentMode = 0;
    }
    clearArea();  // Teljes terület törlése az új mód előtt
    // Mód-specifikus pufferek resetelése
    if (currentMode == 1 || (currentMode != 1 && Rpeak[0] != 0)) {  // Alacsony felb. spektrum
        memset(Rpeak, 0, sizeof(Rpeak));
    }
    if (currentMode == 3 || (currentMode != 3 && osciSamples[0] != 2048)) {  // Oszcilloszkóp
        for (int i = 0; i < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++i) osciSamples[i] = 2048;
    }
    if (currentMode == 4 || currentMode == 5 || (currentMode < 4 && !wabuf.empty() && !wabuf[0].empty() && wabuf[0][0] != 0)) {  // Vízesés/Burkoló
        for (auto& row : wabuf) std::fill(row.begin(), row.end(), 0);
    }
    highResOffset = 0;  // Magas felbontású spektrum eltolásának resetelése

    drawModeIndicator();  // Módváltáskor azonnal kirajzoljuk az új mód nevét
}

/**
 * @brief Letörli a komponens teljes rajzolási területét feketére.
 */
void MiniAudioFft::clearArea() { tft.fillRect(posX, posY, width, height, TFT_BLACK); }

/**
 * @brief Kiszámítja és visszaadja a módkijelző területének magasságát.
 * Ez a `drawModeIndicator`-ban használt font magasságán és egy kis margón alapul.
 * @return int A módkijelző területének magassága pixelekben.
 */
int MiniAudioFft::getIndicatorAreaHeight() const {
    TFT_eSPI& temp_tft = const_cast<TFT_eSPI&>(tft);
    temp_tft.setFreeFont();
    temp_tft.setTextSize(1);
    return temp_tft.fontHeight() + 2;  // +2 pixel margó
}

/**
 * @brief Kiszámítja és visszaadja a grafikonok számára ténylegesen rendelkezésre álló magasságot.
 * Ez a komponens teljes magassága mínusz a módkijelzőnek fenntartott terület.
 * @return int A grafikonok rajzolási magassága pixelekben.
 */
int MiniAudioFft::getGraphHeight() const { return height - getIndicatorAreaHeight(); }

/**
 * @brief Kiszámítja és visszaadja a módkijelző területének Y kezdőpozícióját a komponensen belül.
 * @return int A módkijelző Y kezdőpozíciója.
 */
int MiniAudioFft::getIndicatorAreaY() const { return posY + getGraphHeight(); }

/**
 * @brief Kirajzolja az aktuális mód nevét a komponens aljára, a számára fenntartott sávba.
 *
 * A szöveg hátterét feketére állítja, így felülírja az előző szöveget.
 */
void MiniAudioFft::drawModeIndicator() {
    int indicatorH = getIndicatorAreaHeight();
    int indicatorYstart = getIndicatorAreaY();  // A módkijelző sávjának teteje

    if (width < 20 || indicatorH < 8) return;

    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);  // Alul-középre igazítás

    String modeText = "";
    switch (currentMode) {
        case 0:
            modeText = "Off";
            break;
        case 1:
            modeText = "FFT LowRes";
            break;
        case 2:
            modeText = "FFT HighRes";
            break;
        case 3:
            modeText = "Scope";
            break;
        case 4:
            modeText = "WaterFall";
            break;
        case 5:
            modeText = "Envelope";
            break;
        default:
            modeText = "Unknown";
            break;
    }

    // Módkijelző területének explicit törlése
    tft.fillRect(posX, indicatorYstart, width, indicatorH, TFT_BLACK);
    // Szöveg kirajzolása a komponens aljára, középre.
    // Az Y koordináta a szöveg alapvonala lesz.
    tft.drawString(modeText, posX + width / 2, posY + height + 1);  // 2 pixel a BC_DATUM és a komponens alja közötti margó miatt
}

/**
 * @brief Elvégzi az FFT mintavételezést és a szükséges számításokat.
 *
 * Beolvassa az analóg audio bemenetet, alkalmazza az ablakozást,
 * elvégzi az FFT-t, majd a komplex eredményből magnitúdókat számol.
 * Opcionálisan mintákat gyűjt az oszcilloszkóp módhoz.
 *
 * @param collectOsciSamples Igaz, ha az oszcilloszkóp számára is kell mintákat gyűjteni.
 */
void MiniAudioFft::performFFT(bool collectOsciSamples) {
    using namespace MiniAudioFftConstants;
    int osci_sample_idx = 0;

    for (int i = 0; i < FFT_SAMPLES; i++) {
        uint32_t sum = 0;
        for (int j = 0; j < 4; j++) {
            sum += analogRead(AUDIO_INPUT_PIN);
        }
        double averaged_sample = sum / 4.0;

        if (collectOsciSamples) {
            if (i % OSCI_SAMPLE_DECIMATION_FACTOR == 0 && osci_sample_idx < MAX_INTERNAL_WIDTH) {
                if (osci_sample_idx < sizeof(osciSamples) / sizeof(osciSamples[0])) {
                    osciSamples[osci_sample_idx] = static_cast<int>(averaged_sample);
                    osci_sample_idx++;
                }
            }
        }
        vReal[i] = averaged_sample - 2048.0;
        vImag[i] = 0.0;
    }

    FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);

    for (int i = 0; i < FFT_SAMPLES; ++i) {
        RvReal[i] = vReal[i];
    }
}

/**
 * @brief A komponens fő ciklusfüggvénye.
 *
 * Kezeli a némítás állapotát, elvégzi az FFT-t (ha szükséges),
 * és kirajzolja az aktuális megjelenítési módot.
 */
void MiniAudioFft::loop() {
    bool muteStateJustChanged = (rtv::muteStat != prevMuteState);
    prevMuteState = rtv::muteStat;

    if (rtv::muteStat) {
        // Csak akkor rajzoljuk újra a "MUTED"-et, ha most lett némítva
        if (muteStateJustChanged) {
            clearArea();
            drawMuted();
        }
        return;
    }

    // Ha éppen most szűnt meg a némítás, teljes újrarajzolás szükséges
    if (muteStateJustChanged && !rtv::muteStat) {
        forceRedraw();  // Ez gondoskodik a grafikon és a módkijelző újrarajzolásáról
        return;
    }

    // Ha a mód "Ki" (0), akkor nincs dinamikus tartalom, amit frissíteni kellene.
    // A `cycleMode` vagy `forceRedraw` már kirajzolta az "Off" feliratot.
    if (currentMode == 0) {
        return;
    }

    // FFT mintavételezés és számítás (az oszcilloszkóp módhoz gyűjtünk extra mintákat)
    performFFT(currentMode == 3);

    // Grafikonok kirajzolása a nekik szánt (csökkentett) területre
    // Ezek a függvények csak a `posY`-tól `posY + getGraphHeight() - 1`-ig rajzolnak.
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
    // A drawModeIndicator() metódust NEM hívjuk itt minden ciklusban,
    // csak akkor, ha a mód ténylegesen megváltozik (cycleMode),
    // vagy ha a teljes komponenst újra kell rajzolni (forceRedraw, némítás feloldása).
}

/**
 * @brief Érintési események kezelése a komponensen.
 *
 * Ha a komponens területét érintik meg, vált a következő megjelenítési módra
 * és frissíti a kijelzőt.
 *
 * @param touched Igaz, ha éppen érintés történik.
 * @param tx Az érintés X koordinátája.
 * @param ty Az érintés Y koordinátája.
 * @return Igaz, ha az érintést a komponens kezelte, egyébként hamis.
 */
bool MiniAudioFft::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    if (touched && (tx >= posX && tx < (posX + width) && ty >= posY && ty < (posY + height))) {
        cycleMode();  // Ez már tartalmazza a clearArea()-t és a drawModeIndicator() hívást
        // A loop() meghívása itt opcionális. Ha a cycleMode után azonnal szeretnénk az új mód
        // első grafikonját is látni, akkor itt hívható. Különben a következő normál loop ciklus rajzolja.
        // Jelenleg a cycleMode csak a módkijelzőt frissíti, a grafikon a következő loop()-ban frissül.
        return true;
    }
    return false;
}

/**
 * @brief Kényszeríti a komponens teljes újrarajzolását az aktuális módban.
 *
 * Letörli a komponenst és meghívja a `loop` függvényt az újrarajzoláshoz.
 */
void MiniAudioFft::forceRedraw() {
    clearArea();
    prevMuteState = rtv::muteStat;  // Szinkronizáljuk a némítás állapotát

    if (rtv::muteStat) {
        drawMuted();

    } else if (currentMode == 0) {
        drawModeIndicator();  // "Off" kirajzolása

    } else {
        // Módok 1-5: grafikon kirajzolása
        performFFT(currentMode == 3);  // Szükséges az adatokhoz
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
        drawModeIndicator();  // És a módkijelző kirajzolása
    }
}

// --- Rajzoló metódusok (a MiniAudioDisplay.cpp alapján adaptálva) ---

/**
 * @brief Alacsony felbontású spektrum analizátor kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja a rajzoláshoz.
 */
void MiniAudioFft::drawSpectrumLowRes() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (width == 0 || graphH <= 0) return;

    int actual_low_res_peak_max_height = graphH - 1;

    constexpr int band_width_pixels = 3;
    constexpr int band_gap_pixels = 2;
    constexpr int band_total_pixels = band_width_pixels + band_gap_pixels;

    int num_drawable_bands = width / band_total_pixels;
    int bands_to_process = std::min(LOW_RES_BANDS, num_drawable_bands);
    if (bands_to_process == 0 && LOW_RES_BANDS > 0) bands_to_process = 1;

    int total_drawn_width = (bands_to_process * band_width_pixels) + (std::max(0, bands_to_process - 1) * band_gap_pixels);
    int x_offset = (width - total_drawn_width) / 2;
    int current_draw_x_on_screen = posX + x_offset;

    for (int band_idx = 0; band_idx < bands_to_process; band_idx++) {
        if (Rpeak[band_idx] > 0) {
            int xP = current_draw_x_on_screen + band_total_pixels * band_idx;
            int yP = posY + graphH - Rpeak[band_idx];
            int erase_h = std::min(2, posY + graphH - yP);
            if (yP < posY + graphH && erase_h > 0) {
                tft.fillRect(xP, yP, band_width_pixels, erase_h, TFT_BLACK);
            }
        }
        if (Rpeak[band_idx] >= 1) Rpeak[band_idx] -= 1;
    }

    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {
        if (RvReal[i] > (AMPLITUDE_SCALE / 10.0)) {
            byte band_idx = getBandVal(i);
            if (band_idx < bands_to_process) {
                displayBand(band_idx, (int)RvReal[i], current_draw_x_on_screen, actual_low_res_peak_max_height);
            }
        }
    }
}

/**
 * @brief Meghatározza, hogy egy adott FFT bin melyik alacsony felbontású sávhoz tartozik.
 * @param fft_bin_index Az FFT bin indexe.
 * @return A sáv indexe (0-tól LOW_RES_BANDS-1-ig).
 */
uint8_t MiniAudioFft::getBandVal(int fft_bin_index) {
    if (fft_bin_index < 2) return 0;
    int effective_bins = MiniAudioFftConstants::FFT_SAMPLES / 2 - 2;
    if (effective_bins <= 0) return 0;
    return constrain((fft_bin_index - 2) * MiniAudioFftConstants::LOW_RES_BANDS / effective_bins, 0, MiniAudioFftConstants::LOW_RES_BANDS - 1);
}

/**
 * @brief Kirajzol egyetlen sávot az alacsony felbontású spektrumhoz.
 * @param band_idx A sáv indexe.
 * @param magnitude A sáv magnitúdója.
 * @param actual_start_x_on_screen A spektrum rajzolásának kezdő X koordinátája a képernyőn.
 * @param peak_max_height_for_mode A sáv maximális magassága az adott módban.
 */
void MiniAudioFft::displayBand(int band_idx, int magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode) {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (graphH <= 0) return;

    int dsize = magnitude / AMPLITUDE_SCALE;
    dsize = constrain(dsize, 0, peak_max_height_for_mode);

    constexpr int band_width_pixels = 3;
    constexpr int band_gap_pixels = 2;
    constexpr int band_total_pixels = band_width_pixels + band_gap_pixels;
    int xPos = actual_start_x_on_screen + band_total_pixels * band_idx;

    if (xPos + band_width_pixels > posX + width || xPos < posX) return;

    if (dsize > 0) {
        int y_start_bar = posY + graphH - dsize;
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

/**
 * @brief Magas felbontású spektrum analizátor kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja.
 */
void MiniAudioFft::drawSpectrumHighRes() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (width == 0 || graphH <= 0) return;

    int actual_high_res_bins_to_display = width;

    for (int i = 0; i < actual_high_res_bins_to_display; i++) {
        int fft_bin_index = 2 + (i * (FFT_SAMPLES / 2 - 2)) / std::max(1, (actual_high_res_bins_to_display - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        int screen_x = posX + i;
        tft.drawFastVLine(screen_x, posY, graphH, TFT_BLACK);  // Oszlop törlése a grafikon területén

        int scaled_magnitude = (int)(RvReal[fft_bin_index] / AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, graphH - 1);

        if (scaled_magnitude > 0) {
            int y_bar_start = posY + graphH - 1 - scaled_magnitude;
            int bar_actual_height = (posY + graphH - 1) - y_bar_start + 1;
            if (bar_actual_height > 0) {
                tft.drawFastVLine(screen_x, y_bar_start, bar_actual_height, TFT_SKYBLUE);
            }
        }
    }
}

/**
 * @brief Oszcilloszkóp mód kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja.
 */
void MiniAudioFft::drawOscilloscope() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (width == 0 || graphH <= 0) return;

    int actual_osci_samples_to_draw = width;
    tft.fillRect(posX, posY, width, graphH, TFT_BLACK);  // Grafikon területének törlése

    int prev_x = -1, prev_y = -1;

    for (int i = 0; i < actual_osci_samples_to_draw; i++) {
        int num_available_samples = sizeof(osciSamples) / sizeof(osciSamples[0]);
        int sample_idx = (i * (num_available_samples - 1)) / std::max(1, (actual_osci_samples_to_draw - 1));
        sample_idx = constrain(sample_idx, 0, num_available_samples - 1);

        int raw_sample = osciSamples[sample_idx];
        double centered_sample = (static_cast<double>(raw_sample) - 2048.0) * OSCI_SENSITIVITY_FACTOR;
        double scaled_to_half_height = centered_sample * (static_cast<double>(graphH) / 2.0 - 1.0) / 2048.0;

        int y_pos = posY + graphH / 2 - static_cast<int>(round(scaled_to_half_height));
        y_pos = constrain(y_pos, posY, posY + graphH - 1);
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

/**
 * @brief Vízesés diagram kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja a rajzoláshoz.
 * A `wabuf` feltöltése a teljes komponens magasság (`this->height`) alapján történik.
 */
void MiniAudioFft::drawWaterfall() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (width == 0 || height == 0 || wabuf.empty() || wabuf[0].empty()) return;

    // 1. Adatok eltolása balra a `wabuf`-ban
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Új adatok betöltése a `wabuf` jobb szélére
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        double scaled_fft_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;
        wabuf[r][width - 1] = static_cast<int>(constrain(scaled_fft_val * 50.0, 0.0, 255.0));
    }

    // 3. Pixelek kirajzolása a grafikon területére
    if (graphH <= 0) return;  // Ha nincs hely a grafikonnak, ne rajzoljunk
    for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {
        int screen_y_relative_inverted = (r_wabuf * (graphH - 1)) / std::max(1, (height - 1));
        int screen_y_on_component = posY + (graphH - 1 - screen_y_relative_inverted);

        if (screen_y_on_component >= posY && screen_y_on_component < posY + graphH) {
            for (int c = 0; c < width; ++c) {
                uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r_wabuf][c]);
                tft.drawPixel(posX + c, screen_y_on_component, color);
            }
        }
    }
}

/**
 * @brief Értéket konvertál egy színné a vízesés diagramhoz a definiált színpaletta alapján.
 * @param scaled_value A skálázott bemeneti érték (pl. gradient * wabuf_érték).
 * @return A megfelelő RGB565 színkód.
 */
uint16_t MiniAudioFft::valueToWaterfallColor(int scaled_value) {
    using namespace MiniAudioFftConstants;
    float normalized = static_cast<float>(constrain(scaled_value, 0, MAX_WATERFALL_COLOR_INPUT_VALUE)) / static_cast<float>(MAX_WATERFALL_COLOR_INPUT_VALUE);
    byte color_size = sizeof(WATERFALL_COLORS) / sizeof(WATERFALL_COLORS[0]);
    int index = (int)(normalized * (color_size - 1));
    index = constrain(index, 0, color_size - 1);
    return WATERFALL_COLORS[index];
}

/**
 * @brief Burkológörbe (envelope) mód kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja.
 * A `wabuf` feltöltése a teljes komponens magasság (`this->height`) alapján történik.
 */
void MiniAudioFft::drawEnvelope() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) return;

    // 1. Adatok eltolása balra
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Új adatok betöltése
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        double scaled_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;
        double gained_val = scaled_val * ENVELOPE_INPUT_GAIN;
        wabuf[r][width - 1] = static_cast<int>(constrain(gained_val, 0.0, 255.0));
    }

    // 3. Burkológörbe kirajzolása
    const int half_graph_h = graphH / 2;
    // A simításhoz használt előző értéknek a `wabuf` index skáláján kell lennie (0-tól `height-1`-ig).
    // Ezért `height / 2`-vel inicializáljuk, nem `half_graph_h`-val.
    int prev_smoothed_env_idx_wabuf_scale = height / 2;

    for (int c = 0; c < width; ++c) {
        int env_idx_for_col_wabuf = height / 2;  // Domináns bin indexe a `wabuf` skáláján
        int max_val_in_col = 0;
        bool column_has_signal = false;

        for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {  // Teljes `this->height`
            if (wabuf[r_wabuf][c] > 0) column_has_signal = true;
            if (wabuf[r_wabuf][c] > max_val_in_col) {
                max_val_in_col = wabuf[r_wabuf][c];
                env_idx_for_col_wabuf = r_wabuf;
            }
        }

        // Simítás a `wabuf` index skáláján
        int smoothed_env_idx_wabuf = static_cast<int>(ENVELOPE_SMOOTH_FACTOR * prev_smoothed_env_idx_wabuf_scale + (1.0f - ENVELOPE_SMOOTH_FACTOR) * env_idx_for_col_wabuf);
        prev_smoothed_env_idx_wabuf_scale = smoothed_env_idx_wabuf;

        int col_x_on_screen = posX + c;
        tft.drawFastVLine(col_x_on_screen, posY, graphH, TFT_BLACK);  // Oszlop törlése a grafikon területén

        if (column_has_signal) {
            // A `smoothed_env_idx_wabuf` (0-tól `height-1`-ig) a vastagság alapja.
            for (int i = 0; i <= smoothed_env_idx_wabuf; ++i) {
                int y_offset = static_cast<int>(static_cast<float>(i) * ENVELOPE_THICKNESS_SCALER);
                y_offset = std::min(y_offset, half_graph_h - 1);  // Korlátozás a grafikon félmagasságára

                int yUpper = posY + half_graph_h - y_offset;
                int yLower = posY + half_graph_h + y_offset;

                yUpper = constrain(yUpper, posY, posY + graphH - 1);
                yLower = constrain(yLower, posY, posY + graphH - 1);

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

/**
 * @brief Kirajzolja a "MUTED" feliratot a képernyő közepére.
 *
 * A szöveg hátterét feketére állítja, így felülírja az előző szöveget.
 */
void MiniAudioFft::drawMuted() {

    if (width < 10 || height < 10) return;

    tft.setCursor(posX + width / 2, posY + height / 2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.setFreeFont();
    tft.setTextSize(1);
    tft.print("-- Muted --");
}
