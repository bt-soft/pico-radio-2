#include "MiniAudioFft.h"

#include <cmath>  // std::round, std::max, std::min, std::abs

#include "Config.h"  // Szükséges a config.data eléréséhez
#include "rtVars.h"  // rtv::muteStat eléréséhez

// Konstans a módkijelző láthatósági idejéhez (ms)
constexpr uint32_t MODE_INDICATOR_TIMEOUT_MS = 20000;  // 20 másodperc

#define NUM_BINS(maxX, minX) std::max(1, maxX - minX + 1)

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
 * @param configuredMaxDisplayAudioFreq Az adott képernyőmódhoz (AM/FM) konfigurált maximális megjelenítendő audio frekvencia.
 * @param configModeField Referencia a Config_t megfelelő uint8_t mezőjére, ahova a módot menteni kell.
 * @param fftGainConfigRef Referencia a Config_t megfelelő float mezőjére az FFT erősítés konfigurációjához.
 * @param targetAudioProcessorSamplingRate Az AudioProcessor számára beállítandó cél mintavételezési frekvencia.
 */
MiniAudioFft::MiniAudioFft(TFT_eSPI& tft, int x, int y, int w, int h, float configuredMaxDisplayAudioFreq, uint8_t& configDisplayModeFieldRef, float& fftGainConfigRef)
    : tft(tft),
      posX(x),
      posY(y),
      width(w),
      height(h),
      prevMuteState(rtv::muteStat),                                           // Némítás előző állapotának inicializálása
      modeIndicatorShowUntil(0),                                              // Kezdetben nem látható (setInitialMode állítja)
      isIndicatorCurrentlyVisible(true),                                      // Kezdetben látható (setInitialMode állítja)
      lastTouchProcessTime(0),                                                // Debounce időzítő nullázása
      currentConfiguredMaxDisplayAudioFreqHz(configuredMaxDisplayAudioFreq),  // Maximális frekvencia elmentése
      configModeFieldRef(configDisplayModeFieldRef),                          // Referencia elmentése a kijelzési módhoz
      activeFftGainConfigRef(fftGainConfigRef),                               // Referencia elmentése az erősítés konfigurációhoz
      highResOffset(0),
      envelope_prev_smoothed_max_val(0.0f),
      currentTuningAidMinFreqHz_(0.0f),                   // Inicializálás
      currentTuningAidMaxFreqHz_(0.0f),                   // Inicializálás
      indicatorFontHeight_(0),                            // Inicializálás
      currentTuningAidType_(TuningAidType::OFF_DECODER),  // Alapértelmezetten OFF_DECODER
      pAudioProcessor(nullptr),                           // Inicializáljuk nullptr-rel
      sprGraph(&tft),                                     // Sprite inicializálása a TFT referenciával
      spriteCreated(false) {

    // A mintavételezési frekvencia kétszerese, mert a nyers adatokat kétszer kell feldolgozni
    pAudioProcessor = new AudioProcessor(activeFftGainConfigRef, AUDIO_INPUT_PIN, configuredMaxDisplayAudioFreq * 2.0f, AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // A `wabuf` (vízesés és burkológörbe buffer) inicializálása a komponens tényleges méreteivel.
    // Biztosítjuk, hogy a magasság és szélesség pozitív legyen az átméretezés előtt.
    if (this->height > 0 && this->width > 0) {
        wabuf.resize(this->height, std::vector<uint8_t>(this->width, 0));
    } else {
        DEBUG("MiniAudioFft: Invalid dimensions w=%d, h=%d\n", this->width, this->height);
    }

    // Pufferek nullázása
    memset(Rpeak, 0, sizeof(Rpeak));

    // A módkijelző font magasságának kiszámítása és tárolása
    // Ugyanazokat a beállításokat használjuk, mint a drawModeIndicator-ban
    tft.setFreeFont();
    tft.setTextSize(1);
    indicatorFontHeight_ = tft.fontHeight();
}

/**
 *
 */
MiniAudioFft::~MiniAudioFft() {
    if (spriteCreated) {
        sprGraph.deleteSprite();
    }
    if (pAudioProcessor) {
        delete pAudioProcessor;
        pAudioProcessor = nullptr;
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

    if (currentMode == DisplayMode::TuningAid) {
        // Az alapértelmezett currentTuningAidType_ CW_TUNING.
        // Hívjuk meg a setTuningAidType-ot, hogy a frekvenciahatárok beállítódjanak.
        setTuningAidType(currentTuningAidType_);
    }

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
    modeValue++;  // Lépés a következő módra

    // Ellenőrizzük, hogy FM módban vagyunk-e (a konfigurált maximális megjelenítendő frekvencia alapján)
    // és hogy a következő mód a TuningAid lenne-e. Ha igen, akkor átugorjuk a TuningAid módot.
    if (currentConfiguredMaxDisplayAudioFreqHz == MiniAudioFftConstants::MAX_DISPLAY_AUDIO_FREQ_FM_HZ && static_cast<DisplayMode>(modeValue) == DisplayMode::TuningAid) {
        modeValue++;  // Ugrás a TuningAid utáni módra
    }

    // Ha túlléptünk az utolsó érvényes módon (TuningAid), akkor visszaugrunk az Off-ra.
    if (modeValue > static_cast<uint8_t>(DisplayMode::TuningAid)) {
        modeValue = static_cast<uint8_t>(DisplayMode::Off);
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
    return indicatorFontHeight_ + 2;  // +2 pixel margó
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
        return;
    }

    int indicatorH = getIndicatorAreaHeight();
    int indicatorYstart = getIndicatorAreaY();  // A módkijelző sávjának teteje
    indicatorYstart += 2;                       // x pixellel lejjebb

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
            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                modeText = "Tune CW";
            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                modeText = "Tune RTTY";
            } else {
                modeText = "Tune Off";  // OFF_DECODER state
            }
            break;
        default:
            modeText = "Unknown";
            break;
    }

    // Erősítés állapotának hozzáadása, ha a mód nem "Off"
    if (currentMode != DisplayMode::Off) {
        if (activeFftGainConfigRef == -1.0f) {
            gainText = " (Off)";
        } else if (activeFftGainConfigRef == 0.0f) {
            gainText = " (Auto G)";
        } else if (activeFftGainConfigRef > 0.0f) {
            gainText = " (Manual G)";
        }
    }

    // Módkijelző területének explicit törlése a szövegkirajzolás előtt
    tft.fillRect(posX, indicatorYstart, width, indicatorH, TFT_BLACK);

    // Szöveg kirajzolása a komponens aljára, középre.
    // Az Y koordináta a szöveg alapvonala lesz.
    tft.drawString(modeText + gainText, posX + width / 2, posY + height);
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

    // Csak akkor végezzük el, ha a kijelzési mód nem Off
    if (currentMode != DisplayMode::Off) {

        // Ellenőrizzük, hogy a pAudioProcessor létezik-e
        if (pAudioProcessor) {
            pAudioProcessor->process(currentMode == DisplayMode::Oscilloscope);
        }
    }

    // Grafikonok kirajzolása a nekik szánt (csökkentett) területre
    // Ezek a függvények a `posY`-tól `posY + getGraphHeight() - 1`-ig rajzolnak.
    // A `getGraphHeight()` mindig a grafikon magasságát adja vissza, a módkijelző sávja nélkül.
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
}

