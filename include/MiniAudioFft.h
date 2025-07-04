#ifndef MINI_AUDIO_FFT_H
#define MINI_AUDIO_FFT_H

#include <ArduinoFFT.h>
#include <TFT_eSPI.h>

#include <vector>  // std::vector használatához

#include "AudioProcessor.h"
#include "defines.h"  // AUDIO_INPUT_PIN és színek eléréséhez

// Konstansok a MiniAudioFft komponenshez
namespace MiniAudioFftConstants {

// Belső tömbök maximális méretei, ha a komponens mérete nagyobb lenne.
// A tényleges rajzolás a komponens w,h méreteihez van vágva/skálázva.
constexpr int MAX_INTERNAL_WIDTH = AudioProcessorConstants::MAX_INTERNAL_WIDTH;  // Oszcilloszkóp és magas felb. spektrum belső bufferéhez
constexpr int MAX_INTERNAL_HEIGHT = 80;                                          // Vízesés és burkológörbe belső buffer magassága
constexpr int LOW_RES_BANDS = 24;                                                // Alacsony felbontású spektrum sávjainak száma (csökkentve 24-re)
// A HIGH_RES_BINS_TO_DISPLAY és OSCI_SAMPLES_TO_DRAW a komponens aktuális szélességéből adódik.
// A WF_WIDTH és WF_HEIGHT a komponens aktuális szélességéből és magasságából (csökkentve a kijelzővel) adódik.

// Konstansok a LowRes spektrumhoz
constexpr float LOW_RES_SPECTRUM_MIN_FREQ_HZ = 300.0f;  // Alacsony felbontású spektrum kezdő frekvenciája (Hz)
// constexpr float LOW_RES_SPECTRUM_MAX_FREQ_HZ = 6000.0f; // Ezt most már a MAX_DISPLAY_AUDIO_FREQ_..._HZ konstansok határozzák meg
// constexpr int HIGH_RES_SPECTRUM_COLUMNS = 48;        // Eltávolítva, visszatérünk a vonalankénti rajzoláshoz
constexpr float MAX_DISPLAY_AUDIO_FREQ_AM_HZ = 6000.0f;   // Maximális megjelenítendő audio frekvencia AM módban
constexpr float MAX_DISPLAY_AUDIO_FREQ_FM_HZ = 15000.0f;  // Maximális megjelenítendő audio frekvencia FM módban

constexpr uint32_t TOUCH_DEBOUNCE_MS = 300;  // Érintés "debounce" ideje milliszekundumban

// Evenlope
constexpr float ENVELOPE_INPUT_GAIN = 0.05f;        // Erősítési faktor a burkológörbe bemenetéhez (csökkentve a vastagság miatt)
constexpr float ENVELOPE_SMOOTH_FACTOR = 0.25f;     // Simítási faktor a burkológörbéhez
constexpr float ENVELOPE_THICKNESS_SCALER = 0.95f;  // Burkológörbe vastagságának skálázója

// Oszcilloszkóp
constexpr float OSCI_SENSITIVITY_FACTOR = 25.0f;  // Oszcilloszkóp érzékenységi faktora (növelni a nagyobb amplitúdóhoz)
constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;  // Oszcilloszkóp mintavételi decimációs faktora

// Konstansok a CW//RTTY hangolási segéd módhoz
constexpr float CW_TUNING_AID_SPAN_HZ = 600.0f;                    // Megjelenített sávszélesség CW hangoláshoz
constexpr float TUNING_AID_INPUT_SCALE = 0.1f;                     // Erősítési faktor a hangolási segéd bemenetéhez (csökkentve a "vonal" vékonyításához)
constexpr uint16_t TUNING_AID_TARGET_LINE_COLOR = TFT_GREEN;       // Célvonal színe
constexpr uint16_t TUNING_AID_RTTY_SPACE_LINE_COLOR = TFT_CYAN;    // RTTY Space vonal színe
constexpr uint16_t TUNING_AID_RTTY_MARK_LINE_COLOR = TFT_MAGENTA;  // RTTY Mark vonal színe
//
constexpr int TUNING_AID_INTERNAL_WIDTH = MiniAudioFftConstants::MAX_INTERNAL_WIDTH;  // Belső szélesség a hangolási segédhez (a komponens szélessége)

// Vízesés
constexpr int WF_GRADIENT = 100;  // Vízesés színátmenetének erőssége
// Színek a vízeséshez
const uint16_t WATERFALL_COLORS[16] = {
    0x0000,                         // TFT_BLACK (index 0)
    0x0000,                         // TFT_BLACK (index 1)
    0x0000,                         // TFT_BLACK (index 2)
    0x001F,                         // Nagyon sötét kék
    0x081F,                         // Sötét kék
    0x0810,                         // Sötét zöldeskék
    0x0800,                         // Sötétzöld
    0x0C00,                         // Közepes zöld
    0x1C00,                         // Világosabb zöld
    0xFC00,                         // Narancs
    0xFDE0,                         // Világos sárga
    0xFFE0,                         // Sárga
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF  // Fehér a csúcsokhoz
};  // A színek változatlanok
constexpr int MAX_WATERFALL_COLOR_INPUT_VALUE = 20000;  // Maximális bemeneti érték a vízesés színkonverziójához

}  // namespace MiniAudioFftConstants

