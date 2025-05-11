#include "MiniAudioDisplay.h"

#include "FftBase.h"

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
}

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

    // Mini kijelző területének vastagabb és világosabb körvonala
    constexpr int frameThickness = 1;  // Keret vastagsága
    uint16_t frameColor = TFT_SILVER;  // Világosabb szürke a jobb láthatóságért

    int frameOuterX = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X - frameThickness;
    int frameOuterY = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y - frameThickness;
    int frameOuterW = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W + (frameThickness * 2);
    int frameOuterH = MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H + (frameThickness * 2);

    tft.fillRect(frameOuterX, frameOuterY, frameOuterW, frameOuterH, frameColor);  // Külső keret kitöltése
    // A belső területet (ahol a grafikonok vannak) a clearMiniDisplayArea() és a rajzoló függvények kezelik TFT_BLACK színnel.

    clearMiniDisplayArea();  // Most a SILVER keret után töröljük a belső részt feketére
    drawModeIndicator();
}

void MiniAudioDisplay::clearMiniDisplayArea() {
    tft.fillRect(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y, MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W,
                 MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H, TFT_BLACK);  // Fekete háttér
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
            modeText += "Spect (LowRes)";
            break;
        case 2:
            modeText += "Spect (HighRes)";
            break;
        case 3:
            modeText += "Scope";
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
    int osci_sample_idx = 0;  // Külön index az oszcilloszkóp mintákhoz
    // Az FFT.ino FFTSample logikája alapján
    for (int i = 0; i < FFT_SAMPLES; i++) {
        uint32_t sum = 0;
        // Túlmintavételezés, mint az FFT.ino-ban (dly=0 eset)
        for (int j = 0; j < 4; j++) {
            sum += analogRead(AUDIO_INPUT_PIN);  // Konstans használata
        }

        double averaged_sample = sum / 4.0;
        if (collectOsciSamples) {
            // Minden N-edik mintát mentünk az oszcilloszkóphoz
            // Például minden 2. mintát:
            if (i % 2 == 0 && osci_sample_idx < MINI_OSCI_SAMPLES_TO_DRAW) {
                osciSamples[osci_sample_idx] = static_cast<int>(averaged_sample);
                osci_sample_idx++;
            }
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

    //////////////////////tft.drawRect(actual_start_x_low_res - 1, MINI_DISPLAY_AREA_Y - 1, total_width_low_res + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    for (byte band_idx = 0; band_idx <= LOW_RES_BANDS; band_idx++) {
        if (Rpeak[band_idx] > 0) {
            int xPos = actual_start_x_low_res + 5 * band_idx;
            // Az y_erase_start a törlő téglalap tetejének Y koordinátája
            int y_erase_start = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - Rpeak[band_idx];
            
            // Biztosítjuk, hogy a törlés ne lógjon ki a MINI_DISPLAY_AREA_H területből
            // A maximális Y koordináta, amire még rajzolhatunk a fekete területen belül:
            int max_y_drawable_within_area = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1;
            int erase_height = 2; // Alapértelmezett törlési magasság
            if (y_erase_start + erase_height - 1 > max_y_drawable_within_area) {
                erase_height = max_y_drawable_within_area - y_erase_start + 1;
            }
            if (erase_height > 0 && y_erase_start <= max_y_drawable_within_area) {
                tft.fillRect(xPos, y_erase_start, 3, erase_height, TFT_BLACK);
            }
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

    //////////////////////tft.drawRect(actual_start_x_high_res - 1, MINI_DISPLAY_AREA_Y - 1, HIGH_RES_BINS_TO_DISPLAY + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    for (int i = 2; i < HIGH_RES_BINS_TO_DISPLAY + 2; i++) {
        if (i >= FFT_SAMPLES / 2) break;

        int screen_x = actual_start_x_high_res + (i - 2);
        if (screen_x >= actual_start_x_high_res + HIGH_RES_BINS_TO_DISPLAY) continue;

        tft.drawFastVLine(screen_x, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_H, TFT_BLACK);  // Oszlop törlése háttérszínnel

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

    // clearMiniDisplayArea();  // Terület törlése minden rajzolás előtt

    int x_pixel_offset_osci = 0;
    if (MINI_DISPLAY_AREA_W > MINI_OSCI_SAMPLES_TO_DRAW) {
        x_pixel_offset_osci = (MINI_DISPLAY_AREA_W - MINI_OSCI_SAMPLES_TO_DRAW) / 2;
    }
    int actual_start_x_osci = MINI_DISPLAY_AREA_X + x_pixel_offset_osci;

    // Előző jelalak törlése a háttérszínnel (TFT_BLACK)
    // Optimalizáció: Csak akkor töröljünk, ha volt mit.
    // De egyszerűbb minden ciklusban törölni a jelalak területét.
    tft.fillRect(actual_start_x_osci, MINI_DISPLAY_AREA_Y, MINI_OSCI_SAMPLES_TO_DRAW, MINI_DISPLAY_AREA_H, TFT_BLACK);

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

void MiniAudioDisplay::drawMiniWaterfall() {
    using namespace MiniAudioDisplayConstants;

    // 1. Adatok shiftelése balra
    for (int j = 0; j < MINI_DISPLAY_AREA_W - 1; j++) {
        for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {
            wabuf[i][j] = wabuf[i][j + 1];
        }
    }

    // 2. Új adatok betöltése a jobb szélre
    for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {
        int fft_bin_to_use = i + 2;  // Kezdjük a 2. bintől, mint az FFT.ino-ban
        if (fft_bin_to_use >= FFT_SAMPLES / 2) {
            wabuf[i][MINI_DISPLAY_AREA_W - 1] = 0;  // Ha túllépnénk, nullázzuk
        } else {
            // Az RvReal értékeit skálázzuk. A MINI_AMPLITUDE_SCALE-t úgy kell beállítani,
            // hogy az eredmény értelmes tartományban legyen a valueToMiniWaterfallColor számára.
            // Az FFT.ino-ban az amplitude kb. 200-300 volt.
            // A wabuf értékei legyenek pl. 0-255 között.
            double scaled_fft_val = RvReal[fft_bin_to_use] / MINI_AMPLITUDE_SCALE;
            // Korlátozzuk az értéket, hogy ne legyen túl nagy a színkonverzióhoz
            // Az FFT.ino-ban a wabuf értékei nem voltak expliciten korlátozva a betöltéskor.
            // Itt a wabuf tárolja a skálázott értéket, amit a gradienttel szorzunk a színfüggvényben.
            wabuf[i][MINI_DISPLAY_AREA_W - 1] = static_cast<int>(constrain(scaled_fft_val * 50.0, 0.0, 255.0));  // *50.0 egy kísérleti szorzó
        }
    }

    // 3. Pixelek kirajzolása
    // Mivel MINI_WF_WIDTH helyett MINI_DISPLAY_AREA_W-t használunk, nincs szükség x eltolásra
    int actual_start_x_wf = MINI_DISPLAY_AREA_X;

    // A vízesés Y pozíciójának kiszámítása, hogy a terület aljára kerüljön
    // Ha MINI_WF_HEIGHT helyett MINI_DISPLAY_AREA_H-t használunk, a base_y MINI_DISPLAY_AREA_Y lesz.
    int waterfall_base_y = MINI_DISPLAY_AREA_Y; // + MINI_DISPLAY_AREA_H - MINI_DISPLAY_AREA_H;

    for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {     // Y koordináta a kijelzőn (sor)
        for (int j = 0; j < MINI_DISPLAY_AREA_W; j++) {  // X koordináta a kijelzőn (oszlop)
            uint16_t color = valueToMiniWaterfallColor(MINI_WF_GRADIENT * wabuf[i][j]);
            // Az Y koordináta módosítva, hogy a vízesés alulra kerüljön
            int pixel_y_pos = waterfall_base_y + (MINI_DISPLAY_AREA_H - 1 - i);  // Fordított Y
            tft.drawPixel(actual_start_x_wf + j, pixel_y_pos, color);
        }
    }
}

void MiniAudioDisplay::drawMiniEnvelope() {
    using namespace MiniAudioDisplayConstants;

    // 1. Adatok shiftelése balra (megegyezik a vízeséssel)
    for (int j = 0; j < MINI_DISPLAY_AREA_W - 1; j++) {
        for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {
            wabuf[i][j] = wabuf[i][j + 1];
        }
    }

    // 2. Új adatok betöltése a jobb szélre
    // Az RvReal közvetlen FFT magnitúdókat tartalmaz.
    // A MINI_AMPLITUDE_SCALE-t használjuk a skálázáshoz, hogy a wabuf értékei
    // kezelhető tartományban legyenek (pl. 0-255).
    for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {
        int fft_bin_to_use = i + 2;  // Kezdjük a 2. bintől, mint az FFT.ino-ban
        if (fft_bin_to_use >= FFT_SAMPLES / 2) {
            wabuf[i][MINI_DISPLAY_AREA_W - 1] = 0;  // Ha túllépnénk, nullázzuk
        } else {
            double scaled_val = RvReal[fft_bin_to_use] / MINI_AMPLITUDE_SCALE;
            wabuf[i][MINI_DISPLAY_AREA_W - 1] = static_cast<int>(constrain(scaled_val, 0.0, 255.0));
        }
    }

    // 3. Rajzolási paraméterek
    // Mivel MINI_WF_WIDTH helyett MINI_DISPLAY_AREA_W-t használunk, nincs szükség x eltolásra
    int actual_start_x_env = MINI_DISPLAY_AREA_X;

    // A burkológörbe Y pozíciója. Mivel MINI_DISPLAY_AREA_H == (volt MINI_WF_HEIGHT),
    // a MINI_DISPLAY_AREA_Y-tól indulunk.
    int actual_draw_y_start = MINI_DISPLAY_AREA_Y;
    const int half_h = MINI_DISPLAY_AREA_H / 2;

    // Az FFT.ino-ban a 'prev' lokális volt a rajzoló cikluson belül,
    // ami azt jelenti, hogy a simítás csak az aktuális képkocka oszlopai között történik.
    int prev_smoothed_env_idx = half_h;  // Előző simított index a simításhoz az oszlopok között
    static constexpr float ENVELOPE_SMOOTH_FACTOR = 0.25f;

    // 4. Burkológörbe kirajzolása
    for (int j = 0; j < MINI_DISPLAY_AREA_W; j++) {  // Minden oszlopra
        int env_idx_for_col_j = half_h;        // Az aktuális oszlopban a max. értékű bin indexe, középről indulva
        int max_val_in_col = 0;                // Az aktuális oszlopban talált maximális érték
        bool column_has_signal = false;        // Jelzi, ha van pozitív jel az oszlopban

        // Domináns frekvencia (max. magnitúdójú bin indexének) keresése az aktuális oszlopban
        for (int i = 0; i < MINI_DISPLAY_AREA_H; i++) {
            if (wabuf[i][j] > 0) {
                column_has_signal = true;
            }
            if (wabuf[i][j] > max_val_in_col) {
                max_val_in_col = wabuf[i][j];
                env_idx_for_col_j = i;  // Ez az 'i' (bin index) lesz az alapja a burkológörbe magasságának
            }
        }
        // Ha egyáltalán nem volt jel az oszlopban, env_idx_for_col_j marad half_h (középen)
        // és max_val_in_col 0 lesz. A column_has_signal jelzi ezt.

        // Az 'env_idx_for_col_j' (domináns bin indexének) simítása
        // Az FFT.ino ezt az indexet simítja, nem a magnitúdót.
        int smoothed_env_idx = static_cast<int>(ENVELOPE_SMOOTH_FACTOR * prev_smoothed_env_idx + (1.0f - ENVELOPE_SMOOTH_FACTOR) * env_idx_for_col_j);
        prev_smoothed_env_idx = smoothed_env_idx;  // Elmentjük a következő oszlophoz

        int col_x_on_screen = actual_start_x_env + j;

        // Oszlop törlése
        tft.drawFastVLine(col_x_on_screen, actual_draw_y_start, MINI_DISPLAY_AREA_H, TFT_BLACK);

        if (column_has_signal) {  // Csak akkor rajzolunk, ha volt jel az oszlopban
            // A burkológörbe vizuális "vastagsága" a simított domináns bin indexétől függ
            // Minél magasabb a domináns frekvencia indexe, annál "vastagabb" a görbe
            for (int i = 0; i <= smoothed_env_idx; i++) {
                // Az 'i' itt a "vastagságot" szabályozza, 0-tól a smoothed_env_idx-ig.
                // A /2 azért van, hogy szimmetrikus legyen a half_h körül.
                int yUpper = actual_draw_y_start + half_h - i / 2;
                int yLower = actual_draw_y_start + half_h + i / 2;

                // Biztosítjuk, hogy a pixelek a kijelölt területen belül maradjanak
                yUpper = constrain(yUpper, actual_draw_y_start, actual_draw_y_start + MINI_DISPLAY_AREA_H - 1);
                yLower = constrain(yLower, actual_draw_y_start, actual_draw_y_start + MINI_DISPLAY_AREA_H - 1);

                if (yUpper <= yLower) {  // Csak akkor rajzolunk, ha van értelme (pl. i/2 ne legyen túl nagy)
                    tft.drawPixel(col_x_on_screen, yUpper, TFT_WHITE);
                    if (yUpper != yLower) {  // Ne rajzoljuk kétszer ugyanazt a pixelt, ha i=0
                        tft.drawPixel(col_x_on_screen, yLower, TFT_WHITE);
                    }
                }
            }
        }
    }
}

// Egyedi színskála a mini vízeséshez, hogy a háttér biztosan fekete legyen
namespace MiniWaterfallColors {
const uint16_t miniColors0[16] = {
    0x0000,                                 // TFT_BLACK (index 0)
    0x0000,                                 // TFT_BLACK (index 1)
    0x0000,                                 // TFT_BLACK (index 2) - Még egy fekete szint a tisztább háttérért
    0x001F,                                 // Nagyon sötét kék
    0x081F,                                 // Sötét kék
    0x0810,                                 // Sötét zöldeskék
    0x0800,                                 // Sötétzöld
    0x0C00,                                 // Közepes zöld
    0x1C00,                                 // Világosabb zöld
    0xFC00,                                 // Narancs
    0xFDE0,                                 // Világos sárga
    0xFFE0,                                 // Sárga
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF          // Fehér a csúcsokhoz
};
}

uint16_t MiniAudioDisplay::valueToMiniWaterfallColor(int scaled_value) {
    // scaled_value = gradient * wabuf_value
    // Tegyük fel, hogy a wabuf értékei (RvReal[i+2] / MINI_AMPLITUDE_SCALE * 50.0) kb. 0-255 közöttiek.
    // Ha MINI_WF_GRADIENT = 80 (a konstansokból), akkor scaled_value max kb. 80 * 255 = 20400.
    // Ezt kell 0-15 indexre skálázni.
    const int MAX_COLOR_INPUT_VALUE = 20000;  // Közelebb a tényleges maximumhoz (20400)
                                              // Ha a gradient*150 = 15000), akkor ez jó.
    float normalized = static_cast<float>(constrain(scaled_value, 0, MAX_COLOR_INPUT_VALUE)) / static_cast<float>(MAX_COLOR_INPUT_VALUE);

    const uint16_t* colors = MiniWaterfallColors::miniColors0;  // Az egyedi palettát használjuk
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