// Segédfüggvény: frekvencia skála címkék rajzolása
void MiniAudioFft::drawFrequencyScaleLabels(float min_freq, float max_freq, int x0, int y, int w, bool show) {
    if (!show) return;
    int num_labels = 5;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    for (int i = 0; i < num_labels; ++i) {
        float frac = (float)i / (num_labels - 1);
        float freq = min_freq + frac * (max_freq - min_freq);
        int x;
        if (i == 0) {
            tft.setTextDatum(TL_DATUM);
            x = x0;
        } else if (i == num_labels - 1) {
            tft.setTextDatum(TR_DATUM);
            x = x0 + w - 1;
        } else {
            tft.setTextDatum(TC_DATUM);
            x = x0 + (int)(frac * (w - 1));
        }
        char buf[12];
        if (freq >= 1000.0f)
            snprintf(buf, sizeof(buf), "%dk", (int)(freq / 1000.0f));
        else
            snprintf(buf, sizeof(buf), "%d", (int)freq);
        tft.drawString(buf, x, y, 1);
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
 * Letörli a komponenst az effektív magasságának megfelelően, és újrarajzolja a tartalmat.
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
        // Jelenlegi logika: ha az FFT config -1.0f, de a currentMode nem Off, akkor is rajzolna.
        // A drawModeIndicator továbbra is a currentMode-ot írja ki.

        if (activeFftGainConfigRef != -1.0f) {  // Csak akkor végezzük el az FFT-t és a rajzolást, ha nincs letiltva

            // TuningAid mód használja a nagy felbontású processort, minden más a standard processort
            if (currentMode == DisplayMode::TuningAid) {

                // TuningAid mód: nagy felbontású FFT feldolgozás - Átkapcsolunk ha nem abban vagyunk
                if (pAudioProcessor && pAudioProcessor->getFftSize() != AudioProcessorConstants::HIGH_RES_FFT_SAMPLES) {
                    pAudioProcessor->setFftSize(AudioProcessorConstants::HIGH_RES_FFT_SAMPLES);
                }

            } else {
                // Standard módok: normál FFT feldolgozás - Átkapcsolunk ha nem abban vagyunk
                if (pAudioProcessor && pAudioProcessor->getFftSize() != AudioProcessorConstants::DEFAULT_FFT_SAMPLES) {
                    pAudioProcessor->setFftSize(AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
                }

                if (pAudioProcessor) {
                    pAudioProcessor->process(currentMode == DisplayMode::Oscilloscope);
                }
            }

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
 * ha a komponens Off módban van és a normál módkijelző nem látszik.
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
    constexpr int bar_gap_pixels = 1;
    int bands_to_display_on_screen = LOW_RES_BANDS;
    if (width < (bands_to_display_on_screen + (bands_to_display_on_screen - 1) * bar_gap_pixels)) {
        bands_to_display_on_screen = (width + bar_gap_pixels) / (1 + bar_gap_pixels);
    }
    if (bands_to_display_on_screen <= 0) bands_to_display_on_screen = 1;
    int dynamic_bar_width_pixels = 1;
    if (bands_to_display_on_screen > 0) {
        dynamic_bar_width_pixels = (width - (std::max(0, bands_to_display_on_screen - 1) * bar_gap_pixels)) / bands_to_display_on_screen;
    }
    if (dynamic_bar_width_pixels < 1) dynamic_bar_width_pixels = 1;
    int bar_total_width_pixels_dynamic = dynamic_bar_width_pixels + bar_gap_pixels;
    int total_drawn_width = (bands_to_display_on_screen * dynamic_bar_width_pixels) + (std::max(0, bands_to_display_on_screen - 1) * bar_gap_pixels);
    int x_offset = (width - total_drawn_width) / 2;
    int current_draw_x_on_screen = posX + x_offset;

    // Lassabb peak ereszkedés: csak minden 3. hívásnál csökkentjük
    static uint8_t peak_fall_counter = 0;
    peak_fall_counter = (peak_fall_counter + 1) % 3;

    for (int band_idx = 0; band_idx < bands_to_display_on_screen; band_idx++) {
        if (peak_fall_counter == 0) {
            if (Rpeak[band_idx] >= 1) Rpeak[band_idx] -= 1;
        }
    }

    float currentBinWidthHz = pAudioProcessor ? pAudioProcessor->getBinWidthHz() : (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    if (currentBinWidthHz == 0) currentBinWidthHz = (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    const uint16_t actualFftSize = pAudioProcessor ? pAudioProcessor->getFftSize() : AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    const int min_bin_idx_low_res = std::max(2, static_cast<int>(std::round(LOW_RES_SPECTRUM_MIN_FREQ_HZ / currentBinWidthHz)));
    const int max_bin_idx_low_res = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentConfiguredMaxDisplayAudioFreqHz / currentBinWidthHz)));
    const int num_bins_in_low_res_range = NUM_BINS(max_bin_idx_low_res, min_bin_idx_low_res);
    double band_magnitudes[LOW_RES_BANDS] = {0.0};
    if (!pAudioProcessor) {
        memset(band_magnitudes, 0, sizeof(band_magnitudes));
    }
    for (int i = min_bin_idx_low_res; i <= max_bin_idx_low_res; i++) {
        byte band_idx = getBandVal(i, min_bin_idx_low_res, num_bins_in_low_res_range);
        if (band_idx < LOW_RES_BANDS) {
            band_magnitudes[band_idx] = std::max(band_magnitudes[band_idx], pAudioProcessor ? pAudioProcessor->getMagnitudeData()[i] : 0.0);
        }
    }

    // Sávok kirajzolása
    for (int band_idx = 0; band_idx < bands_to_display_on_screen; band_idx++) {
        int x_pos_for_bar = current_draw_x_on_screen + bar_total_width_pixels_dynamic * band_idx;
        tft.fillRect(x_pos_for_bar, posY, dynamic_bar_width_pixels, graphH, TFT_BLACK);
        drawSpectrumBar(band_idx, band_magnitudes[band_idx], current_draw_x_on_screen, actual_low_res_peak_max_height, dynamic_bar_width_pixels);

        // Peak (csúcs) kirajzolása más színnel, csak ha nagyobb mint 3
        int peak = Rpeak[band_idx];
        if (peak > 3) {
            int y_peak = posY + graphH - peak;
            tft.fillRect(x_pos_for_bar, y_peak, dynamic_bar_width_pixels, 2, TFT_CYAN);  // 2 pixel magas sárga csík
        }
    }

    // Frekvencia skála címkék kirajzolása CSAK ha nincs módfelirat
    drawFrequencyScaleLabels(LOW_RES_SPECTRUM_MIN_FREQ_HZ, currentConfiguredMaxDisplayAudioFreqHz, posX, posY + graphH + 2, width, !isIndicatorCurrentlyVisible);
}

/**
 * @brief Meghatározza, hogy egy adott FFT bin melyik alacsony felbontású sávhoz tartozik.
 * @param fft_bin_index Az FFT bin indexe.
 * @param min_bin_low_res A LowRes spektrumhoz figyelembe vett legalacsonyabb FFT bin index.
 * @param num_bins_low_res_range A LowRes spektrumhoz figyelembe vett FFT bin-ek száma.
 * @return A sáv indexe (0-tól LOW_RES_BANDS-1-ig).
 */
uint8_t MiniAudioFft::getBandVal(int fft_bin_index, int min_bin_low_res, int num_bins_low_res_range) {
    if (fft_bin_index < min_bin_low_res || num_bins_low_res_range <= 0) {
        return 0;  // Vagy egy érvénytelen sáv index, ha szükséges
    }
    // Kiszámítjuk a relatív indexet a megadott tartományon belül
    int relative_bin_index = fft_bin_index - min_bin_low_res;
    if (relative_bin_index < 0) return 0;  // Elvileg nem fordulhat elő, ha fft_bin_index >= min_bin_low_res

    // Leképezzük a relatív bin indexet (0-tól num_bins_low_res_range-1-ig) a LOW_RES_BANDS sávokra
    return constrain(relative_bin_index * MiniAudioFftConstants::LOW_RES_BANDS / num_bins_low_res_range, 0, MiniAudioFftConstants::LOW_RES_BANDS - 1);
}

/**
 * @brief Kirajzol egyetlen oszlopot/sávot (bar-t) az alacsony felbontású spektrumhoz.
 * @param band_idx A frekvenciasáv indexe, amelyhez az oszlop tartozik.
 * @param magnitude A sáv magnitúdója (most double).
 * @param actual_start_x_on_screen A spektrum rajzolásának kezdő X koordinátája a képernyőn.
 * @param peak_max_height_for_mode A sáv maximális magassága az adott módban.
 * @param current_bar_width_pixels Az aktuális sávszélesség pixelekben.
 */
void MiniAudioFft::drawSpectrumBar(int band_idx, double magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode, int current_bar_width_pixels) {
    using namespace MiniAudioFftConstants;
    int graphH = getGraphHeight();
    if (graphH <= 0) return;

    int dsize = static_cast<int>(magnitude / AudioProcessorConstants::AMPLITUDE_SCALE);
    dsize = constrain(dsize, 0, peak_max_height_for_mode);
    constexpr int bar_gap_pixels = 1;
    int bar_total_width_pixels_dynamic = current_bar_width_pixels + bar_gap_pixels;
    int xPos = actual_start_x_on_screen + bar_total_width_pixels_dynamic * band_idx;

    if (xPos + current_bar_width_pixels > posX + width || xPos < posX) return;

    if (dsize > 0) {
        int y_start_bar = posY + graphH - dsize;
        int bar_h_visual = dsize;
        if (y_start_bar < posY) {
            bar_h_visual -= (posY - y_start_bar);
            y_start_bar = posY;
        }
        if (bar_h_visual > 0) {
            tft.fillRect(xPos, y_start_bar, current_bar_width_pixels, bar_h_visual, TFT_GREEN);  // Zöld bar
        }
    }

    if (dsize > Rpeak[band_idx]) {
        Rpeak[band_idx] = dsize;
    }
    // Ha a peak érték 0-ra csökkent, töröljük (biztosítjuk, hogy ne jelenjen meg)
    if (Rpeak[band_idx] < 1) {
        Rpeak[band_idx] = 0;
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

    tft.fillRect(posX, posY, width, graphH, TFT_BLACK);
    float currentBinWidthHz = pAudioProcessor ? pAudioProcessor->getBinWidthHz() : (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    if (currentBinWidthHz == 0) currentBinWidthHz = (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    const uint16_t actualFftSize = pAudioProcessor ? pAudioProcessor->getFftSize() : AudioProcessorConstants::DEFAULT_FFT_SAMPLES;
    const int min_bin_idx_for_display = std::max(2, static_cast<int>(std::round(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / currentBinWidthHz)));
    const int max_bin_idx_for_display = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentConfiguredMaxDisplayAudioFreqHz / currentBinWidthHz)));
    const int num_bins_in_display_range = NUM_BINS(max_bin_idx_for_display, min_bin_idx_for_display);
    if (!pAudioProcessor) return;

    for (int screen_pixel_x = 0; screen_pixel_x < width; ++screen_pixel_x) {
        int fft_bin_index;
        if (width == 1) {
            fft_bin_index = min_bin_idx_for_display;
        } else {
            float ratio = static_cast<float>(screen_pixel_x) / (width - 1);
            fft_bin_index = min_bin_idx_for_display + static_cast<int>(std::round(ratio * (num_bins_in_display_range - 1)));
        }
        fft_bin_index = constrain(fft_bin_index, 0, static_cast<int>(actualFftSize / 2 - 1));
        double magnitude = pAudioProcessor ? pAudioProcessor->getMagnitudeData()[fft_bin_index] : 0.0;
        int scaled_magnitude = static_cast<int>(magnitude / AudioProcessorConstants::AMPLITUDE_SCALE);
        scaled_magnitude = constrain(scaled_magnitude, 0, graphH - 1);
        int x_coord_on_tft = posX + screen_pixel_x;
        if (scaled_magnitude > 0) {
            int y_bar_start = posY + graphH - 1 - scaled_magnitude;
            int bar_actual_height = scaled_magnitude + 1;
            if (y_bar_start < posY) {
                bar_actual_height -= (posY - y_bar_start);
                y_bar_start = posY;
            }
            if (bar_actual_height > 0) {
                tft.drawFastVLine(x_coord_on_tft, y_bar_start, bar_actual_height, TFT_SKYBLUE);
            }
        }
    }
    // Frekvencia skála címkék kirajzolása CSAK ha nincs módfelirat
    drawFrequencyScaleLabels(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ, currentConfiguredMaxDisplayAudioFreqHz, posX, posY + graphH + 2, width,
                             !isIndicatorCurrentlyVisible);
}

/**
 * @brief Beállítja a hangolási segéd típusát (CW vagy RTTY).
 * @param type A beállítandó TuningAidType.
 */
void MiniAudioFft::setTuningAidType(TuningAidType type) {
    using namespace MiniAudioFftConstants;

    bool typeChanged = (currentTuningAidType_ != type);  // Check if type actually changed
    currentTuningAidType_ = type;

    if (currentMode == DisplayMode::TuningAid) {
        float oldMinFreq = currentTuningAidMinFreqHz_;  // Store old values to check if span changed
        float oldMaxFreq = currentTuningAidMaxFreqHz_;

        if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
            currentTuningAidMinFreqHz_ = config.data.cwReceiverOffsetHz - (CW_TUNING_AID_SPAN_HZ / 2.0f);
            currentTuningAidMaxFreqHz_ = config.data.cwReceiverOffsetHz + (CW_TUNING_AID_SPAN_HZ / 2.0f);

        } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
            float f_mark = config.data.rttyMarkFrequencyHz;
            float f_space = config.data.rttyMarkFrequencyHz - config.data.rttyShiftHz;  // Számított Space
            float f_low = std::min(f_space, f_mark);
            float f_high = std::max(f_space, f_mark);
            float delta_f = std::abs(f_high - f_low);  // Ensure positive delta
            if (delta_f < 1.0f) delta_f = 170.0f;      // Prevent too small or zero delta, default to common RTTY shift

            float total_span = 3.0f * delta_f;
            // Ensure total_span is reasonable
            total_span = std::max(total_span, CW_TUNING_AID_SPAN_HZ);  // At least as wide as CW
            total_span = std::min(total_span, 3200.0f);                // Cap at e.g. 3.2kHz (max RTTY freq for typical amateur use + margin)

            float center_freq = (f_high + f_low) / 2.0f;
            currentTuningAidMinFreqHz_ = center_freq - (total_span / 2.0f);
            currentTuningAidMaxFreqHz_ = center_freq + (total_span / 2.0f);

        } else {  // OFF_DECODER - use a default range for display but don't draw lines
            currentTuningAidMinFreqHz_ = config.data.cwReceiverOffsetHz - (CW_TUNING_AID_SPAN_HZ / 2.0f);
            currentTuningAidMaxFreqHz_ = config.data.cwReceiverOffsetHz + (CW_TUNING_AID_SPAN_HZ / 2.0f);
        }

        if (currentTuningAidMinFreqHz_ < 0) currentTuningAidMinFreqHz_ = 0;
        // Ensure min < max and there's a minimal span
        if (currentTuningAidMaxFreqHz_ <= currentTuningAidMinFreqHz_) {
            currentTuningAidMaxFreqHz_ = currentTuningAidMinFreqHz_ + std::max(CW_TUNING_AID_SPAN_HZ, 100.0f);
        }

        bool freqSpanChanged = (std::abs(currentTuningAidMinFreqHz_ - oldMinFreq) > 1.0f || std::abs(currentTuningAidMaxFreqHz_ - oldMaxFreq) > 1.0f);

        if (typeChanged || freqSpanChanged) {
            forceRedraw();  // Redraw if type or calculated frequency span changed
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
    double sum_samples = 0.0;
    const int* osciData = pAudioProcessor ? pAudioProcessor->getOscilloscopeData() : nullptr;
    // A MAX_INTERNAL_WIDTH konstans a headerben van definiálva
    for (int k = 0; k < MiniAudioFftConstants::MAX_INTERNAL_WIDTH; ++k) {
        sum_samples += (osciData ? osciData[k] : 2048);
    }
    double dc_offset_correction = (MiniAudioFftConstants::MAX_INTERNAL_WIDTH > 0 && osciData) ? sum_samples / MiniAudioFftConstants::MAX_INTERNAL_WIDTH : 2048.0;

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
        int num_available_samples = pAudioProcessor ? AudioProcessorConstants::MAX_INTERNAL_WIDTH : 0;
        if (num_available_samples == 0) continue;  // Ha nincs minta, ne csináljunk semmit

        // Minták leképezése a rendelkezésre álló MAX_INTERNAL_WIDTH-ből a tényleges 'width'-re
        int sample_idx = (i * (num_available_samples - 1)) / std::max(1, (actual_osci_samples_to_draw - 1));
        sample_idx = constrain(sample_idx, 0, num_available_samples - 1);

        int raw_sample = osciData ? osciData[sample_idx] : 2048;
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
    }  // 1. Adatok eltolása balra a `wabuf`-ban (ez továbbra is szükséges a `wabuf` frissítéséhez)
    for (int r = 0; r < height; ++r) {  // A teljes `this->height` magasságon iterálunk a `wabuf` miatt
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    float currentBinWidthHz = pAudioProcessor ? pAudioProcessor->getBinWidthHz() : (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    if (currentBinWidthHz == 0) currentBinWidthHz = (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // Get the actual FFT size from the processor or use default fallback
    const uint16_t actualFftSize = pAudioProcessor ? pAudioProcessor->getFftSize() : AudioProcessorConstants::DEFAULT_FFT_SAMPLES;

    const int min_bin_for_wf_env = std::max(2, static_cast<int>(std::round(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / currentBinWidthHz)));
    const int max_bin_for_wf_env = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentConfiguredMaxDisplayAudioFreqHz / currentBinWidthHz)));
    const int num_bins_in_wf_env_range = NUM_BINS(max_bin_for_wf_env, min_bin_for_wf_env);

    // 2. Új adatok betöltése a `wabuf` jobb szélére (a `wabuf` továbbra is `height` magas)
    for (int r = 0; r < height; ++r) {
        // 'r' (0 to height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_wf_env + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (height - 1)) * (num_bins_in_wf_env_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_wf_env, max_bin_for_wf_env);

        if (!pAudioProcessor) continue;
        constexpr float WATERFALL_INPUT_SCALE = 0.1f;  // Csökkentve, hogy ne legyen túl fehér auto gain mellett
        wabuf[r][width - 1] = static_cast<uint8_t>(constrain((pAudioProcessor ? pAudioProcessor->getMagnitudeData()[fft_bin_index] : 0.0) * WATERFALL_INPUT_SCALE, 0.0, 255.0));
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

    sprGraph.fillSprite(TFT_BLACK);     // Sprite törlése minden rajzolás előtt    // 1. Adatok eltolása balra
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        for (int c = 0; c < width - 1; ++c) {
            wabuf[r][c] = wabuf[r][c + 1];
        }
    }

    float currentBinWidthHz = pAudioProcessor ? pAudioProcessor->getBinWidthHz() : (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    if (currentBinWidthHz == 0) currentBinWidthHz = (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // Get the actual FFT size from the processor or use default fallback
    const uint16_t actualFftSize = pAudioProcessor ? pAudioProcessor->getFftSize() : AudioProcessorConstants::DEFAULT_FFT_SAMPLES;

    const int min_bin_for_wf_env = std::max(2, static_cast<int>(std::round(AudioProcessorConstants::LOW_FREQ_ATTENUATION_THRESHOLD_HZ / currentBinWidthHz)));
    const int max_bin_for_wf_env = std::min(static_cast<int>(actualFftSize / 2 - 1), static_cast<int>(std::round(currentConfiguredMaxDisplayAudioFreqHz / currentBinWidthHz)));
    const int num_bins_in_wf_env_range = NUM_BINS(max_bin_for_wf_env, min_bin_for_wf_env);

    // 2. Új adatok betöltése
    // Az Envelope módhoz az RvReal értékeit használjuk, de egy saját erősítéssel.
    for (int r = 0; r < height; ++r) {  // Teljes `this->height`
        // 'r' (0 to height-1) leképezése FFT bin indexre a szűkített tartományon belül
        int fft_bin_index = min_bin_for_wf_env + static_cast<int>(std::round(static_cast<float>(r) / std::max(1, (height - 1)) * (num_bins_in_wf_env_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_bin_for_wf_env, max_bin_for_wf_env);

        if (!pAudioProcessor) continue;
        // Az AudioProcessor->getMagnitudeData()[fft_bin_index] már tartalmazza a csillapított értéket.
        // Alkalmazzuk az ENVELOPE_INPUT_GAIN-t.
        double gained_val = (pAudioProcessor ? pAudioProcessor->getMagnitudeData()[fft_bin_index] : 0.0) * ENVELOPE_INPUT_GAIN;
        wabuf[r][width - 1] = static_cast<uint8_t>(constrain(gained_val, 0.0, 255.0));  // 0-255 közé korlátozzuk a wabuf számára
    }

    // 3. Burkológörbe kirajzolása
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
            // A simított amplitúdó skálázása a grafikon magasságára (0-255 -> 0-graphH)
            float y_offset_float = (envelope_prev_smoothed_max_val / 255.0f) * (graphH / 2 - 1);  // 0-255 amplitúdó a grafikon feléig

            int y_offset_pixels = static_cast<int>(round(y_offset_float));
            y_offset_pixels = std::min(y_offset_pixels, graphH / 2 - 1);
            if (y_offset_pixels < 0) y_offset_pixels = 0;

            if (y_offset_pixels >= 0) {
                int yCenter_on_sprite = graphH / 2;
                int yUpper_on_sprite = yCenter_on_sprite - y_offset_pixels;
                int yLower_on_sprite = yCenter_on_sprite + y_offset_pixels;

                yUpper_on_sprite = constrain(yUpper_on_sprite, 0, graphH - 1);
                yLower_on_sprite = constrain(yLower_on_sprite, 0, graphH - 1);

                if (yUpper_on_sprite <= yLower_on_sprite) {
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

    // 2. Nagy felbontású adatok használata
    float currentBinWidthHz = pAudioProcessor ? pAudioProcessor->getBinWidthHz() : (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);
    if (currentBinWidthHz == 0) currentBinWidthHz = (40000.0f / AudioProcessorConstants::DEFAULT_FFT_SAMPLES);

    // Get the actual FFT size from the high-res processor or use default fallback
    const uint16_t actualHighResFftSize = pAudioProcessor ? pAudioProcessor->getFftSize() : AudioProcessorConstants::HIGH_RES_FFT_SAMPLES;

    const int min_fft_bin_for_tuning_local = std::max(1, static_cast<int>(std::round(currentTuningAidMinFreqHz_ / currentBinWidthHz)));
    const int max_fft_bin_for_tuning_local = std::min(static_cast<int>(actualHighResFftSize / 2 - 1), static_cast<int>(std::round(currentTuningAidMaxFreqHz_ / currentBinWidthHz)));
    const int num_bins_in_tuning_range = NUM_BINS(max_fft_bin_for_tuning_local, min_fft_bin_for_tuning_local);

    // Biztonsági ellenőrzés - nagy felbontású processor szükséges
    if (!pAudioProcessor or pAudioProcessor->getFftSize() != AudioProcessorConstants::HIGH_RES_FFT_SAMPLES) return;

    // Iterálás a komponens teljes szélességén (minden pixel oszlop)
    for (int c = 0; c < width; ++c) {
        // Képernyő pixel X koordinátájának (c) leképezése FFT bin indexre
        // a nagy felbontású FFT tartományon belül
        float ratio_in_display_width = (width <= 1) ? 0.0f : (static_cast<float>(c) / (width - 1));  // <= 1 a biztonság kedvéért
        int fft_bin_index = min_fft_bin_for_tuning_local + static_cast<int>(std::round(ratio_in_display_width * (num_bins_in_tuning_range - 1)));
        fft_bin_index = constrain(fft_bin_index, min_fft_bin_for_tuning_local, max_fft_bin_for_tuning_local);
        fft_bin_index = constrain(fft_bin_index, 2, static_cast<int>(actualHighResFftSize / 2 - 1));

        // Nagy felbontású FFT adatok használata
        wabuf[0][c] = static_cast<uint8_t>(constrain((pAudioProcessor->getMagnitudeData()[fft_bin_index]) * TUNING_AID_INPUT_SCALE, 0.0, 255.0));
    }

    // 3. Sprite görgetése és új sor kirajzolása
    sprGraph.scroll(0, 1);  // Tartalom görgetése 1 pixellel lefelé

    // Az új (legfelső) sor kirajzolása a sprite-ra (teljes szélességben)
    for (int c = 0; c < width; ++c) {
        // 'c' (0-tól width-1-ig) az index a wabuf[0]-hoz és a sprite X koordinátája
        uint16_t color = valueToWaterfallColor(WF_GRADIENT * wabuf[0][c]);
        sprGraph.drawPixel(c, 0, color);  // Rajzolás a sprite x=c, y=0 pozíciójára
    }

    // 4. Célfrekvencia vonalának kirajzolása a sprite-ra
    // Only draw lines if the tuning aid type is not OFF_DECODER
    if (currentTuningAidType_ != TuningAidType::OFF_DECODER) {

        // A pontosabb pozícionáláshoz a beállított min/max frekvenciákat használjuk
        float min_freq_displayed_actual = currentTuningAidMinFreqHz_;
        float max_freq_displayed_actual = currentTuningAidMaxFreqHz_;
        float displayed_span_hz = max_freq_displayed_actual - min_freq_displayed_actual;  // Szöveg beállítások a frekvencia kiíráshoz (a sprite-ra rajzolunk)
        sprGraph.setFreeFont();                                                           // Alapértelmezett vagy válassz egy kisebb, jól olvasható fontot a sprite-hoz
        sprGraph.setTextSize(1);                                                          // Megfelelő méret a sprite-on

        sprGraph.setTextDatum(BC_DATUM);  // Bottom-Center igazítás a vonal alatti kiíráshoz

        if (displayed_span_hz > 0) {
            if (currentTuningAidType_ == TuningAidType::CW_TUNING) {
                // CW célfrekvencia (config.data.cwReceiverOffsetHz) középre kerül
                int line_x_on_sprite = width / 2;
                sprGraph.drawFastVLine(line_x_on_sprite, 0, graphH, TUNING_AID_TARGET_LINE_COLOR);
                sprGraph.setTextColor(TUNING_AID_TARGET_LINE_COLOR, TFT_BLACK);
                sprGraph.drawString(String(static_cast<int>(config.data.cwReceiverOffsetHz)) + "Hz", line_x_on_sprite, graphH - 2);

            } else if (currentTuningAidType_ == TuningAidType::RTTY_TUNING) {
                float f_mark_cfg = config.data.rttyMarkFrequencyHz;
                float f_space_cfg = config.data.rttyMarkFrequencyHz - config.data.rttyShiftHz;
                // Space vonal
                if (f_space_cfg >= min_freq_displayed_actual && f_space_cfg <= max_freq_displayed_actual) {
                    float ratio_space = (f_space_cfg - min_freq_displayed_actual) / displayed_span_hz;
                    int line_x_space = static_cast<int>(std::round(ratio_space * (width - 1)));  // width-1, ha 0-tól width-1-ig terjed
                    line_x_space = constrain(line_x_space, 0, width - 1);                        // Biztosítjuk, hogy a sprite-on belül legyen
                    sprGraph.drawFastVLine(line_x_space, 0, graphH, TUNING_AID_RTTY_SPACE_LINE_COLOR);
                    sprGraph.setTextColor(TUNING_AID_RTTY_SPACE_LINE_COLOR, TFT_BLACK);
                    sprGraph.drawString(String(static_cast<int>(round(f_space_cfg))) + "Hz", line_x_space, graphH - 2);
                }
                // Mark vonal
                if (f_mark_cfg >= min_freq_displayed_actual && f_mark_cfg <= max_freq_displayed_actual) {
                    float ratio_mark = (f_mark_cfg - min_freq_displayed_actual) / displayed_span_hz;
                    int line_x_mark = static_cast<int>(std::round(ratio_mark * (width - 1)));
                    line_x_mark = constrain(line_x_mark, 0, width - 1);
                    sprGraph.drawFastVLine(line_x_mark, 0, graphH, TUNING_AID_RTTY_MARK_LINE_COLOR);
                    sprGraph.setTextColor(TUNING_AID_RTTY_MARK_LINE_COLOR, TFT_BLACK);
                    sprGraph.drawString(String(static_cast<int>(round(f_mark_cfg))) + "Hz", line_x_mark, graphH - 2);
                }
            }
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
