#include "MiniAudioFft.h"

#include <cmath>  // std::round, std::max, std::min

#include "rtVars.h"  // rtv::muteStat eléréséhez

// Konstans a módkijelző láthatósági idejéhez (ms)
constexpr uint32_t MODE_INDICATOR_TIMEOUT_MS = 20000;  // 20 másodperc

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
 * @param configModeField Referencia a Config_t megfelelő uint8_t mezőjére, ahova a módot menteni kell.
 */
MiniAudioFft::MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h, uint8_t& configModeField)
    : tft(tft_ref),
      posX(x),
      posY(y),
      width(w),
      height(h),
      // currentMode itt nem kap explicit kezdőértéket, a setInitialMode állítja be
      prevMuteState(rtv::muteStat),         // Némítás előző állapotának inicializálása
      modeIndicatorShowUntil(0),            // Kezdetben nem látható (setInitialMode állítja)
      isIndicatorCurrentlyVisible(true),    // Kezdetben látható (setInitialMode állítja)
      lastTouchProcessTime(0),              // Debounce időzítő nullázása
      configModeFieldRef(configModeField),  // Referencia elmentése
      FFT(),                                // FFT objektum inicializálása
      highResOffset(0),
      envelope_prev_smoothed_max_val(0.0f) { // Új tagváltozó inicializálása
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
 * @brief Beállítja a komponens kezdeti megjelenítési módját.
 * Ezt a szülő képernyő hívja meg a konstruktor után, a configból kiolvasott értékkel.
 * @param mode A beállítandó DisplayMode.
 */
void MiniAudioFft::setInitialMode(DisplayMode mode) {
    currentMode = mode;
    isIndicatorCurrentlyVisible = true;  // Induláskor mindig látható
    modeIndicatorShowUntil = millis() + MODE_INDICATOR_TIMEOUT_MS;
    forceRedraw();  // Ez gondoskodik a clearArea-ról és a drawModeIndicator-ról
}

/**
 * @brief Vált a következő megjelenítési módra.
 *
 * Körbejár a rendelkezésre álló módok között (0-tól 5-ig).
 * Módváltáskor törli a komponens területét, kirajzolja az új mód nevét,
 * és reseteli a mód-specifikus puffereket.
 */
void MiniAudioFft::cycleMode() {
    // Az enum értékének növelése és körbejárás
    uint8_t modeValue = static_cast<uint8_t>(currentMode);
    modeValue++;
    if (modeValue > static_cast<uint8_t>(DisplayMode::TuningAid)) {  // Az utolsó érvényes mód most a TuningAid
        modeValue = static_cast<uint8_t>(DisplayMode::Off);          // Visszaugrás az Off-ra
    }
    currentMode = static_cast<DisplayMode>(modeValue);

    // Új mód mentése a konfigurációba a referencián keresztül
    configModeFieldRef = static_cast<uint8_t>(currentMode);

    // Módkijelző időzítőjének és láthatóságának beállítása
    isIndicatorCurrentlyVisible = true;
    modeIndicatorShowUntil = millis() + MODE_INDICATOR_TIMEOUT_MS;

    // Mód-specifikus pufferek resetelése
    if (currentMode == DisplayMode::SpectrumLowRes || (currentMode != DisplayMode::SpectrumLowRes && Rpeak[0] != 0)) {
        memset(Rpeak, 0, sizeof(Rpeak));
    }
    if (currentMode == DisplayMode::Oscilloscope || (currentMode != DisplayMode::Oscilloscope && osciSamples[0] != 2048)) {
        for (int i = 0; i < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++i) osciSamples[i] = 2048;
    }
    if (currentMode == DisplayMode::Waterfall || currentMode == DisplayMode::Envelope || currentMode == DisplayMode::TuningAid ||  // TuningAid is használja a wabuf-ot
        (currentMode < DisplayMode::Waterfall && !wabuf.empty() && !wabuf[0].empty() && wabuf[0][0] != 0)) {                       // Ha korábbi módból váltunk, ami nem használta
        for (auto& row : wabuf) std::fill(row.begin(), row.end(), 0);
    }
    if (currentMode == DisplayMode::Envelope || (currentMode != DisplayMode::Envelope && envelope_prev_smoothed_max_val != 0.0f)) {
        envelope_prev_smoothed_max_val = 0.0f; // Envelope simítási előzmény nullázása
    }
    highResOffset = 0;  // Magas felbontású spektrum eltolásának resetelése

    forceRedraw();  // Teljes újrarajzolás, ami kezeli a clearArea-t és a drawModeIndicator-t
}

/**
 * @brief Letörli a komponens teljes rajzolási területét feketére, és megrajzolja a keretet.
 * A keret magassága az `isIndicatorCurrentlyVisible` állapottól függ.
 */
void MiniAudioFft::clearArea() {
    constexpr int frameThickness = 1;
    int effectiveH = getEffectiveHeight();

    // 1. Meghatározzuk a komponens maximális lehetséges külső határait
    //    (beleértve a keretet is, amikor a módkijelző látható volt).
    //    Ez a terület lesz feketére törölve, hogy a régi keretmaradványok eltűnjenek.
    int maxOuterX = posX - frameThickness;
    int maxOuterY = posY - frameThickness;
    int maxOuterW = width + (frameThickness * 2);
    int maxOuterH_forClear = height + (frameThickness * 2);  // A komponens maximális magasságát használjuk a törléshez

    // Teljes maximális terület törlése feketére
    tft.fillRect(maxOuterX, maxOuterY, maxOuterW, maxOuterH_forClear, TFT_BLACK);

    // 2. Az ÚJ keret kirajzolása az AKTUÁLIS effektív magasság alapján.
    int currentFrameOuterX = posX - frameThickness;
    int currentFrameOuterY = posY - frameThickness;
    int currentFrameOuterW = width + (frameThickness * 2);
    int currentFrameOuterH_forNewFrame = effectiveH + (frameThickness * 2);  // Keret az aktuális effektív magassághoz

    tft.drawRect(currentFrameOuterX, currentFrameOuterY, currentFrameOuterW, currentFrameOuterH_forNewFrame, TFT_COLOR(80, 80, 80));
}

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
 * @brief Visszaadja a komponens aktuális effektív magasságát,
 * figyelembe véve a módkijelző láthatóságát.
 * @return Az effektív magasság pixelekben.
 */
int MiniAudioFft::getEffectiveHeight() const {
    if (isIndicatorCurrentlyVisible) {
        return height;  // Teljes magasság, ha a kijelző látszik
    } else {
        return height - getIndicatorAreaHeight();  // Csökkentett magasság, ha nem látszik
    }
}

/**
 * @brief Kiszámítja és visszaadja a grafikonok számára ténylegesen rendelkezésre álló magasságot.
 * Ez a komponens teljes magassága mínusz a módkijelzőnek MINDIG fenntartott terület,
 * függetlenül attól, hogy a kijelző éppen látható-e. A grafikonok nem nyúlnak bele ebbe a sávba.
 * @return int A grafikonok rajzolási magassága pixelekben.
 */
int MiniAudioFft::getGraphHeight() const { return height - getIndicatorAreaHeight(); }

/**
 * @brief Kiszámítja és visszaadja a módkijelző területének Y kezdőpozícióját a komponensen belül.
 * Ez mindig a grafikon területe alatt van.
 * @return int A módkijelző Y kezdőpozíciója.
 */
int MiniAudioFft::getIndicatorAreaY() const { return posY + getGraphHeight(); }

/**
 * @brief Kirajzolja az aktuális mód nevét a komponens aljára, a számára fenntartott sávba,
 * de csak akkor, ha az `isIndicatorCurrentlyVisible` igaz.
 */
void MiniAudioFft::drawModeIndicator() {
    if (!isIndicatorCurrentlyVisible) {
        // Ha nem látható, akkor a `clearArea` már a kisebb kerettel törölt.
        // Azt a területet, ahol a módkijelző volt, expliciten feketére kell állítani,
        // hogy eltűnjön a szöveg, amikor az `isIndicatorCurrentlyVisible` false-ra vált.
        // Ezt a `loop` fogja kezelni a `forceRedraw` hívásával, amikor a láthatóság változik.
        // Itt elég, ha nem rajzolunk semmit. A `forceRedraw` `clearArea`-ja gondoskodik a háttérről.
        return;
    }

    int indicatorH = getIndicatorAreaHeight();
    int indicatorYstart = getIndicatorAreaY();  // A módkijelző sávjának teteje

    if (width < 20 || indicatorH < 8) return;

    tft.setFreeFont();
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);  // Háttér fekete, ez törli az előzőt
    tft.setTextDatum(BC_DATUM);               // Alul-középre igazítás

    String modeText = "";
    switch (currentMode) {
        case DisplayMode::Off:
            modeText = "Off";
            break;
        case DisplayMode::SpectrumLowRes:
            modeText = "FFT LowRes";
            break;
        case DisplayMode::SpectrumHighRes:
            modeText = "FFT HighRes";
            break;
        case DisplayMode::Oscilloscope:
            modeText = "Scope";
            break;
        case DisplayMode::Waterfall:
            modeText = "WaterFall";
            break;
        case DisplayMode::Envelope:
            modeText = "Envelope";
            break;
        case DisplayMode::TuningAid:
            modeText = "Tune CW";  // Vagy általánosabb "Tune Aid"
            break;
        default:
            modeText = "Unk";
            break;
    }

    // Módkijelző területének explicit törlése a szövegkirajzolás előtt
    tft.fillRect(posX, indicatorYstart, width, indicatorH, TFT_BLACK);
    // Szöveg kirajzolása a komponens aljára, középre.
    // Az Y koordináta a szöveg alapvonala lesz.
    // A `posY + height - 2` a teljes komponensmagasság aljára igazít.
    tft.drawString(modeText, posX + width / 2, posY + height - 2);
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
    bool needsFullRedrawAfterChecks = false;
    bool muteStateChangedThisLoop = (rtv::muteStat != prevMuteState);
    prevMuteState = rtv::muteStat;

    if (muteStateChangedThisLoop) {
        needsFullRedrawAfterChecks = true;  // Némítás változásakor mindig teljes újrarajzolás
    }

    // Módkijelző láthatóságának időzítő alapú ellenőrzése
    if (isIndicatorCurrentlyVisible && millis() >= modeIndicatorShowUntil) {
        isIndicatorCurrentlyVisible = false;
        needsFullRedrawAfterChecks = true;  // Láthatóság változott, teljes újrarajzolás kell
    }

    if (needsFullRedrawAfterChecks) {
        forceRedraw();  // Ez kezeli a némítást, a módot, és a kijelzőt
        return;         // Ha újrarajzoltunk, ebben a ciklusban nincs több teendő
    }

    // Ha némítva van (és nem most változott az állapota, mert azt már a forceRedraw kezelte)
    if (rtv::muteStat) {
        return;  // A "MUTED" felirat már kint van a forceRedraw miatt
    }

    // Ha a mód "Ki" (és nem most változott az állapota)
    if (currentMode == DisplayMode::Off) {
        return;  // Az "Off" felirat már kint van
    }

    // --- Csak akkor jutunk ide, ha nincs némítás, a mód nem "Off", és nem volt állapotváltozás miatti forceRedraw ---

    // FFT mintavételezés és számítás
    performFFT(currentMode == DisplayMode::Oscilloscope);

    // Grafikonok kirajzolása a nekik szánt (csökkentett) területre
    // Ezek a függvények a `posY`-tól `posY + getGraphHeight() - 1`-ig rajzolnak.
    // A `getGraphHeight()` mindig a grafikon magasságát adja vissza, a módkijelző sávja nélkül.
    switch (currentMode) {
        case DisplayMode::SpectrumLowRes:
            drawSpectrumLowRes();
            break;
        case DisplayMode::SpectrumHighRes:
            drawSpectrumHighRes();
            break;
        case DisplayMode::Oscilloscope:
            drawOscilloscope();
            break;
        case DisplayMode::Waterfall:
            drawWaterfall();
            break;
        case DisplayMode::Envelope:
            drawEnvelope();
            break;
        case DisplayMode::TuningAid:
            drawTuningAid();
            break;
            // Nem kell default, mert a currentMode mindig érvényes DisplayMode érték.
    }
    // A drawModeIndicator() metódust NEM hívjuk itt minden ciklusban,
    // csak akkor, ha a mód/láthatóság ténylegesen megváltozik (cycleMode, forceRedraw).
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
        // Debounce logika: Csak akkor dolgozzuk fel, ha elegendő idő telt el az utolsó feldolgozás óta
        if (millis() - lastTouchProcessTime > MiniAudioFftConstants::TOUCH_DEBOUNCE_MS) {
            lastTouchProcessTime = millis(); // Időbélyeg frissítése
            cycleMode();  // Ez beállítja az isIndicatorCurrentlyVisible-t, a timert, és hívja a forceRedraw-t
            return true;  // Kezeltük az érintést
        }
        // Ha túl gyorsan jött az érintés, akkor is jelezzük, hogy a komponens területén volt,
        // de nem váltunk módot, hogy más ne kezelje le.
        return true; 
    }
    return false;
}

