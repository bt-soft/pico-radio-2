#ifndef MINI_AUDIO_FFT_H
#define MINI_AUDIO_FFT_H

#include <ArduinoFFT.h>
#include <TFT_eSPI.h>

#include <vector>  // std::vector használatához

#include "defines.h"  // AUDIO_INPUT_PIN és színek eléréséhez

// A 10kHz-es SAMPLING_FREQUENCY miatt az FFT eljárás 0 Hz-től 5 kHz-ig terjedő sávszélességben képes mérni a frekvenciakomponenseket.
// A grafikus megjelenítők ebből tipikusan a kb. 78 Hz-től 4.96 kHz-ig terjedő tartományt ábrázolják.


// Konstansok a MiniAudioFft komponenshez 
namespace MiniAudioFftConstants {
constexpr uint16_t FFT_SAMPLES = 256;         // Minták száma az FFT-hez (2 hatványának kell lennie)
constexpr double SAMPLING_FREQUENCY = 10000;  // Mintavételezési frekvencia Hz-ben
constexpr float AMPLITUDE_SCALE = 200.0f;     // Skálázási faktor az FFT eredményekhez

// Belső tömbök maximális méretei, ha a komponens mérete nagyobb lenne.
// A tényleges rajzolás a komponens w,h méreteihez van vágva/skálázva.
constexpr int MAX_INTERNAL_WIDTH = 86;  // Oszcilloszkóp és magas felb. spektrum belső bufferéhez
constexpr int MAX_INTERNAL_HEIGHT = 80;
constexpr int LOW_RES_BANDS = 16;  // Alacsony felbontású spektrum sávjainak száma
// A HIGH_RES_BINS_TO_DISPLAY és OSCI_SAMPLES_TO_DRAW a komponens aktuális szélességéből adódik.
// A WF_WIDTH és WF_HEIGHT a komponens aktuális szélességéből és magasságából (csökkentve a kijelzővel) adódik.

constexpr int WF_GRADIENT = 100;                    // Vízesés színátmenetének erőssége
constexpr float ENVELOPE_INPUT_GAIN = 50.0f;        // Erősítési faktor a burkológörbe bemenetéhez
constexpr float ENVELOPE_SMOOTH_FACTOR = 0.25f;     // Simítási faktor a burkológörbéhez
constexpr float ENVELOPE_THICKNESS_SCALER = 0.95f;  // Burkológörbe vastagságának skálázója
constexpr float OSCI_SENSITIVITY_FACTOR = 3.0f;     // Oszcilloszkóp érzékenységi faktora
constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;    // Oszcilloszkóp mintavételi decimációs faktora

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
};
constexpr int MAX_WATERFALL_COLOR_INPUT_VALUE = 20000;  // Maximális bemeneti érték a vízesés színkonverziójához

}  // namespace MiniAudioFftConstants

class MiniAudioFft {
   public:
    /**
     * @brief Konstruktor.
     * @param tft_ref Referencia a TFT_eSPI objektumra.
     * @param x A komponens bal felső sarkának X koordinátája.
     * @param y A komponens bal felső sarkának Y koordinátája.
     * @param w A komponens szélessége.
     * @param h A komponens magassága.
     */
    MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h);
    ~MiniAudioFft() = default;  // Alapértelmezett destruktor

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

   private:
    TFT_eSPI& tft;                  // Referencia a TFT objektumra
    int posX, posY, width, height;  // Komponens pozíciója és méretei

    uint8_t currentMode;  // Aktuális megjelenítési mód (0:ki, 1:low-res, 2:high-res, 3:osci, 4:waterfall, 5:envelope)
    bool prevMuteState;   // Előző némítási állapot a változások érzékeléséhez

    ArduinoFFT<double> FFT;                             // FFT objektum
    double vReal[MiniAudioFftConstants::FFT_SAMPLES];   // Valós rész az FFT bemenetéhez/kimenetéhez
    double vImag[MiniAudioFftConstants::FFT_SAMPLES];   // Képzetes rész az FFT-hez
    double RvReal[MiniAudioFftConstants::FFT_SAMPLES];  // FFT magnitúdók tárolására (az FFT.ino alapján)

    // Pufferek a különböző módokhoz
    int Rpeak[MiniAudioFftConstants::LOW_RES_BANDS + 1];         // Csúcsértékek az alacsony felbontású spektrumhoz
    std::vector<std::vector<int>> wabuf;                         // Vízeséshez és burkológörbéhez, méretezése a konstruktorban történik
    int osciSamples[MiniAudioFftConstants::MAX_INTERNAL_WIDTH];  // Oszcilloszkóp minták
    int highResOffset;                                           // Magas felbontású spektrum eltolásához (FFT.ino 'offset')

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
    /**
     * @brief Elvégzi az FFT mintavételezést és számítást.
     * @param collectOsciSamples Igaz, ha az oszcilloszkóp számára is kell mintákat gyűjteni.
     */
    void performFFT(bool collectOsciSamples);

    // Az egyes módok kirajzoló függvényei
    void drawSpectrumLowRes();
    void drawSpectrumHighRes();
    void drawOscilloscope();
    void drawWaterfall();
    void drawEnvelope();

    // Segédfüggvények az alacsony felbontású spektrumhoz
    uint8_t getBandVal(int fft_bin_index);
    void displayBand(int band_idx, int magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode);
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

    void drawMuted();
};

#endif  // MINI_AUDIO_FFT_H
