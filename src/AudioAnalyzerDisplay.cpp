#include "AudioAnalyzerDisplay.h"

#include "rtVars.h"  // Szükséges a ::newDisplay globális változóhoz

// Színprofilok
namespace FftDisplayConstants {
const uint16_t colors0[16] = {0x0000, 0x000F, 0x001F, 0x081F, 0x0810, 0x0800, 0x0C00, 0x1C00, 0xFC00, 0xFDE0, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};  // Cold
const uint16_t colors1[16] = {0x0000, 0x1000, 0x2000, 0x4000, 0x8000, 0xC000, 0xF800, 0xF8A0, 0xF9C0, 0xFD20, 0xFFE0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};  // Hot
};  // namespace FftDisplayConstants

/**
 * Az AudioAnalyzerDisplay osztály konstruktora
 */
AudioAnalyzerDisplay::AudioAnalyzerDisplay(TFT_eSPI& tft_ref, SI4735& si4735_ref, Band& band_ref)
    : DisplayBase(tft_ref, si4735_ref, band_ref), FFT() {  // Az FFT tagváltozó alapértelmezett konstruktorának hívása
    DEBUG("AudioAnalyzerDisplay::AudioAnalyzerDisplay\n");
    // A buildHorizontalScreenButtons-t a drawScreen-ben hívjuk,
    // miután a képernyő méretei és a DisplayBase inicializálása megtörtént.
}

/**
 * Az AudioAnalyzerDisplay osztály destruktora
 */
AudioAnalyzerDisplay::~AudioAnalyzerDisplay() { DEBUG("AudioAnalyzerDisplay::~AudioAnalyzerDisplay\n"); }

/**
 *
 */
void AudioAnalyzerDisplay::drawScreen() {
    tft.fillScreen(TFT_COLOR_BACKGROUND);  // Háttér törlése

    // Státuszsor kirajzolása (ősosztályból)
    dawStatusLine();

    // Csak az "Exit" gombot hozzuk létre a horizontális gombsorból
    DisplayBase::BuildButtonData exitButtonData[] = {
        {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
    };
    DisplayBase::buildHorizontalScreenButtons(exitButtonData, ARRAY_ITEM_COUNT(exitButtonData), false);

    // Az "Exit" gombot jobbra igazítjuk
    TftButton* exitButton = findButtonByLabel("Exit");
    if (exitButton != nullptr) {
        uint16_t exitButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;
        // Az Y pozíciót az alapértelmezett horizontális elrendezésből vesszük, ami a képernyő alja
        uint16_t exitButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, 0, false);
        exitButton->setPosition(exitButtonX, exitButtonY);  // Gomb pozicionálása
        // A DisplayBase::drawScreenButtons() fogja ténylegesen kirajzolni
    }

    // Képernyőgombok kirajzolása (most csak az "Exit" gomb)
    drawScreenButtons();

    // A gombok által elfoglalt magasság meghatározása
    uint16_t buttonAreaHeight = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2;

    // Frekvencia skála kirajzolása alulra
    audioScaleAnalyzer(buttonAreaHeight);  // Átadjuk a gombok magasságát, hogy felette rajzoljon

    // A vízesés rajzolásának kezdő Y pozíciója, közvetlenül a skála felett
    current_y_analyzer = tft.height() - buttonAreaHeight - (AudioAnalyzerConstants::ANALYZER_BOTTOM_MARGIN + 1);

    // Az FFT objektumot a konstruktorban inicializáltuk.
}