/**
 * @brief Kényszeríti a komponens teljes újrarajzolását az aktuális módban.
 *
 * Letörli a komponenst az effektív magasságnak megfelelően, és újrarajzolja a tartalmat.
 */
void MiniAudioFft::forceRedraw() {
    clearArea();  // Ez már az `getEffectiveHeight()`-et használja a kerethez és a törléshez

    if (rtv::muteStat) {
        drawMuted();  // A drawMuted-nek is az effektív magasság közepére kell rajzolnia
    } else if (currentMode == DisplayMode::Off) {
        drawModeIndicator();  // "Off" kirajzolása (ha látható)
    } else {
        // Aktív módok (1-5): grafikon kirajzolása
        performFFT(currentMode == DisplayMode::Oscilloscope);
        switch (currentMode) {
            case DisplayMode::SpectrumLowRes:
                drawSpectrumLowRes();
                break;
            case DisplayMode::SpectrumHighRes:
                drawSpectrumHighRes();
                break;
            case DisplayMode::Oscilloscope:
                drawOscilloscope();
                break;
            case DisplayMode::Waterfall:
                drawWaterfall();
                break;
            case DisplayMode::Envelope:
                drawEnvelope();
                break;
            case DisplayMode::TuningAid:
                drawTuningAid();
                break;
        }
        drawModeIndicator();  // És a módkijelző kirajzolása (ha látható)
    }
}

