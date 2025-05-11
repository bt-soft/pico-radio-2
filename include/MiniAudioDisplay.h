#ifndef MINI_AUDIO_DISPLAY_H
#define MINI_AUDIO_DISPLAY_H

#include <ArduinoFFT.h>  // FFT könyvtár

#include "DisplayBase.h"
#include "defines.h"  // PIN_AUDIO_INPUT_PIN

// Konstansok a mini kijelzőhöz és az FFT-hez
namespace MiniAudioDisplayConstants {
// FFT Konfiguráció (az FFT.ino-hoz hasonlóan)
constexpr uint16_t FFT_SAMPLES = 256;          // Minták száma az FFT-hez (2 hatványa kell legyen)
constexpr double SAMPLING_FREQUENCY = 10000;   // Mintavételezési frekvencia Hz-ben (az FFT.ino-ban 25000 is volt, de a többi analizátor 10000-et használ)
constexpr float MINI_AMPLITUDE_SCALE = 200.0;  // Skálázási faktor a mini kijelzőkhöz (az FFT.ino 'amplitude'-hoz hasonlóan, kísérletezést igényel)

// Mini kijelző területének definíciója
constexpr int MINI_DISPLAY_AREA_X = 20;
constexpr int MINI_DISPLAY_AREA_Y = 50;   // Státuszsor és cím alatt
constexpr int MINI_DISPLAY_AREA_W = 280;  // Kijelző szélesség - 2*margó
constexpr int MINI_DISPLAY_AREA_H = 100;  // Magasság a mini kijelzőnek

// Mini vízeséshez (FFT.ino alapján)
constexpr int MINI_WF_WIDTH = 84;
constexpr int MINI_WF_HEIGHT = 24;
constexpr int MINI_WF_GRADIENT = 100;

// Alacsony felbontású spektrumhoz (FFT.ino alapján)
constexpr int LOW_RES_BANDS = 16;
constexpr int LOW_RES_PEAK_MAX_HEIGHT = 23;  // dmax az FFT.ino-ban

// Magas felbontású spektrumhoz
constexpr int HIGH_RES_BINS_TO_DISPLAY = 85;  // SAMPLES / 3 az FFT.ino-ban

// Oszcilloszkóphoz
constexpr int MINI_OSCI_SAMPLES_TO_DRAW = 86;  // Az FFT.ino-ban ennyit rajzol
}  // namespace MiniAudioDisplayConstants

class MiniAudioDisplay : public DisplayBase {
   public:
    MiniAudioDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band);
    ~MiniAudioDisplay() override;

    void drawScreen() override;
    void displayLoop() override;
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) override;

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayType::miniAudioAnalyzer; }

    void setPrevDisplayType(DisplayBase::DisplayType prev) override { this->prevDisplay = prev; };

   private:
    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;

    // Mini kijelző módja (0:off, 1:low-res, 2:high-res, 3:osci, 4:waterfall, 5:envelope)
    uint8_t currentMiniWindowMode;
    bool audioMutedState;  // Csak a "MUTED" felirat megjelenítéséhez

    // FFT Változók
    ArduinoFFT<double> FFT;
    double vReal[MiniAudioDisplayConstants::FFT_SAMPLES];   // Bemenet az FFT-hez
    double vImag[MiniAudioDisplayConstants::FFT_SAMPLES];   // Képzetes rész
    double RvReal[MiniAudioDisplayConstants::FFT_SAMPLES];  // FFT kimenet (magnitúdók)

    // Alacsony felbontású spektrumhoz (mode 1)
    int Rpeak[MiniAudioDisplayConstants::LOW_RES_BANDS + 1];  // Peak hold értékek

    // Mini vízeséshez és burkológörbéhez (mode 4, 5)
    int wabuf[MiniAudioDisplayConstants::MINI_WF_HEIGHT][MiniAudioDisplayConstants::MINI_WF_WIDTH];

    // Magas felbontású spektrumhoz (mode 2)
    int highResOffset;  // Az FFT.ino 'offset' változója

    // Segédfüggvények
    void cycleMiniWindowMode();
    void drawModeIndicator();
    void clearMiniDisplayArea();

    void FFTSampleMini(bool drawOsci);  // Adaptált FFT mintavételező

    // Az egyes módok kirajzoló függvényei
    void drawMiniSpectrumLowRes();
    void drawMiniSpectrumHighRes();
    void drawMiniOscilloscope();  // Az FFTSampleMini-n belül is lehet
    void drawMiniWaterfall();
    void drawMiniEnvelope();

    // Segédfüggvények az FFT.ino-ból (adaptálva)
    uint8_t getBandValMini(int fft_bin_index);
    void displayBandMini(int band_index, int magnitude, int actual_start_x); // Harmadik paraméter hozzáadva
    uint16_t valueToMiniWaterfallColor(int scaled_value);  // A normalizált bemenetű helyett
};

#endif  // MINI_AUDIO_DISPLAY_H