void AudioAnalyzerDisplay::displayLoop() {
    // Ha van dialógus ablak, akkor nem csinálunk semmit a háttérben.
    if (pDialog != nullptr) {
        return;
    }

    // FFT mintavételezés és számítás
    FFTSampleAnalyzer();  // Az eredmények (magnitúdók) a vReal tömbbe kerülnek

    // Az új spektrumvonal kirajzolása
    // Végigiterálunk a kijelző szélességén, és leképezzük az FFT "bin"-ekre (frekvenciasávokra)
    for (int x_coord = 0; x_coord < tft.width(); x_coord++) {
        // Képernyő x-koordináta leképezése FFT bin indexre
        // Az FFT_START_BIN_OFFSET-től indulunk (kb. 100Hz) az FFT_SAMPLES/2 - 1 bin-ig (Nyquist).
        
        constexpr int start_display_bin = AudioAnalyzerConstants::FFT_START_BIN_OFFSET;
        constexpr int end_display_bin = AudioAnalyzerConstants::FFT_SAMPLES / 2 - 1;
        constexpr int num_displayable_bins = end_display_bin - start_display_bin + 1;

        int fft_bin_index;
        if (num_displayable_bins <= 1) { // Elkerüljük az osztást nullával vagy negatívval, ha a tartomány túl szűk
            fft_bin_index = start_display_bin;
        } else {
            fft_bin_index = start_display_bin + static_cast<int>(roundf(((float)x_coord / (tft.width() - 1.0f)) * (num_displayable_bins - 1.0f)));
        }
        fft_bin_index = constrain(fft_bin_index, start_display_bin, end_display_bin);

        // Magnitúdó kiolvasása a vReal tömbből
        float magnitude = vReal[fft_bin_index];

        // Magnitúdó skálázása és normalizálása (0.0 és 1.0 közé)
        float normalized_magnitude = magnitude / AudioAnalyzerConstants::AMPLITUDE_SCALE;
        if (normalized_magnitude > 1.0f) normalized_magnitude = 1.0f;
        if (normalized_magnitude < 0.0f) normalized_magnitude = 0.0f;

        // Szín meghatározása (a FftDisplayConstants-ben lévő colors0 profilt használva)
        uint16_t color = valueToWaterfallColorAnalyzer(normalized_magnitude, 0.0f, 1.0f, 0);

        // Pixel kirajzolása
        tft.drawPixel(x_coord, current_y_analyzer, color);
    }

    // A gombok által elfoglalt magasság meghatározása
    uint16_t buttonAreaHeight = SCRN_BTN_H + SCREEN_HBTNS_Y_MARGIN * 2;
    // A vízesés alsó határa (ahol újra kell kezdeni a rajzolást)
    uint16_t waterfallBottomLimit = tft.height() - buttonAreaHeight - (AudioAnalyzerConstants::ANALYZER_BOTTOM_MARGIN + 1);

    // Kijelző görgetése: Y csökkentése, körbejárás, ha eléri a tetejét
    current_y_analyzer--;
    // A görgetés felső határa a státuszsor alatt kezdődik
    if (current_y_analyzer < AudioAnalyzerConstants::WATERFALL_TOP_Y) {
        current_y_analyzer = waterfallBottomLimit;
        // Opcionális: a skála újrarajzolása, ha felülíródott vagy az átláthatóság kedvéért,
        // de az új vonal rajzolása előtti fillRect-nek ezt kezelnie kellene.

        // Annak a sornak a törlése, amelyik újra lesz rajzolva, a szellemkép elkerülése érdekében
        tft.drawFastHLine(0, current_y_analyzer, tft.width(), TFT_COLOR_BACKGROUND);
    }
    // A következő rajzolandó sor törlése, hogy elkerüljük az előző képkocka szellemképét
    if (current_y_analyzer >= AudioAnalyzerConstants::WATERFALL_TOP_Y) {  // Biztosítjuk, hogy y a határokon belül legyen
        tft.drawFastHLine(0, current_y_analyzer, tft.width(), TFT_COLOR_BACKGROUND);
    } else {  // Ha y körbejárt, töröljük az új kezdő sort
        tft.drawFastHLine(0, waterfallBottomLimit, tft.width(), TFT_COLOR_BACKGROUND);
    }
}

bool AudioAnalyzerDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    // Ebben a képernyőben a rotary encodernek nincs funkciója
    return false;
}

bool AudioAnalyzerDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    // Ebben a képernyőben a direkt érintésnek (nem gomb) nincs funkciója
    return false;
}

void AudioAnalyzerDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) {
    if (STREQ("Exit", event.label)) {
        // Visszalépés az előző képernyőre (amit a DisplayBase tárol)
        ::newDisplay = prevDisplay;              // Visszatérés az előző képernyőre
        if (prevDisplay == DisplayType::none) {  // Ha nincs explicit előző, menjünk a fő (FM/AM) képernyőre
            ::newDisplay = band.getCurrentBandType() == FM_BAND_TYPE ? DisplayType::fm : DisplayType::am;
        }
    }
}

/**
 * @brief Audio mintavételezés és FFT végrehajtása. Az eredmények (magnitúdók) a vReal tömbbe kerülnek.
 */
void AudioAnalyzerDisplay::FFTSampleAnalyzer() {

    using namespace AudioAnalyzerConstants;

    for (int i = 0; i < FFT_SAMPLES; i++) {
        // Analóg érték olvasása, középre igazítás 0 körül az FFT-hez (12 bites ADC-t, 0-4095 tartományban)
        // Módosítsd a 2048-at, ha az ADC-dnek más a középpontja vagy tartománya.
        vReal[i] = (double)analogRead(AUDIO_INPUT_PIN) - 2048.0;
        vImag[i] = 0;  // A képzetes rész 0 valós bemenet esetén
    }

    FFT.windowing(vReal, FFT_SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);  // Ablakozás (Hamming ablak)
    FFT.compute(vReal, vImag, FFT_SAMPLES, FFT_FORWARD);                  // FFT számítás
    FFT.complexToMagnitude(vReal, vImag, FFT_SAMPLES);                    // Komplex számokból magnitúdók számítása, az eredmény a vReal-be kerül
}

/**
 * @brief Kirajzolja a frekvencia skálát az analizátor aljára.
 */