// Forward declaration
class AudioProcessor;

class MiniAudioFft {
   public:  // Publikus enum a könnyebb elérhetőségért, ha külsőleg is hivatkoznánk rá
    // Megjelenítési módok enum definíciója
    enum class DisplayMode : uint8_t { Off = 0, SpectrumLowRes, SpectrumHighRes, Oscilloscope, Waterfall, Envelope, TuningAid };

    // Enum a TuningAid típusának megkülönböztetésére
    enum class TuningAidType : uint8_t {
        CW_TUNING,
        RTTY_TUNING,
        OFF_DECODER  // Added to indicate that the main decoder is OFF
    };

   public:
    /**
     * @brief Konstruktor.
     * @param tft_ref Referencia a TFT_eSPI objektumra.
     * @param x A komponens bal felső sarkának X koordinátája.
     * @param y A komponens bal felső sarkának Y koordinátája.
     * @param w A komponens szélessége.
     * @param h A komponens magassága.
     * @param configuredMaxDisplayAudioFreq Az adott képernyőmódhoz (AM/FM) konfigurált maximális megjelenítendő audio frekvencia.
     * @param configModeField Referencia a Config_t megfelelő uint8_t mezőjére, ahova a módot menteni kell.
     * @param fftGainConfigRef Referencia a Config_t megfelelő float mezőjére az FFT erősítés konfigurációjához.
     * */
    MiniAudioFft(TFT_eSPI& tft, int x, int y, int w, int h, float configuredMaxDisplayAudioFreq, uint8_t& configDisplayModeFieldRef, float& fftGainConfigRef);
    ~MiniAudioFft();

    void setInitialMode(DisplayMode mode);  // Kezdeti mód beállítása
    /**
     * @brief A komponens fő ciklusfüggvénye, kezeli az FFT mintavételezést és a rajzolást.
     */
    void loop();
    /**
     * @brief Érintési események kezelése a komponensen belül.
     * @param touched Igaz, ha éppen érintés történik.
     * @param tx Az érintés X koordinátája.
     * @param ty Az érintés Y koordinátája.
     * @return Igaz, ha az eseményt a komponens kezelte, egyébként hamis.
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty);
    /**
     * @brief Kényszeríti a komponens teljes újrarajzolását az aktuális módban.
     */
    void forceRedraw();
    void setTuningAidType(TuningAidType type);

    DisplayMode getCurrentDisplayMode() const { return currentMode; }

   private:
    TFT_eSPI& tft;                  // Referencia a TFT objektumra
    int posX, posY, width, height;  // Komponens pozíciója és méretei

    DisplayMode currentMode;                       // Aktuális megjelenítési mód
    bool prevMuteState;                            // Előző némítási állapot a változások érzékeléséhez
    uint32_t modeIndicatorShowUntil;               // Időbélyeg, meddig látható a módkijelző
    bool isIndicatorCurrentlyVisible;              // A módkijelző aktuálisan látható-e
    uint32_t lastTouchProcessTime;                 // Utolsó érintésfeldolgozás ideje a debounce-hoz
    uint8_t& configModeFieldRef;                   // Referencia a Config mezőre a mód mentéséhez
    float currentConfiguredMaxDisplayAudioFreqHz;  // Az AM/FM módnak megfelelő maximális frekvencia
    float& activeFftGainConfigRef;                 // Referencia az aktív FFT erősítés konfigurációra (AM vagy FM)
    AudioProcessor* pAudioProcessor;               // Pointer az audio feldolgozó osztályra (dinamikus FFT mérettel)

