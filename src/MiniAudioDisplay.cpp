#include "MiniAudioDisplay.h"

#include "rtVars.h"  // ::newDisplay

// Színprofilok az AudioAnalyzerDisplay-ből
namespace FftDisplayConstants {
extern const uint16_t colors0[16];
extern const uint16_t colors1[16];
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
    memset(wabuf, 0, sizeof(wabuf));
    // A gombokat a drawScreen-ben hozzuk létre
}

MiniAudioDisplay::~MiniAudioDisplay() {}

/**
 *
 */
void MiniAudioDisplay::drawScreen() {
    tft.fillScreen(TFT_COLOR_BACKGROUND);
    dawStatusLine();  // Státuszsor

    // Gombok: "Back" és "Mode"
    DisplayBase::BuildButtonData screenButtonsData[] = {
        {"Mode", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off}, {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},  // Átnevezve "Exit"-re
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
    if (currentMiniWindowMode == 1 || (currentMiniWindowMode != 1 && Rpeak[0] != 0)) {  // A második feltétel nem tökéletes
        memset(Rpeak, 0, sizeof(Rpeak));
    }
    // Reset wabuf if switching to mode 4/5 or away
    if (currentMiniWindowMode == 4 || currentMiniWindowMode == 5 || (currentMiniWindowMode < 4 && wabuf[0][0] != 0)) {
        memset(wabuf, 0, sizeof(wabuf));
    }
    highResOffset = 0;  // Reset high-res offset
}

void MiniAudioDisplay::FFTSampleMini(bool drawOsci) {
    using namespace MiniAudioDisplayConstants;
    // Az FFT.ino FFTSample logikája alapján
    for (int i = 0; i < FFT_SAMPLES; i++) {
        uint32_t sum = 0;
        // Túlmintavételezés, mint az FFT.ino-ban (dly=0 eset)
        for (int j = 0; j < 4; j++) {
            sum += analogRead(AUDIO_INPUT_PIN);
        }

        if (drawOsci && i < MINI_OSCI_SAMPLES_TO_DRAW) {
            // Mini oszcilloszkóp rajzolása
            // Az FFT.ino-ban: int am = sum / 100; am = std::clamp(am, 43, 69);
            // tft.drawPixel(135 + i, 250 + am, TFT_WHITE);
            // Ezt a drawMiniOscilloscope-ba kellene helyezni, vagy itt egy egyszerűsített verziót.
            // Most a FFTSampleMini csak az adatgyűjtést és FFT-t végzi.
            // A `sum` itt 4 olvasás összege, az `am` skálázása ehhez igazodjon.
            // Az eredeti `250 + am` a `startY + magasság_középpont + skálázott_jel` lenne.
        }
        // Az FFT.ino-ban: RvReal[i] = sum / (sampleCount / 64);
        // Ha sampleCount = 256, akkor (256/64) = 4. Tehát sum / 4.
        // A vReal-t kell feltölteni a nyers (de DC-eltolt) adatokkal.
        // Az FFT könyvtár várja, hogy a vReal[i] a jel értéke legyen.
        // Az FFT.ino-ban az RvReal-t használja bemenetként és kimenetként is.
        // Itt a vReal-t használjuk bemenetként.
        vReal[i] = (double)(sum / 4.0) - 2048.0;  // Átlagolás és DC eltolás (feltételezve 0-4095 ADC)
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

// --- Kirajzoló függvények implementációi (kezdetben üresek vagy egyszerűsítettek) ---
void MiniAudioDisplay::drawMiniSpectrumLowRes() {
    // FFT.ino: miniWindowMode == 1 logika
    // Szükséges: Rpeak, getBandValMini, displayBandMini
    // A rajzolás a MINI_DISPLAY_AREA_X, MINI_DISPLAY_AREA_Y koordinátákhoz képest történik.
    using namespace MiniAudioDisplayConstants;

    // Ténylegesen rajzolt terület szélességének kiszámítása
    constexpr int total_width_low_res = (LOW_RES_BANDS * 3) + ((LOW_RES_BANDS - 1) * 2); // 3px sáv, 2px rés
    // X eltolás a középre igazításhoz
    int x_pixel_offset_low_res = 0;
    if (MINI_DISPLAY_AREA_W > total_width_low_res) {
        x_pixel_offset_low_res = (MINI_DISPLAY_AREA_W - total_width_low_res) / 2;
    }
    // A sávok tényleges kezdő X koordinátája a MINI_DISPLAY_AREA-n belül
    int actual_start_x_low_res = MINI_DISPLAY_AREA_X + x_pixel_offset_low_res;

    // Szürke keret a ténylegesen rajzolt terület köré
    tft.drawRect(actual_start_x_low_res - 1, MINI_DISPLAY_AREA_Y - 1, total_width_low_res + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    // Régi peak-ek törlése feketével
    for (byte band_idx = 0; band_idx <= LOW_RES_BANDS; band_idx++) {
        if (Rpeak[band_idx] > 0) {  // Csak akkor töröljünk, ha volt mit
            int xPos = actual_start_x_low_res + 5 * band_idx; // Igazított X pozíció
            // Az Y pozíció a mini display aljától számítva
            int yPos = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - Rpeak[band_idx];
            tft.fillRect(xPos, yPos, 3, 2, TFT_NAVY);  // Háttérszínnel törlés
        }
        if (Rpeak[band_idx] >= 1) {
            Rpeak[band_idx] -= 1;  // Lassú csökkenés
        }
    }

    // Új értékek rajzolása
    for (int i = 2; i < (FFT_SAMPLES / 2); i++) {  // Csak az alsó felét használjuk
        // Az FFT.ino-ban: if (RvReal[i] > 750) { // eliminate noise
        // Ez a 750-es küszöb az ottani RvReal skálázáshoz tartozik.
        // Itt az RvReal értékei mások lehetnek. Egyelőre egy alacsonyabb küszöböt használunk.
        if (RvReal[i] > (MINI_AMPLITUDE_SCALE / 10.0)) {  // Kísérleti zajküszöb
            byte band_idx = getBandValMini(i);
            displayBandMini(band_idx, (int)RvReal[i], actual_start_x_low_res); // Átadjuk az igazított kezdő X-et
        }
    }
}

uint8_t MiniAudioDisplay::getBandValMini(int fft_bin_index) {
    // Az FFT.ino getBandVal logikája alapján, de a bin indexek eltérhetnek
    // az FFT_SAMPLES miatt. Most 256 mintával dolgozunk.
    // FFT_SAMPLES / 2 = 128 binünk van (a DC-t és a Nyquistet nem számítva).
    // Ezt kell 16 sávra osztani. 128 / 16 = 8 bin/sáv.
    if (fft_bin_index < 2) return 0;  // Első pár bin-t kihagyjuk
    return constrain((fft_bin_index - 2) / 8, 0, MiniAudioDisplayConstants::LOW_RES_BANDS - 1);
}

void MiniAudioDisplay::displayBandMini(int band_idx, int magnitude, int actual_start_x) {
    using namespace MiniAudioDisplayConstants;
    // Az FFT.ino displayBand logikája alapján
    // dsize /= amplitude;
    // if (dsize > dmax) dsize = dmax;
    // A magasságot a MINI_DISPLAY_AREA_H-hoz kell skálázni.
    // LOW_RES_PEAK_MAX_HEIGHT (23) az eredeti kód max magassága volt a 25 pixeles területen.
    // Itt a MINI_DISPLAY_AREA_H-hoz igazítjuk.

    int dsize = magnitude / MINI_AMPLITUDE_SCALE;
    dsize = constrain(dsize, 0, MINI_DISPLAY_AREA_H - 1);  // Skálázás a teljes magasságra

    int xPos = actual_start_x + 5 * band_idx; // Igazított X pozíció használata
    if (xPos + 2 >= actual_start_x + (LOW_RES_BANDS * 5)) return;  // Ne lógjon ki a sávok tényleges területéről

    // Sáv rajzolása
    if (dsize > 0) {
        tft.fillRect(xPos, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - dsize, 3, dsize, TFT_YELLOW);
    }

    // Peak frissítése (a dsize itt már a magasság a MINI_DISPLAY_AREA_H-n belül)
    if (dsize > Rpeak[band_idx]) {
        Rpeak[band_idx] = dsize;
    }
}

void MiniAudioDisplay::drawMiniSpectrumHighRes() {
    using namespace MiniAudioDisplayConstants;
    // Az FFT.ino miniWindowMode == 2 logikája alapján

    // X pozíció középre igazítása, ha a terület szélesebb, mint a megjelenítendő sávok
    int x_pixel_offset = 0;
    if (MINI_DISPLAY_AREA_W > HIGH_RES_BINS_TO_DISPLAY) {
        x_pixel_offset = (MINI_DISPLAY_AREA_W - HIGH_RES_BINS_TO_DISPLAY) / 2;
    }
    // A sávok tényleges kezdő X koordinátája a MINI_DISPLAY_AREA-n belül
    int actual_start_x_high_res = MINI_DISPLAY_AREA_X + x_pixel_offset;

    // Szürke keret a ténylegesen rajzolt terület köré
    tft.drawRect(actual_start_x_high_res - 1, MINI_DISPLAY_AREA_Y - 1, HIGH_RES_BINS_TO_DISPLAY + 2, MINI_DISPLAY_AREA_H + 2, TFT_DARKGREY);

    // Előző képkocka törlése (csak a sávok helyén) - egyszerűsített, a teljes területet töröljük
    // A clearMiniDisplayArea() a cycleMiniWindowMode-ban és a MUTE kezelésben már megtörténik.
    // Itt csak a sávok előző pozícióját kellene törölni, ha a háttér nem lenne minden ciklusban törölve.
    // Mivel a displayLoop-ban a clearMiniDisplayArea() nincs minden ciklusban, itt kell törölni.
    // De a sávok maguk törlik az előző helyüket a háttérszínnel.

    for (int i = 2; i < HIGH_RES_BINS_TO_DISPLAY + 2; i++) {  // Az FFT.ino SAMPLES/3 bin-t rajzol, ami kb. 85. i-2 az indexelés miatt.
        if (i >= FFT_SAMPLES / 2) break;                      // Ne lépjük túl a valós adatokat

        int screen_x = actual_start_x_high_res + (i - 2); // Igazított X pozíció
        if (screen_x >= actual_start_x_high_res + HIGH_RES_BINS_TO_DISPLAY) continue;  // Ne lógjon ki a sávok tényleges területéről

        // Előző sáv törlése (egyszerűsített: az egész oszlopot töröljük a háttérszínnel)
        tft.drawFastVLine(screen_x, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_H, TFT_NAVY);

        int scaled_magnitude = (int)(RvReal[i] / MINI_AMPLITUDE_SCALE);  // Skálázás
        scaled_magnitude = constrain(scaled_magnitude, 0, MINI_DISPLAY_AREA_H - 1);

        int y_bar_start = MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1 - scaled_magnitude + (highResOffset % (MINI_DISPLAY_AREA_H / 10));  // "Táncoló" effektus
        y_bar_start = constrain(y_bar_start, MINI_DISPLAY_AREA_Y, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1);
        int bar_actual_height = (MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H - 1) - y_bar_start + 1;
        if (bar_actual_height > 0 && scaled_magnitude > 0) {  // Csak akkor rajzolunk, ha van magassága
            tft.drawFastVLine(screen_x, y_bar_start, bar_actual_height, TFT_SKYBLUE);
        }
    }
    highResOffset++;  // A "táncoló" effektushoz
}

void MiniAudioDisplay::drawMiniOscilloscope() {
    // Az FFTSampleMini(true) hívásakor kellene rajzolni, vagy itt külön.
    // Az FFT.ino-ban a FFTSample-ön belül van.
    // Itt egy egyszerűsített verzió:
    using namespace MiniAudioDisplayConstants;
    // A vReal most az FFT bemeneti mintáit tartalmazza (DC eltolva)
    // Mielőtt az FFT lefutna, a vReal-ban vannak az időtartománybeli minták.
    // Ezt a displayLoop-ban az FFTSampleMini előtt kellene meghívni, ha mode 3.
    // Vagy az FFTSampleMini-nek kellene egy flag, hogy rajzoljon-e.
    // Most feltételezzük, hogy az FFTSampleMini(true) már feltöltötte a vReal-t
    // a megfelelő módon (vagy egy külön bufferbe mentette az időtartománybeli adatokat).

    // Az FFTSampleMini-t úgy módosítottam, hogy a vReal-t használja FFT bemenetként.
    // Az oszcilloszkóphoz az eredeti, nem FFT-zett, de átlagolt és DC-eltolt minták kellenek.
    // Ezt az FFTSampleMini-nek kellene biztosítania egy külön bufferben, vagy a rajzolást ott végezni.

    // Egyszerűsítés: most csak egy vonalat rajzolunk, ami jelzi, hogy ez a mód aktív.
    tft.drawLine(MINI_DISPLAY_AREA_X, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H / 2, MINI_DISPLAY_AREA_X + MINI_DISPLAY_AREA_W, MINI_DISPLAY_AREA_Y + MINI_DISPLAY_AREA_H / 2,
                 TFT_GREEN);
    tft.setCursor(MINI_DISPLAY_AREA_X + 5, MINI_DISPLAY_AREA_Y + 5);
    tft.print("Oscilloscope (WIP)");
}

void MiniAudioDisplay::drawMiniWaterfall() { /* TODO */ }

void MiniAudioDisplay::drawMiniEnvelope() { /* TODO */ }

uint16_t MiniAudioDisplay::valueToMiniWaterfallColor(int scaled_value) {
    // Az FFT.ino-ban: valueToWaterfallColor(gradient * wabuf[i][j])
    // A `wabuf` értékei `RvReal[i+2] / amplitude`.
    // Ha `amplitude` = MINI_AMPLITUDE_SCALE = 200.
    // `RvReal` értékei az FFT után lehetnek nagyok.
    // Normalizáljuk a `scaled_value`-t 0.0-1.0 közé.
    // A `gradient` 100 volt. Max `wabuf` érték kb. MINI_WF_HEIGHT.
    // Tehát max `scaled_value` kb. 100 * MINI_WF_HEIGHT.
    float normalized = (float)scaled_value / (float)(MiniAudioDisplayConstants::MINI_WF_GRADIENT * MiniAudioDisplayConstants::MINI_WF_HEIGHT);
    normalized = constrain(normalized, 0.0f, 1.0f);

    // Az AudioAnalyzerDisplay-ben lévő valueToWaterfallColorAnalyzer-hez hasonló logika
    const uint16_t* colors = FftDisplayConstants::colors0;  // FftDisplayConstants-ból származó konstansok használata
    byte color_size = 16;

    int index = (int)(normalized * (color_size - 1));
    index = constrain(index, 0, color_size - 1);
    return colors[index];
}

void MiniAudioDisplay::displayLoop() {
    if (pDialog != nullptr) return;

    // Muted felirat kezelése (az FFT.ino alapján)
    // A `rtv::muteStat` globális némítást használjuk
    if (rtv::muteStat) {
        if (!audioMutedState) {  // Ha eddig nem volt némítva a kijelzőn
            audioMutedState = true;
            clearMiniDisplayArea();
            tft.setCursor(MiniAudioDisplayConstants::MINI_DISPLAY_AREA_X + MiniAudioDisplayConstants::MINI_DISPLAY_AREA_W / 2,
                          MiniAudioDisplayConstants::MINI_DISPLAY_AREA_Y + MiniAudioDisplayConstants::MINI_DISPLAY_AREA_H / 2);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_YELLOW);
            tft.print("MUTED");
        }
        return;  // Némítva ne csináljunk mást
    } else {
        if (audioMutedState) {  // Ha eddig némítva volt a kijelzőn, de már nem globálisan
            audioMutedState = false;
            clearMiniDisplayArea();  // Töröljük a "MUTED" feliratot
            // A következő ciklusban újrarajzolódik a tartalom
        }
    }

    if (currentMiniWindowMode == 0) {  // Off
        // Opcionálisan törölhetjük a területet, ha nem volt már üres
        // clearMiniDisplayArea(); // Ezt a cycleMiniWindowMode már megteszi
        return;
    }

    if (currentMiniWindowMode == 3) {  // Oszcilloszkóp
        // Az oszcilloszkóp rajzolása speciális, az FFTSampleMini-n belül történhetne,
        // vagy itt kellene az időtartománybeli adatokat felhasználni az FFT előtt.
        // Most az FFTSampleMini-t hívjuk false-szal, és a drawMiniOscilloscope rajzol valamit.
        FFTSampleMini(false);  // FFT lefut, de nem használjuk oszcilloszkóphoz
        drawMiniOscilloscope();
    } else {
        FFTSampleMini(false);  // Adatgyűjtés és FFT a többi módhoz
        switch (currentMiniWindowMode) {
            case 1:
                drawMiniSpectrumLowRes();
                break;
            case 2:
                drawMiniSpectrumHighRes();
                break;
            // case 3: handled above
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

bool MiniAudioDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    // Ebben a képernyőben a direkt érintésnek (nem gomb) egyelőre nincs funkciója
    return false;
}

void MiniAudioDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) {
    if (STREQ("Exit", event.label)) {  // Átnevezve "Exit"-re
        ::newDisplay = prevDisplay;
        if (prevDisplay == DisplayType::none) {
            ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayType::fm : DisplayType::am;
        }
    } else if (STREQ("Mode", event.label)) {
        cycleMiniWindowMode();
    }
}