void AudioAnalyzerDisplay::audioScaleAnalyzer(uint16_t occupiedBottomHeight) {
    using namespace AudioAnalyzerConstants;

    // A skála Y pozíciója a gombok felett
    uint16_t scaleBaseY = tft.height() - occupiedBottomHeight;

    tft.fillRect(0, scaleBaseY - (ANALYZER_BOTTOM_MARGIN - 1), tft.width(), (ANALYZER_BOTTOM_MARGIN - 1), TFT_COLOR_BACKGROUND);
    tft.drawLine(0, scaleBaseY - ANALYZER_BOTTOM_MARGIN, tft.width(), scaleBaseY - ANALYZER_BOTTOM_MARGIN, TFT_BLUE);
    tft.setTextColor(TFT_YELLOW, TFT_COLOR_BACKGROUND);  // Háttérszínnel töröljön
    tft.setTextSize(1);                                  // Kisebb betűméret a skálához
    tft.setFreeFont();                                   // Standard font

    // Kiszámítja a ténylegesen megjelenített kezdő és végfrekvenciát kHz-ben
    float actual_start_freq_for_scale_kHz = (float)AudioAnalyzerConstants::FFT_START_BIN_OFFSET * (AudioAnalyzerConstants::SAMPLING_FREQUENCY / (float)AudioAnalyzerConstants::FFT_SAMPLES) / 1000.0f;
    float max_freq_for_scale_kHz = (AudioAnalyzerConstants::SAMPLING_FREQUENCY / 2.0f) / 1000.0f;
    float displayed_freq_span_kHz = max_freq_for_scale_kHz - actual_start_freq_for_scale_kHz;
    if (displayed_freq_span_kHz < 0) displayed_freq_span_kHz = 0; // Negatív tartomány elkerülése

    for (int i = 0; i <= 4; i++) {                       // 5 címkét rajzol (0, 1/4, 1/2, 3/4, 1 * max_freq)
        float freq_k = actual_start_freq_for_scale_kHz + (displayed_freq_span_kHz * (i / 4.0f));
        int x_pos = (tft.width() / 4) * i;
        char label[10];

        // dtostrf(freq_k, 3, (freq_k < 10 && freq_k != 0 ? 1 : 0), label);  // 3 karakter összesen, 1 tizedesjegy ha < 10 és nem 0
        // Robusztusabb formázás:
        if (freq_k < 0.001f && i == 0) { // Ha az első címke nagyon közel van a 0-hoz (de nem 0), írjuk ki pontosabban, vagy kerekítsük
             dtostrf(freq_k, 4, 2, label); // pl. "0.10"
        } else {
            dtostrf(freq_k, 4, (freq_k < 1.0f && i > 0 ? 2 : (freq_k < 10.0f ? 1 : 0)), label); // pl. "0.97", "1.2", "12"
        }
        strcat(label, "k");

        int textWidth = strlen(label) * 6;                              // Hozzávetőleges szélesség (6px/karakter 1-es méretnél)
        int text_x = x_pos - (i == 4 ? textWidth - 3 : textWidth / 2);  // Címkék középre igazítása, az utolsó jobbra
        if (text_x < 0) text_x = 0;
        if (text_x + textWidth > tft.width()) text_x = tft.width() - textWidth;
        tft.setTextDatum(BC_DATUM);                                                                    // Bottom-Center igazítás a skála szövegéhez
        tft.drawString(label, text_x + textWidth / 2, scaleBaseY - (ANALYZER_BOTTOM_MARGIN / 2) + 4);  // Y pozíció igazítása a scaleBaseY-hoz
    }
}

/**
 * @brief Egy értéket RGB565 színkóddá alakít a vízesés diagramhoz.
 * @param val Bemeneti érték (normalizálás után tipikusan 0.0 és 1.0 között).
 * @param min_val Várható minimális bemeneti érték.
 * @param max_val Várható maximális bemeneti érték.
 * @param colorProfileIndex Színprofil indexe (0 a FftDisplayConstants::colors0-hoz, 1 a FftDisplayConstants::colors1-hez).
 * @return RGB565 színkód.
 */
uint16_t AudioAnalyzerDisplay::valueToWaterfallColorAnalyzer(float val, float min_val, float max_val, byte colorProfileIndex) {
    uint16_t color;
    // A DisplayBase-ben definiált színprofilokat használjuk
    const uint16_t* colors = (colorProfileIndex == 0) ? FftDisplayConstants::colors0 : FftDisplayConstants::colors1;
    byte color_size = 16;  // A DisplayBase-ben lévő profilok mérete

    if (val < min_val) val = min_val;
    if (val > max_val) val = max_val;

    int index = (int)((val - min_val) * (color_size - 1) / (max_val - min_val));
    if (index < 0) index = 0;
    if (index >= color_size) index = color_size - 1;

    color = colors[index];
    return color;
}
