#include "MiniAudioFft.h"

#include <cmath>  // std::round, std::max, std::min, std::abs

#include "Config.h"  // Szükséges a config.data eléréséhez
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
 * @param fftGainConfigRef Referencia a Config_t megfelelő float mezőjére az FFT erősítés konfigurációjához.
 */
MiniAudioFft::MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h, uint8_t& configDisplayModeFieldRef, float& fftGainConfigRef)
    : tft(tft_ref),
      posX(x),
      posY(y),
      width(w),
      height(h),
      // currentMode itt nem kap explicit kezdőértéket, a setInitialMode állítja be
      prevMuteState(rtv::muteStat),                   // Némítás előző állapotának inicializálása
      modeIndicatorShowUntil(0),                      // Kezdetben nem látható (setInitialMode állítja)
      isIndicatorCurrentlyVisible(true),              // Kezdetben látható (setInitialMode állítja)
      lastTouchProcessTime(0),                        // Debounce időzítő nullázása
      configModeFieldRef(configDisplayModeFieldRef),  // Referencia elmentése a kijelzési módhoz
      activeFftGainConfigRef(fftGainConfigRef),       // Referencia elmentése az erősítés konfigurációhoz
      FFT(),                                          // FFT objektum inicializálása
      highResOffset(0),
      envelope_prev_smoothed_max_val(0.0f),
      sprGraph(&tft_ref),  // Sprite inicializálása a TFT referenciával
      spriteCreated(false) {
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

MiniAudioFft::~MiniAudioFft() {
    if (spriteCreated) {
        sprGraph.deleteSprite();
        spriteCreated = false;
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
    manageSpriteForMode(currentMode);  // Sprite előkészítése a kezdeti módhoz
    forceRedraw();                     // Ez gondoskodik a clearArea-ról és a drawModeIndicator-ról
}

/**
 * @brief Kezeli a sprite létrehozását/törlését a megadott módhoz.
 * @param modeToPrepareFor Az a mód, amelyhez a sprite-ot elő kell készíteni.
 */
void MiniAudioFft::manageSpriteForMode(DisplayMode modeToPrepareFor) {
    if (spriteCreated) {  // Ha létezik sprite egy korábbi módból
        sprGraph.deleteSprite();
        spriteCreated = false;
    }
    // Sprite használata Waterfall, TuningAid és Envelope módokhoz
    if (modeToPrepareFor == DisplayMode::Waterfall || modeToPrepareFor == DisplayMode::TuningAid || modeToPrepareFor == DisplayMode::Envelope) {
        int graphH = getGraphHeight();
        if (width > 0 && graphH > 0) {
            sprGraph.setColorDepth(16);  // Vagy 8, ha palettát használsz
            sprGraph.createSprite(width, graphH);
            sprGraph.fillSprite(TFT_BLACK);  // Kezdeti törlés
            spriteCreated = true;
        } else {
            spriteCreated = false;  // Nem lehetett létrehozni
            DEBUG("MiniAudioFft: Sprite creation failed for mode %d (w:%d, graphH:%d)\n", static_cast<int>(modeToPrepareFor), width, graphH);
        }
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
        envelope_prev_smoothed_max_val = 0.0f;  // Envelope simítási előzmény nullázása
    }
    highResOffset = 0;  // Magas felbontású spektrum eltolásának resetelése

    manageSpriteForMode(currentMode);  // Sprite előkészítése az új módhoz

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
    String gainText = "";
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

    // Erősítés állapotának hozzáadása, ha a mód nem "Off"
    if (currentMode != DisplayMode::Off) {
        if (activeFftGainConfigRef == -1.0f) {
            gainText = " (Off)";
        } else if (activeFftGainConfigRef == 0.0f) {
            gainText = " (Auto G)";
        } else if (activeFftGainConfigRef > 0.0f) {
            gainText = " (Man G)";
        }
    }

    // Módkijelző területének explicit törlése a szövegkirajzolás előtt
    tft.fillRect(posX, indicatorYstart, width, indicatorH, TFT_BLACK);
    // Szöveg kirajzolása a komponens aljára, középre.
    // Az Y koordináta a szöveg alapvonala lesz. A `posY + height - 2` a teljes komponensmagasság aljára igazít.
    tft.drawString(modeText + gainText, posX + width / 2, posY + height - 2);
}

/**
 * @brief Elvégzi az FFT mintavételezést és a szükséges számításokat.
 *
 * Beolvassa az analóg audio bemenetet, alkalmazza az erősítést (manuális vagy auto),
 * alkalmazza az ablakozást, elvégzi az FFT-t, majd a komplex eredményből magnitúdókat számol.
 * Opcionálisan mintákat gyűjt az oszcilloszkóp módhoz.
 * Az alacsony frekvenciás komponenseket csillapítja a jobb vizuális megjelenítés érdekében.
 *
 * @param collectOsciSamples Igaz, ha az oszcilloszkóp számára is kell mintákat gyűjteni.
 */
void MiniAudioFft::performFFT(bool collectOsciSamples) {
    using namespace MiniAudioFftConstants;
    int osci_sample_idx = 0;
    double max_abs_sample_for_auto_gain = 0.0;

    // Ha az FFT ki van kapcsolva (-1.0f), akkor nem csinálunk semmit a mintavételezéssel és erősítéssel.
    // Ezt a hívó loop()-nak kellene kezelnie, de biztonsági ellenőrzésként itt is lehet.
    if (activeFftGainConfigRef == -1.0f) {
        return;  // Vagy nullázzuk a vReal-t és RvReal-t
    }

    // 1. Mintavételezés és középre igazítás, opcionális oszcilloszkóp mintagyűjtés
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
        vReal[i] = averaged_sample - 2048.0;  // Középre igazítás (feltételezve, hogy 2048 a nulla szint)
        vImag[i] = 0.0;

        if (activeFftGainConfigRef == 0.0f) {  // Auto Gain
            if (std::abs(vReal[i]) > max_abs_sample_for_auto_gain) {
                max_abs_sample_for_auto_gain = std::abs(vReal[i]);
            }
        }
    }

    // 2. Erősítés alkalmazása (manuális vagy automatikus)
    if (activeFftGainConfigRef > 0.0f) {  // Manual Gain
        for (int i = 0; i < FFT_SAMPLES; i++) {
            vReal[i] *= activeFftGainConfigRef;  // A config érték a manuális faktor
        }
    } else if (activeFftGainConfigRef == 0.0f) {  // Auto Gain (normalizálás)

        if (max_abs_sample_for_auto_gain > 0.001) {  // Elkerüljük a nullával való osztást és a túl kicsi jelek extrém erősítését
            float auto_gain_factor = FFT_AUTO_GAIN_TARGET_PEAK / max_abs_sample_for_auto_gain;
            auto_gain_factor = constrain(auto_gain_factor, FFT_AUTO_GAIN_MIN_FACTOR, FFT_AUTO_GAIN_MAX_FACTOR);
            for (int i = 0; i < FFT_SAMPLES; i++) {
                vReal[i] *= auto_gain_factor;
            }
        }
        // Ha max_abs_sample_for_auto_gain nagyon kicsi vagy nulla, nem alkalmazunk erősítést
    }  // Ha -1.0f (Disabled), akkor ide el sem jutunk, vagy a vReal érintetlen marad

    // 3. Ablakozás, FFT számítás, magnitúdó
    FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);  // Az eredmény a vReal-be kerül

    // Magnitúdók átmásolása az RvReal tömbbe
    for (int i = 0; i < FFT_SAMPLES; ++i) {
        RvReal[i] = vReal[i];
    }

    // 4. Alacsony frekvenciák csillapítása az RvReal tömbben
    const float binWidthHz = static_cast<float>(SAMPLING_FREQUENCY) / FFT_SAMPLES;
    const int attenuation_cutoff_bin = static_cast<int>(LOW_FREQ_ATTENUATION_THRESHOLD_HZ / binWidthHz);

    for (int i = 0; i < (FFT_SAMPLES / 2); ++i) {  // Csak a releváns (nem tükrözött) frekvencia bin-eken iterálunk
        if (i < attenuation_cutoff_bin) {
            RvReal[i] /= LOW_FREQ_ATTENUATION_FACTOR;
        }
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
        // A "MUTED" felirat már kint van a forceRedraw miatt, vagy ha a némítás
        // korábban bekapcsolt és azóta nem volt teljes újrarajzolás.
        return;
    }

    // Ha némítva van (és nem most változott az állapota, mert azt már a forceRedraw kezelte)
    if (rtv::muteStat) {
        return;  // A "MUTED" felirat már kint van a forceRedraw miatt
    }

    // Ellenőrizzük az FFT konfigurációt (Disabled, Auto, Manual)
    // Az activeFftGainConfigRef már a megfelelő AM/FM configra mutat.
    // --- ÚJ: "Off" állapot kijelzése a grafikon közepén, ha a módjelző nem látszik ---
    if (currentMode == DisplayMode::Off && !isIndicatorCurrentlyVisible) {
        // A grafikon területét a forceRedraw() már törölte, amikor az isIndicatorCurrentlyVisible false-ra váltott.
        // Vagy egy előző mód rajzolása törölte, mielőtt Off-ra váltottunk.
        drawOffStatusInCenter();
        return;  // Nincs más teendő Off módban, ha a jelző nem látszik.
    }
    // --- ÚJ VÉGE ---
    if (activeFftGainConfigRef == -1.0f) {  // FFT Disabled
        // Ha a módkijelző látható, azt még ki kell rajzolni, de utána return
        // A drawModeIndicator az "Off" szöveget fogja mutatni, ha currentMode == DisplayMode::Off
        // De itt az FFT-t kapcsoltuk ki, a currentMode (pl. SpectrumLowRes) maradhat.
        // A drawModeIndicator-t módosítani kellene, hogy az FFT kikapcsolt állapotát is jelezze,
        // vagy a currentMode-ot DisplayMode::Off-ra állítani, amikor az FFT-t kikapcsoljuk.
        // Egyszerűbb, ha a currentMode-ot állítjuk Off-ra.
        // Ezt a SetupDisplay-nek kellene kezelnie.
        // Jelenleg, ha az FFT config -1.0f, de a currentMode nem Off, akkor is rajzolna.
        return;  // Az "Off" felirat már kint van
    }

    // --- Csak akkor jutunk ide, ha nincs némítás, a mód nem "Off", és nem volt állapotváltozás miatti forceRedraw ---

    // FFT mintavételezés és számítás
    unsigned long fft_start_time = micros();
    if (currentMode != DisplayMode::Off) {  // Csak akkor végezzük el, ha a kijelzési mód nem Off
        performFFT(currentMode == DisplayMode::Oscilloscope);
    }
    unsigned long fft_duration = micros() - fft_start_time;

    // Grafikonok kirajzolása a nekik szánt (csökkentett) területre
    // Ezek a függvények a `posY`-tól `posY + getGraphHeight() - 1`-ig rajzolnak.
    // A `getGraphHeight()` mindig a grafikon magasságát adja vissza, a módkijelző sávja nélkül.
    unsigned long draw_start_time = micros();
    switch (currentMode) {
        // Csak akkor rajzolunk, ha a currentMode nem Off.
        // Az FFT letiltását (-1.0f) a loop elején már kezeltük.
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
        case DisplayMode::Off:  // Nem csinálunk semmit
        default:
            break;
    }
    unsigned long draw_duration = micros() - draw_start_time;

    // A drawModeIndicator() metódust NEM hívjuk itt minden ciklusban,
    // csak akkor, ha a mód/láthatóság ténylegesen megváltozik (cycleMode, forceRedraw).

    // Segédfüggvény a DisplayMode szöveges nevének lekérdezéséhez
    static auto getModeNameString = [](MiniAudioFft::DisplayMode mode) -> const char* {
        switch (mode) {
            case MiniAudioFft::DisplayMode::Off:
                return "Off";
            case MiniAudioFft::DisplayMode::SpectrumLowRes:
                return "LowRes";
            case MiniAudioFft::DisplayMode::SpectrumHighRes:
                return "HighRes";
            case MiniAudioFft::DisplayMode::Oscilloscope:
                return "Scope";
            case MiniAudioFft::DisplayMode::Waterfall:
                return "Waterfall";
            case MiniAudioFft::DisplayMode::Envelope:
                return "Envelope";
            case MiniAudioFft::DisplayMode::TuningAid:
                return "TuneAid";
            default:
                return "Unknown";
        }
    };

    // Belső időmérés kiíratása (opcionális, csak debugoláshoz)
    static unsigned long lastInternalTimingPrint = 0;
    static unsigned long max_fft_loop_duration = 0;
    static unsigned long max_draw_loop_duration = 0;
    if (fft_duration > max_fft_loop_duration) max_fft_loop_duration = fft_duration;
    if (draw_duration > max_draw_loop_duration) max_draw_loop_duration = draw_duration;
    if (millis() - lastInternalTimingPrint >= 1000) {
        DEBUG("MiniAudioFft Internal (max 1s) - FFT: %lu us, Draw (%s): %lu us\n", max_fft_loop_duration, getModeNameString(currentMode), max_draw_loop_duration);
        lastInternalTimingPrint = millis();
        max_fft_loop_duration = 0;
        max_draw_loop_duration = 0;
    }
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
            lastTouchProcessTime = millis();  // Időbélyeg frissítése
            cycleMode();                      // Ez beállítja az isIndicatorCurrentlyVisible-t, a timert, és hívja a forceRedraw-t
            return true;                      // Kezeltük az érintést
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
        // Ha a currentMode Off, akkor az FFT configtól függetlenül "Off" jelenik meg.
        drawModeIndicator();  // "Off" kirajzolása (ha látható)
    } else {
        // Ha a currentMode nem Off, de az FFT config Disabled (-1.0f),
        // akkor a performFFT nem csinál semmit, és a rajzoló függvények üres adatot kapnak.
        // Ideális esetben a currentMode-ot Off-ra kellene állítani, ha az FFT config -1.0f.
        // Ezt a SetupDisplay-nek kellene kezelnie.
        // Jelenlegi logika: ha az FFT config -1.0f, a performFFT nem tölti fel RvReal-t,
        // így a rajzoló függvények nem rajzolnak semmit (vagy feketét).
        // A drawModeIndicator továbbra is a currentMode-ot írja ki.

        if (activeFftGainConfigRef != -1.0f) {  // Csak akkor végezzük el az FFT-t és a rajzolást, ha nincs letiltva
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
                default:
                    break;  // DisplayMode::Off itt nem fordulhat elő az else ág miatt
            }
        } else {
            // Ha az FFT le van tiltva (-1.0f), de a currentMode nem Off,
            // akkor a grafikon területét törölhetnénk, vagy hagyhatjuk az utolsó állapotot.
            // A clearArea már törölte a hátteret.
            // A performFFT nem futott, RvReal üres vagy régi.
            // A rajzoló függvények nem fognak semmit rajzolni.
        }
        drawModeIndicator();  // És a módkijelző kirajzolása (ha látható)
    }
}