    // Pufferek a különböző módokhoz
    int Rpeak[MiniAudioFftConstants::LOW_RES_BANDS + 1];  // Csúcsértékek az alacsony felbontású spektrumhoz
    std::vector<std::vector<uint8_t>> wabuf;              // Vízeséshez és burkológörbéhez, méretezése a konstruktorban történik
    // Az osciSamples mostantól az AudioProcessor része
    TFT_eSprite sprGraph;  // Sprite a grafikonokhoz (Waterfall, TuningAid)
    bool spriteCreated;    // Jelzi, hogy a sprGraph létre van-e hozva

    int highResOffset;                     // Magas felbontású spektrum eltolásához (FFT.ino 'offset')
    float envelope_prev_smoothed_max_val;  // Előző simított maximális amplitúdó az Envelope módhoz
    int indicatorFontHeight_;              // A módkijelzőhöz használt font magassága

    float currentTuningAidMinFreqHz_;  // Dinamikusan számolt min frekvencia a TuningAid-hez
    float currentTuningAidMaxFreqHz_;  // Dinamikusan számolt max frekvencia a TuningAid-hez

    TuningAidType currentTuningAidType_;  // Aktuális hangolási segéd típus (CW / RTTY / OFF_DECODER)

    // Belső segédfüggvények
    /**
     * @brief Vált a következő megjelenítési módra.
     */
    void cycleMode();
    /**
     * @brief Kirajzolja az aktuális mód nevét a komponens aljára.
     */
    void drawModeIndicator();
    /**
     * @brief Letörli a komponens teljes területét.
     */
    void clearArea();

    // Az egyes módok kirajzoló függvényei
    void drawSpectrumLowRes();
    void drawSpectrumHighRes();
    void drawOscilloscope();
    void drawWaterfall();
    void drawTuningAid();  // Hangolást segítő mód kirajzolása
    void drawEnvelope();

    // Segédfüggvények az alacsony felbontású spektrumhoz
    uint8_t getBandVal(int fft_bin_index, int min_bin_low_res, int num_bins_low_res_range);
    void drawSpectrumBar(int band_idx, double magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode, int current_bar_width_pixels);

    // Segédfüggvény a vízesés/burkológörbe színeihez
    uint16_t valueToWaterfallColor(int scaled_value);

    // Segédfüggvények a grafikon és a módkijelző területének meghatározásához
    /**
     * @brief Visszaadja a módkijelző területének magasságát pixelekben.
     * A számítás a `drawModeIndicator`-ban használt fonton alapul.
     * @return A módkijelző területének magassága.
     */
    int getIndicatorAreaHeight() const;
    /**
     * @brief Visszaadja a grafikonok számára rendelkezésre álló effektív magasságot.
     * Ez a komponens teljes magassága mínusz a módkijelzőnek fenntartott terület.
     * @return A grafikonok rajzolási magassága.
     */
    int getGraphHeight() const;
    /**
     * @brief Visszaadja a módkijelző területének Y kezdőpozícióját a komponensen belül.
     * @return A módkijelző Y kezdőpozíciója.
     */
    int getIndicatorAreaY() const;
    /**
     * @brief Visszaadja a komponens aktuális effektív magasságát, figyelembe véve a módkijelző láthatóságát.
     * @return Az effektív magasság pixelekben.
     */
    int getEffectiveHeight() const;
    void manageSpriteForMode(DisplayMode modeToPrepareFor);
    void drawOffStatusInCenter();  // ÚJ: "Off" státusz kirajzolása középre

    void drawMuted();

    // Segédfüggvény: frekvencia skála címkék rajzolása
    void drawFrequencyScaleLabels(float min_freq, float max_freq, int x0, int y, int w, bool show);
};

#endif  // MINI_AUDIO_FFT_H