// --- Rajzoló metódusok (a MiniAudioDisplay.cpp alapján adaptálva) ---
// A grafikonrajzoló függvények (drawSpectrumLowRes, stb.) változatlanok maradnak,
// mivel a `getGraphHeight()` által visszaadott magasságot használják,
// ami most már konzisztensen a módkijelző feletti terület magasságát jelenti.
// A `posY` szintén a grafikonterület tetejét jelöli.

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

    constexpr int bar_width_pixels = 3;
    constexpr int bar_gap_pixels = 2;
    constexpr int bar_total_width_pixels = bar_width_pixels + bar_gap_pixels;

    int num_drawable_bars = width / bar_total_width_pixels;
    int bands_to_process = std::min(LOW_RES_BANDS, num_drawable_bars);
    if (bands_to_process == 0 && LOW_RES_BANDS > 0) bands_to_process = 1;

    int total_drawn_width = (bands_to_process * bar_width_pixels) + (std::max(0, bands_to_process - 1) * bar_gap_pixels);
    int x_offset = (width - total_drawn_width) / 2;
    int current_draw_x_on_screen = posX + x_offset;

    // Grafikon területének törlése (csak a grafikon sávja)
    // Ezt a fő `clearArea` már elvégezte, vagy a `loop` frissíti az oszlopokat.
    // Itt csak a csúcsokat töröljük.
    for (int band_idx = 0; band_idx < bands_to_process; band_idx++) {
        if (Rpeak[band_idx] > 0) {
            int xP = current_draw_x_on_screen + bar_total_width_pixels * band_idx;
            int yP = posY + graphH - Rpeak[band_idx];  // Y a grafikon területén belül
            int erase_h = std::min(2, posY + graphH - yP);
            if (yP < posY + graphH && erase_h > 0) {  // Biztosítjuk, hogy a grafikonon belül törlünk
                tft.fillRect(xP, yP, bar_width_pixels, erase_h, TFT_BLACK);
            }
        }
        if (Rpeak[band_idx] >= 1) Rpeak[band_idx] -= 1;
    }

    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {
        if (RvReal[i] > (AMPLITUDE_SCALE / 10.0)) {
            byte band_idx = getBandVal(i);
            if (band_idx < bands_to_process) {
                drawSpectrumBar(band_idx, (int)RvReal[i], current_draw_x_on_screen, actual_low_res_peak_max_height);
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
 * @brief Kirajzol egyetlen oszlopot/sávot (bar-t) az alacsony felbontású spektrumhoz.
 * @param band_idx A frekvenciasáv indexe, amelyhez az oszlop tartozik.
 * @param magnitude A sáv magnitúdója.
 * @param actual_start_x_on_screen A spektrum rajzolásának kezdő X koordinátája a képernyőn.
 * @param peak_max_height_for_mode A sáv maximális magassága az adott módban.
 */
void MiniAudioFft::drawSpectrumBar(int band_idx, int magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode) {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (graphH <= 0) return;

    int dsize = magnitude / AMPLITUDE_SCALE;
    dsize = constrain(dsize, 0, peak_max_height_for_mode);  // peak_max_height_for_mode már graphH-1

    constexpr int bar_width_pixels = 3;
    constexpr int bar_gap_pixels = 2;
    constexpr int bar_total_width_pixels = bar_width_pixels + bar_gap_pixels;
    int xPos = actual_start_x_on_screen + bar_total_width_pixels * band_idx;

    if (xPos + bar_width_pixels > posX + width || xPos < posX) return;

    if (dsize > 0) {
        int y_start_bar = posY + graphH - dsize;  // Y a grafikon területén belül
        int bar_h_visual = dsize; // A ténylegesen kirajzolandó magasság
        // Biztosítjuk, hogy a sáv a grafikon területén belül maradjon
        if (y_start_bar < posY) {  // Elvileg a constrain(dsize) ezt kezeli
            bar_h_visual -= (posY - y_start_bar);
            y_start_bar = posY;
        }
        if (bar_h_visual > 0) {
            tft.fillRect(xPos, y_start_bar, bar_width_pixels, bar_h_visual, TFT_YELLOW);
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
        y_pos = constrain(y_pos, posY, posY + graphH - 1);  // Korlátozás a grafikon területére
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
    int graphH = getGraphHeight();  // A grafikon tényleges rajzolási magassága
    // A `wabuf` sorainak száma `this->height`, oszlopainak száma `this->width`.
    if (width == 0 || height == 0 || wabuf.empty() || wabuf[0].empty()) return;

    // 1. Adatok eltolása balra a `wabuf`-ban
    for (int r = 0; r < height; ++r) {  // A teljes `this->height` magasságon iterálunk a `wabuf` miatt
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Új adatok betöltése a `wabuf` jobb szélére
    for (int r = 0; r < height; ++r) {  // A teljes `this->height` magasságon iterálunk
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        double scaled_fft_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;
        wabuf[r][width - 1] = static_cast<int>(constrain(scaled_fft_val * 50.0, 0.0, 255.0));
    }

    // 3. Pixelek kirajzolása a grafikon területére
    if (graphH <= 0) return;
    for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {  // Iterálás a `wabuf` összes során
        // Átskálázzuk a `r_wabuf` indexet a `graphH` magasságra a képernyőn való megjelenítéshez.
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
    // Az 'envelope_prev_smoothed_max_val' tagváltozót használjuk a simításhoz

    for (int c = 0; c < width; ++c) {
        // int env_idx_for_col_wabuf = height / 2; // Erre már nincs szükség az amplitúdó alapú rajzolásnál
        int max_val_in_col = 0;
        bool column_has_signal = false;

        for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {  // Teljes `this->height`
            if (wabuf[r_wabuf][c] > 0) column_has_signal = true;
            if (wabuf[r_wabuf][c] > max_val_in_col) {
                max_val_in_col = wabuf[r_wabuf][c];
                // env_idx_for_col_wabuf = r_wabuf; // Erre már nincs szükség
            }
        }

        // A maximális amplitúdó simítása az oszlopban
        float current_col_max_amplitude = static_cast<float>(max_val_in_col);
        // A tagváltozót használjuk a simításhoz az oszlopok között
        envelope_prev_smoothed_max_val = ENVELOPE_SMOOTH_FACTOR * envelope_prev_smoothed_max_val + (1.0f - ENVELOPE_SMOOTH_FACTOR) * current_col_max_amplitude;

        int col_x_on_screen = posX + c;
        tft.drawFastVLine(col_x_on_screen, posY, graphH, TFT_BLACK);  // Oszlop törlése a grafikon területén

        // Csak akkor rajzolunk, ha van jel vagy a simított érték számottevő
       if (column_has_signal || envelope_prev_smoothed_max_val > 0.5f) {
            // A simított amplitúdó skálázása a grafikon magasságára (0-255 -> 0-half_graph_h)
            float y_offset_float = (envelope_prev_smoothed_max_val / 255.0f) * (half_graph_h - 1.0f);
            int y_offset_pixels = static_cast<int>(round(y_offset_float));
            y_offset_pixels = std::min(y_offset_pixels, half_graph_h - 1); // Biztosítjuk, hogy a határokon belül maradjon
            if (y_offset_pixels < 0) y_offset_pixels = 0;

            if (y_offset_pixels >= 0) { // Ha van vastagság (akár 0 is, ami egy középvonalat jelenthet)
                int yCenter = posY + half_graph_h;
                int yUpper = yCenter - y_offset_pixels;
                int yLower = yCenter + y_offset_pixels;

                yUpper = constrain(yUpper, posY, posY + graphH - 1);
                yLower = constrain(yLower, posY, posY + graphH - 1);

                if (yUpper <= yLower) { // Biztosítjuk, hogy van mit rajzolni
                    // Kitöltött burkológörbe rajzolása
                    tft.drawFastVLine(col_x_on_screen, yUpper, yLower - yUpper + 1, TFT_WHITE);
                }
            }
        }
    }
}

/**
 * @brief Hangolási segéd mód kirajzolása (szűkített vízesés célvonallal).
 *
 * A `getGraphHeight()` által visszaadott magasságot használja a rajzoláshoz.
 * A `wabuf` feltöltése a TUNING_AID_DISPLAY_MIN_FREQ_HZ és MAX_FREQ_HZ közötti FFT bin-ek alapján történik.
 */
void MiniAudioFft::drawTuningAid() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();  // A grafikon tényleges rajzolási magassága
    if (width == 0 || height == 0 || wabuf.empty() || wabuf[0].empty() || graphH <= 0) return;

    // 1. Adatok eltolása "lefelé" a `wabuf`-ban (időbeli léptetés)
    // Csak a grafikon magasságáig (`graphH`) használjuk a `wabuf` sorait.
    for (int r = graphH - 1; r > 0; --r) {  // Utolsó sortól a másodikig
        for (int c = 0; c < width; ++c) {   // Minden oszlop (frekvencia bin)
            wabuf[r][c] = wabuf[r - 1][c];
        }
    }

    // 2. Új adatok betöltése a `wabuf[0]` sorába (legfrissebb spektrum)
    //    a hangolási tartományból.
    const float binWidthHz = static_cast<float>(SAMPLING_FREQUENCY) / FFT_SAMPLES;
    const int min_fft_bin_for_tuning = std::max(2, static_cast<int>(std::round(TUNING_AID_DISPLAY_MIN_FREQ_HZ / binWidthHz)));
    const int max_fft_bin_for_tuning = std::min(static_cast<int>(FFT_SAMPLES / 2 - 1), static_cast<int>(std::round(TUNING_AID_DISPLAY_MAX_FREQ_HZ / binWidthHz)));
    const int num_bins_in_tuning_range = std::max(1, max_fft_bin_for_tuning - min_fft_bin_for_tuning + 1);

    for (int c_col = 0; c_col < width; ++c_col) {  // c_col a képernyő X pozíciója / wabuf oszlop indexe
        // Képernyő oszlop (c_col) leképezése egy FFT bin indexre a kívánt tartományon belül
        float ratio_in_display_width = (width == 1) ? 0.0f : (static_cast<float>(c_col) / (width - 1));
        int fft_bin_index = min_fft_bin_for_tuning + static_cast<int>(std::round(ratio_in_display_width * (num_bins_in_tuning_range - 1)));
        // Biztosítjuk, hogy az index a számított és az abszolút érvényes tartományon belül maradjon
        fft_bin_index = constrain(fft_bin_index, min_fft_bin_for_tuning, max_fft_bin_for_tuning);
        fft_bin_index = constrain(fft_bin_index, 2, static_cast<int>(FFT_SAMPLES / 2 - 1));

        double scaled_fft_val = RvReal[fft_bin_index] / AMPLITUDE_SCALE;

        // Érzékenység beállítása a hangolási segédhez.
        // Legyen ugyanaz, mint a normál vízesésnél, ami a felhasználó szerint "egész jó".
        wabuf[0][c_col] = static_cast<int>(constrain(scaled_fft_val * 50.0f, 0.0, 255.0));
    }

    // 3. Pixelek kirajzolása a grafikon területére
    // A sorok (r) az időt, az oszlopok (c) a frekvenciát jelentik.
    for (int r_time = 0; r_time < graphH; ++r_time) {     // Y koordináta a képernyőn (idő)
        for (int c_freq = 0; c_freq < width; ++c_freq) {  // X koordináta a képernyőn (frekvencia)
            // A wabuf[r_time][c_freq] tartalmazza a színerősséget
            uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r_time][c_freq]);
            tft.drawPixel(posX + c_freq, posY + r_time, color);
        }
    }

    // 4. Célfrekvencia vonalának kirajzolása (FÜGGŐLEGES vonal)
    // A ténylegesen megjelenített frekvenciatartomány alsó és felső határa Hz-ben

    float min_freq_displayed_actual = static_cast<float>(min_fft_bin_for_tuning) * binWidthHz;
    float max_freq_displayed_actual = static_cast<float>(max_fft_bin_for_tuning) * binWidthHz;
    float displayed_span_hz = max_freq_displayed_actual - min_freq_displayed_actual;

    // Csak akkor rajzoljuk a vonalat, ha a célfrekvencia a megjelenített tartományon belül van
    if (displayed_span_hz > 0 && TUNING_AID_TARGET_FREQ_HZ >= min_freq_displayed_actual && TUNING_AID_TARGET_FREQ_HZ <= max_freq_displayed_actual) {

        // Arány kiszámítása: hol helyezkedik el a célfrekvencia a megjelenített sávon belül
        float ratio = (TUNING_AID_TARGET_FREQ_HZ - min_freq_displayed_actual) / displayed_span_hz;

        // X pozíció kiszámítása a komponens szélességén
        int line_x_on_graph = static_cast<int>(std::round(ratio * (width - 1)));
        int final_line_x = posX + line_x_on_graph;  // Abszolút X pozíció a képernyőn

        // Függőleges vonal kirajzolása a grafikon teljes magasságában
        tft.drawFastVLine(final_line_x, posY, graphH, TUNING_AID_TARGET_LINE_COLOR);

        // Opcionálisan egy második vonal a jobb láthatóságért
        if (final_line_x + 1 < posX + width) {  // Biztosítjuk, hogy a komponensen belül maradjon
            tft.drawFastVLine(final_line_x + 1, posY, graphH, TUNING_AID_TARGET_LINE_COLOR);
        }
    }
}

/**
 * @brief Kirajzolja a "MUTED" feliratot a komponens aktuális effektív magasságának közepére.
 */
void MiniAudioFft::drawMuted() {
    int effectiveH = getEffectiveHeight();
    if (width < 10 || effectiveH < 10) return;  // Nincs elég hely

    // A `clearArea` már törölte a hátteret az `effectiveH` magasságig.
    // Ide rajzoljuk a "MUTED" feliratot.
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.setFreeFont();
    tft.setTextSize(1);
    // A drawString használata a setCursor + print helyett a pontosabb középre igazításért MC_DATUM mellett.
    // A háttérszínt a clearArea már beállította.
    tft.drawString("-- Muted --", posX + width / 2, posY + effectiveH / 2);
}