/**
 * @brief Kirajzolja az "Off" státuszt a grafikon területének közepére,
 * ha a komponens Off módban van és a normál módkijelző nem látható.
 */
void MiniAudioFft::drawOffStatusInCenter() {
    int graphH = getGraphHeight();
    if (width < 10 || graphH < 10) return;  // Nincs elég hely

    // A grafikon területének háttere már fekete kell, hogy legyen
    // a korábbi clearArea() vagy egy másik mód rajzolása miatt.
    // Itt csak a szöveget rajzoljuk ki.

    tft.setTextDatum(MC_DATUM);      // Middle-Center
    tft.setTextColor(TFT_DARKGREY);  // Sötétszürke szín
    tft.setFreeFont();               // Alapértelmezett vagy választott font
    tft.setTextSize(2);              // Megfelelő méret

    int centerX = posX + width / 2;
    int centerY = posY + graphH / 2;
    tft.drawString("Off", centerX, centerY);
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

    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {    // FFT bin indexek 2-től indulnak a DC és Nyquist komponensek elhagyása miatt
        if (RvReal[i] > (AMPLITUDE_SCALE / 10.0)) {  // Csak akkor rajzolunk, ha a jel erőssége meghalad egy küszöböt
            byte band_idx = getBandVal(i);
            if (band_idx < bands_to_process) {
                // Most RvReal[i]-t (double) adjuk át, nem (int)RvReal[i]-t
                drawSpectrumBar(band_idx, RvReal[i], current_draw_x_on_screen, actual_low_res_peak_max_height);
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
    if (fft_bin_index < 2) return 0;                                  // Az első két bin-t (DC, Nyquist/2) általában nem használjuk a spektrum sávokhoz
    int effective_bins = MiniAudioFftConstants::FFT_SAMPLES / 2 - 2;  // Hasznos bin-ek száma (a 0. és 1. nélkül)
    if (effective_bins <= 0) return 0;
    // Lineáris leképezés a hasznos bin-ekről a LOW_RES_BANDS sávokra
    return constrain((fft_bin_index - 2) * MiniAudioFftConstants::LOW_RES_BANDS / effective_bins, 0, MiniAudioFftConstants::LOW_RES_BANDS - 1);
}

/**
 * @brief Kirajzol egyetlen oszlopot/sávot (bar-t) az alacsony felbontású spektrumhoz.
 * @param band_idx A frekvenciasáv indexe, amelyhez az oszlop tartozik.
 * @param magnitude A sáv magnitúdója (most double).
 * @param actual_start_x_on_screen A spektrum rajzolásának kezdő X koordinátája a képernyőn.
 * @param peak_max_height_for_mode A sáv maximális magassága az adott módban.
 */
void MiniAudioFft::drawSpectrumBar(int band_idx, double magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode) {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (graphH <= 0) return;

    // A magnitúdó (double) osztása a float AMPLITUDE_SCALE-lel, majd int-re kasztolás
    int dsize = static_cast<int>(magnitude / AMPLITUDE_SCALE);
    dsize = constrain(dsize, 0, peak_max_height_for_mode);  // peak_max_height_for_mode már graphH-1

    constexpr int bar_width_pixels = 3;
    constexpr int bar_gap_pixels = 2;
    constexpr int bar_total_width_pixels = bar_width_pixels + bar_gap_pixels;
    int xPos = actual_start_x_on_screen + bar_total_width_pixels * band_idx;

    if (xPos + bar_width_pixels > posX + width || xPos < posX) return;  // Ne rajzoljunk a komponensen kívülre

    if (dsize > 0) {
        int y_start_bar = posY + graphH - dsize;  // Y a grafikon területén belül
        int bar_h_visual = dsize;                 // A ténylegesen kirajzolandó magasság
        // Biztosítjuk, hogy a sáv a grafikon területén belül maradjon
        if (y_start_bar < posY) {
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

    int actual_high_res_bins_to_display = width;  // Minden pixel egy FFT bin-t (vagy interpolált értéket) képvisel

    for (int i = 0; i < actual_high_res_bins_to_display; i++) {
        // FFT bin index leképezése a képernyő pixelére.
        // A 2. bin-től indulunk (elhagyva a DC-t és az első komponenst).
        // Az FFT_SAMPLES/2 - 2 a használható bin-ek száma (a 0. és 1. nélkül).
        int fft_bin_index = 2 + (i * (FFT_SAMPLES / 2 - 2)) / std::max(1, (actual_high_res_bins_to_display - 1));
        // Biztosítjuk, hogy az index a határokon belül maradjon
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        int screen_x = posX + i;
        tft.drawFastVLine(screen_x, posY, graphH, TFT_BLACK);  // Oszlop törlése a grafikon területén

        // A RvReal[fft_bin_index] (double) osztása AMPLITUDE_SCALE-lel (float), majd int-re kasztolás
        int scaled_magnitude = static_cast<int>(RvReal[fft_bin_index] / AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, graphH - 1);  // Korlátozás a grafikon magasságára

        if (scaled_magnitude > 0) {
            int y_bar_start = posY + graphH - 1 - scaled_magnitude;         // Oszlop teteje
            int bar_actual_height = (posY + graphH - 1) - y_bar_start + 1;  // Oszlop magassága
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
    // Kiszámoljuk az aktuális oszcilloszkóp minták átlagát (DC komponens)
    double sum_samples = 0;
    // A MAX_INTERNAL_WIDTH konstans a headerben van definiálva
    for (int k = 0; k < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++k) {
        sum_samples += osciSamples[k];
    }
    double dc_offset_correction = MiniAudioFftConstants::MAX_INTERNAL_WIDTH > 0 ? sum_samples / MiniAudioFftConstants::MAX_INTERNAL_WIDTH : 2048.0;

    int actual_osci_samples_to_draw = width;
    // --- Érzékenységi faktor meghatározása ---
    // Az oszcilloszkóp mindig a manuális érzékenységi faktort használja,
    // mivel az osciSamples nyers ADC értékeket tartalmaz, függetlenül az FFT erősítési módjától.
    float current_sensitivity_factor = OSCI_SENSITIVITY_FACTOR;
    // --- Érzékenységi faktor vége ---

    // Grafikon területének törlése (csak a grafikon sávja)
    tft.fillRect(posX, posY, width, graphH, TFT_BLACK);

    int prev_x = -1, prev_y = -1;

    for (int i = 0; i < actual_osci_samples_to_draw; i++) {
        int num_available_samples = sizeof(osciSamples) / sizeof(osciSamples[0]);
        if (num_available_samples == 0) continue;  // Ha nincs minta, ne csináljunk semmit

        // Minták leképezése a rendelkezésre álló MAX_INTERNAL_WIDTH-ből a tényleges 'width'-re
        int sample_idx = (i * (num_available_samples - 1)) / std::max(1, (actual_osci_samples_to_draw - 1));
        sample_idx = constrain(sample_idx, 0, num_available_samples - 1);

        int raw_sample = osciSamples[sample_idx];
        // ADC érték (0-4095) átalakítása a KISZÁMÍTOTT DC KÖZÉPPONTHOZ képest,
        // majd skálázás az OSCI_SENSITIVITY_FACTOR-ral és a grafikon magasságára
        double sample_deviation = (static_cast<double>(raw_sample) - dc_offset_correction);
        double gain_adjusted_deviation = sample_deviation * current_sensitivity_factor;
        // Skálázás a grafikon felére (mivel a jel a középvonal körül ingadozik)
        double scaled_y_deflection = gain_adjusted_deviation * (static_cast<double>(graphH) / 2.0 - 1.0) / 2048.0;  // A 2048.0 itt a maximális elméleti ADC eltérésre skáláz

        int y_pos = posY + graphH / 2 - static_cast<int>(round(scaled_y_deflection));
        y_pos = constrain(y_pos, posY, posY + graphH - 1);  // Korlátozás a grafikon területére
        int x_pos = posX + i;

        if (prev_x != -1) {
            tft.drawLine(prev_x, prev_y, x_pos, y_pos, TFT_GREEN);
        } else {
            tft.drawPixel(x_pos, y_pos, TFT_GREEN);  // Első pont kirajzolása
        }
        prev_x = x_pos;
        prev_y = y_pos;
    }
    // A drawModeIndicator-t a loop() vagy a forceRedraw() hívja, itt nem szükséges
}

/**
 * @brief Vízesés diagram kirajzolása.
 *
 * A `getGraphHeight()` által visszaadott magasságot használja a rajzoláshoz.
 * A `wabuf` feltöltése a teljes komponens magasság (`this->height`) alapján történik.
 * Sprite-ot használ a gyorsabb rajzoláshoz.
 */
void MiniAudioFft::drawWaterfall() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (!spriteCreated || width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated && (currentMode == DisplayMode::Waterfall || currentMode == DisplayMode::TuningAid)) {
            DEBUG("MiniAudioFft::drawWaterfall - Sprite not created for mode %d\n", static_cast<int>(currentMode));
        }
        return;
    }

    // 1. Adatok eltolása balra a `wabuf`-ban (ez továbbra is szükséges a `wabuf` frissítéséhez)
    for (int r = 0; r < height; ++r) {  // A teljes `this->height` magasságon iterálunk a `wabuf` miatt
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Új adatok betöltése a `wabuf` jobb szélére (a `wabuf` továbbra is `height` magas)
    for (int r = 0; r < height; ++r) {
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        constexpr float WATERFALL_INPUT_SCALE = 0.25f;
        wabuf[r][width - 1] = static_cast<int>(constrain(RvReal[fft_bin_index] * WATERFALL_INPUT_SCALE, 0.0, 255.0));
    }

    // 3. Sprite görgetése és új oszlop kirajzolása
    sprGraph.scroll(-1, 0);  // Tartalom görgetése 1 pixellel balra

    // Az új (jobb szélső) oszlop kirajzolása a sprite-ra
    // A sprite `graphH` magas, a `wabuf` `height` magas.
    for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {
        // `r_wabuf` (0..height-1) leképezése `y_on_sprite`-ra (0..graphH-1)
        // A vízesés "fentről lefelé" jelenik meg a képernyőn, de a `wabuf` sorai a frekvenciákat jelentik (alulról felfelé).
        // Tehát a `wabuf` r-edik sorát a sprite (graphH - 1 - r_scaled) pozíciójára kell rajzolni.
        int screen_y_relative_inverted = (r_wabuf * (graphH - 1)) / std::max(1, (height - 1));
        int y_on_sprite = (graphH - 1 - screen_y_relative_inverted);  // Y koordináta a sprite-on belül

        if (y_on_sprite >= 0 && y_on_sprite < graphH) {                                       // Biztosítjuk, hogy a sprite-on belül rajzolunk
            uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[r_wabuf][width - 1]);  // Az új oszlop adata
            sprGraph.drawPixel(width - 1, y_on_sprite, color);                                // Rajzolás a sprite jobb szélére
        }
    }

    // Sprite kirakása a képernyőre
    sprGraph.pushSprite(posX, posY);
}

/**
 * @brief Értéket konvertál egy színné a vízesés diagramhoz a definiált színpaletta alapján.
 * @param scaled_value A skálázott bemeneti érték (pl. gradient * wabuf_érték).
 * @return A megfelelő RGB565 színkód.
 */
uint16_t MiniAudioFft::valueToWaterfallColor(int scaled_value) {
    using namespace MiniAudioFftConstants;
    // Normalizálás 0.0 és 1.0 közé a MAX_WATERFALL_COLOR_INPUT_VALUE alapján
    float normalized = static_cast<float>(constrain(scaled_value, 0, MAX_WATERFALL_COLOR_INPUT_VALUE)) / static_cast<float>(MAX_WATERFALL_COLOR_INPUT_VALUE);
    byte color_size = sizeof(WATERFALL_COLORS) / sizeof(WATERFALL_COLORS[0]);
    int index = (int)(normalized * (color_size - 1));  // Index számítása a normalizált értékből
    index = constrain(index, 0, color_size - 1);       // Biztosítjuk, hogy az index a tömb határain belül maradjon
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
    if (!spriteCreated || width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated && currentMode == DisplayMode::Envelope) {
            DEBUG("MiniAudioFft::drawEnvelope - Sprite not created for Envelope mode.\n");
        }
        return;
    }

    sprGraph.fillSprite(TFT_BLACK);  // Sprite törlése minden rajzolás előtt

    // 1. Adatok eltolása balra
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    // 2. Új adatok betöltése
    // Az Envelope módhoz az RvReal értékeit használjuk, de egy saját erősítéssel.
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        int fft_bin_index = 2 + (r * (FFT_SAMPLES / 2 - 2)) / std::max(1, (height - 1));
        if (fft_bin_index >= FFT_SAMPLES / 2) fft_bin_index = FFT_SAMPLES / 2 - 1;
        if (fft_bin_index < 2) fft_bin_index = 2;

        // Az RvReal[fft_bin_index] már tartalmazza a csillapított értéket.
        // Alkalmazzuk az ENVELOPE_INPUT_GAIN-t.
        double gained_val = RvReal[fft_bin_index] * ENVELOPE_INPUT_GAIN;
        wabuf[r][width - 1] = static_cast<int>(constrain(gained_val, 0.0, 255.0));  // 0-255 közé korlátozzuk a wabuf számára
    }

    // 3. Burkológörbe kirajzolása
    const int half_graph_h = graphH / 2;
    // Az 'envelope_prev_smoothed_max_val' tagváltozót használjuk a simításhoz

    for (int c = 0; c < width; ++c) {
        int max_val_in_col = 0;
        bool column_has_signal = false;

        for (int r_wabuf = 0; r_wabuf < height; ++r_wabuf) {  // Teljes `this->height`
            if (wabuf[r_wabuf][c] > 0) column_has_signal = true;
            if (wabuf[r_wabuf][c] > max_val_in_col) {
                max_val_in_col = wabuf[r_wabuf][c];
            }
        }

        // A maximális amplitúdó simítása az oszlopban
        float current_col_max_amplitude = static_cast<float>(max_val_in_col);
        // A tagváltozót használjuk a simításhoz az oszlopok között
        envelope_prev_smoothed_max_val = ENVELOPE_SMOOTH_FACTOR * envelope_prev_smoothed_max_val + (1.0f - ENVELOPE_SMOOTH_FACTOR) * current_col_max_amplitude;

        // Az oszlop törlését a sprGraph.fillSprite(TFT_BLACK) már elvégezte.
        // Itt közvetlenül a sprite-ra rajzolunk.

        // Csak akkor rajzolunk, ha van jel vagy a simított érték számottevő
        if (column_has_signal || envelope_prev_smoothed_max_val > 0.5f) {
            // A simított amplitúdó skálázása a grafikon magasságára (0-255 -> 0-half_graph_h)
            // Az ENVELOPE_THICKNESS_SCALER-t itt nem használjuk közvetlenül,
            // a vastagságot a `y_offset_pixels` adja. Ha vastagabb görbét szeretnénk,
            // az `ENVELOPE_INPUT_GAIN`-t kell növelni, vagy a skálázást módosítani.
            float y_offset_float = (envelope_prev_smoothed_max_val / 255.0f) * (half_graph_h - 1.0f);
            int y_offset_pixels = static_cast<int>(round(y_offset_float));
            y_offset_pixels = std::min(y_offset_pixels, half_graph_h - 1);  // Biztosítjuk, hogy a határokon belül maradjon
            if (y_offset_pixels < 0) y_offset_pixels = 0;

            if (y_offset_pixels >= 0) {  // Ha van vastagság
                // Y koordináták a sprite-on belül (0-tól graphH-1-ig)
                int yCenter_on_sprite = half_graph_h;
                int yUpper_on_sprite = yCenter_on_sprite - y_offset_pixels;
                int yLower_on_sprite = yCenter_on_sprite + y_offset_pixels;

                yUpper_on_sprite = constrain(yUpper_on_sprite, 0, graphH - 1);
                yLower_on_sprite = constrain(yLower_on_sprite, 0, graphH - 1);

                if (yUpper_on_sprite <= yLower_on_sprite) {  // Biztosítjuk, hogy van mit rajzolni
                    // Kitöltött burkológörbe rajzolása
                    // Az X koordináta 'c' (0-tól width-1-ig)
                    sprGraph.drawFastVLine(c, yUpper_on_sprite, yLower_on_sprite - yUpper_on_sprite + 1, TFT_WHITE);
                }
            }
        }
    }
    sprGraph.pushSprite(posX, posY);  // Sprite kirakása a képernyőre
}

/**
 * @brief Hangolási segéd mód kirajzolása (szűkített vízesés célvonallal).
 *
 * A `getGraphHeight()` által visszaadott magasságot használja a rajzoláshoz.
 * A `wabuf` feltöltése a TUNING_AID_DISPLAY_MIN_FREQ_HZ és MAX_FREQ_HZ közötti FFT bin-ek alapján történik.
 * Sprite-ot használ a gyorsabb rajzoláshoz.
 */
void MiniAudioFft::drawTuningAid() {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (!spriteCreated || width == 0 || graphH <= 0 || wabuf.empty() || wabuf[0].empty()) {
        if (!spriteCreated && (currentMode == DisplayMode::Waterfall || currentMode == DisplayMode::TuningAid)) {
            DEBUG("MiniAudioFft::drawTuningAid - Sprite not created for mode %d\n", static_cast<int>(currentMode));
        }
        return;
    }

    // 1. Adatok eltolása "lefelé" a `wabuf`-ban (időbeli léptetés)
    // Csak a grafikon magasságáig (`graphH`) használjuk a `wabuf` sorait.
    // A wabuf mérete (height x width), de itt csak graphH sort használunk fel a vízeséshez.
    for (int r = graphH - 1; r > 0; --r) {  // Utolsó sortól a másodikig
        for (int c = 0; c < width; ++c) {   // Minden oszlop (frekvencia bin)
            wabuf[r][c] = wabuf[r - 1][c];
        }
    }

    // 2. Új adatok betöltése a `wabuf[0]` sorába (legfrissebb spektrum)
    //    a hangolási tartományból, a belső szélességnek megfelelően.
    const float binWidthHz_local = static_cast<float>(SAMPLING_FREQUENCY) / FFT_SAMPLES;  // Helyi, mert a globális nincs itt
    const int min_fft_bin_for_tuning_local = std::max(2, static_cast<int>(std::round(TUNING_AID_DISPLAY_MIN_FREQ_HZ / binWidthHz_local)));
    const int max_fft_bin_for_tuning_local = std::min(static_cast<int>(FFT_SAMPLES / 2 - 1), static_cast<int>(std::round(TUNING_AID_DISPLAY_MAX_FREQ_HZ / binWidthHz_local)));
    const int num_bins_in_tuning_range = std::max(1, max_fft_bin_for_tuning_local - min_fft_bin_for_tuning_local + 1);

    // Iterálás a belső frekvencia-oszlopokon
    for (int internal_x = 0; internal_x < TUNING_AID_INTERNAL_WIDTH; ++internal_x) {
        // Leképezés a belső oszlop indexről az FFT bin indexre
        float ratio_in_display_width = (TUNING_AID_INTERNAL_WIDTH == 1) ? 0.0f : (static_cast<float>(internal_x) / (TUNING_AID_INTERNAL_WIDTH - 1));
        int fft_bin_index = min_fft_bin_for_tuning_local + static_cast<int>(std::round(ratio_in_display_width * (num_bins_in_tuning_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_fft_bin_for_tuning_local, max_fft_bin_for_tuning_local);
        fft_bin_index = constrain(fft_bin_index, 2, static_cast<int>(FFT_SAMPLES / 2 - 1));

        // Az adat tárolása a wabuf első sorában, a belső indexen
        if (internal_x < width) {  // Biztosítjuk, hogy ne írjunk a wabuf szélességén kívülre
            wabuf[0][internal_x] = static_cast<int>(constrain(RvReal[fft_bin_index] * TUNING_AID_INPUT_SCALE, 0.0, 255.0));
        }
    }

    // 3. Sprite görgetése és új sor kirajzolása
    sprGraph.scroll(0, 1);  // Tartalom görgetése 1 pixellel lefelé

    // Az új (legfelső) sor kirajzolása a sprite-ra, középre igazítva
    int startX_on_sprite = (width - TUNING_AID_INTERNAL_WIDTH) / 2;
    int endX_of_data_on_sprite = startX_on_sprite + TUNING_AID_INTERNAL_WIDTH;

    // Bal oldali üres sáv a sprite tetején (y=0)
    if (startX_on_sprite > 0) {
        sprGraph.fillRect(0, 0, startX_on_sprite, 1, TFT_BLACK);
    }

    // A tényleges TUNING_AID_INTERNAL_WIDTH (50) pixelnyi adat kirajzolása a sprite tetejére (y=0)
    for (int internal_col_idx = 0; internal_col_idx < TUNING_AID_INTERNAL_WIDTH; ++internal_col_idx) {
        int current_sprite_x = startX_on_sprite + internal_col_idx;
        // internal_col_idx (0-49) az index a wabuf[0]-hoz
        uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[0][internal_col_idx]);
        sprGraph.drawPixel(current_sprite_x, 0, color);
    }

    // Jobb oldali üres sáv a sprite tetején (y=0)
    if (endX_of_data_on_sprite < width) {
        sprGraph.fillRect(endX_of_data_on_sprite, 0, width - endX_of_data_on_sprite, 1, TFT_BLACK);
    }

    // 4. Célfrekvencia vonalának kirajzolása a sprite-ra
    float min_freq_displayed_actual = static_cast<float>(min_fft_bin_for_tuning_local) * binWidthHz_local;
    float max_freq_displayed_actual = static_cast<float>(max_fft_bin_for_tuning_local) * binWidthHz_local;
    float displayed_span_hz = max_freq_displayed_actual - min_freq_displayed_actual;

    if (displayed_span_hz > 0 && TUNING_AID_TARGET_FREQ_HZ >= min_freq_displayed_actual && TUNING_AID_TARGET_FREQ_HZ <= max_freq_displayed_actual) {
        float ratio = (TUNING_AID_TARGET_FREQ_HZ - min_freq_displayed_actual) / displayed_span_hz;
        // A célfrekvencia pozícióját a BELSŐ TUNING_AID_INTERNAL_WIDTH (50) oszlophoz képest számoljuk ki
        int internal_line_x = static_cast<int>(std::round(ratio * (TUNING_AID_INTERNAL_WIDTH - 1)));
        internal_line_x = constrain(internal_line_x, 0, TUNING_AID_INTERNAL_WIDTH - 1); // Biztosítjuk, hogy a belső szélességen belül maradjon

        // Ezt a belső pozíciót ültetjük át a sprite-ra, figyelembe véve a középre igazítást
        // A startX_on_sprite már ki lett számolva feljebb
        int line_x_on_sprite = startX_on_sprite + internal_line_x;

        // Biztosítjuk, hogy a vonal a sprite-on belül legyen (bár a startX_on_sprite + internal_line_x elvileg már jó kell legyen)
        line_x_on_sprite = constrain(line_x_on_sprite, 0, width - 1);

        sprGraph.drawFastVLine(line_x_on_sprite, 0, graphH, TUNING_AID_TARGET_LINE_COLOR);
        if (line_x_on_sprite + 1 < width) { // Két pixel vastag vonal
            sprGraph.drawFastVLine(line_x_on_sprite + 1, 0, graphH, TUNING_AID_TARGET_LINE_COLOR);
        }
    }

    // Sprite kirakása a képernyőre
    sprGraph.pushSprite(posX, posY);
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
