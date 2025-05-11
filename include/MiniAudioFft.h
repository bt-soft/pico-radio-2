#ifndef MINI_AUDIO_FFT_H
#define MINI_AUDIO_FFT_H

#include <ArduinoFFT.h>
#include <TFT_eSPI.h>

#include <vector>  // For std::vector

#include "defines.h"  // For AUDIO_INPUT_PIN and colors

// Constants for MiniAudioFft (adapted from MiniAudioDisplayConstants)
namespace MiniAudioFftConstants {
constexpr uint16_t FFT_SAMPLES = 256;
constexpr double SAMPLING_FREQUENCY = 10000;
constexpr float AMPLITUDE_SCALE = 200.0f;

// Max dimensions for internal array sizing if component size is larger.
// Actual drawing is clipped/scaled to component's w,h.
constexpr int MAX_INTERNAL_WIDTH = 86;
constexpr int MAX_INTERNAL_HEIGHT = 24;

constexpr int LOW_RES_BANDS = 16;
// HIGH_RES_BINS_TO_DISPLAY, OSCI_SAMPLES_TO_DRAW will be component width
// WF_WIDTH and WF_HEIGHT will be component width and height
constexpr int WF_GRADIENT = 100;
constexpr float ENVELOPE_INPUT_GAIN = 50.0f;
constexpr float ENVELOPE_SMOOTH_FACTOR = 0.25f; // Hiányzó konstans hozzáadva
constexpr float ENVELOPE_THICKNESS_SCALER = 0.95f;
constexpr float OSCI_SENSITIVITY_FACTOR = 3.0f;
constexpr int OSCI_SAMPLE_DECIMATION_FACTOR = 2;

// Colors for waterfall (can be a separate namespace if preferred)
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
constexpr int MAX_WATERFALL_COLOR_INPUT_VALUE = 20000;

}  // namespace MiniAudioFftConstants

class MiniAudioFft {
   public:
    MiniAudioFft(TFT_eSPI& tft_ref, int x, int y, int w, int h);
    ~MiniAudioFft() = default;

    void loop();  // Handles FFT sampling and drawing
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty);
    void forceRedraw();  // Forces a full redraw of the current mode

   private:
    TFT_eSPI& tft;
    int posX, posY, width, height;  // Component position and dimensions

    uint8_t currentMode;  // 0:off, 1:low-res, 2:high-res, 3:osci, 4:waterfall, 5:envelope
    // bool audioMutedState; // Not actively used for now, but kept for potential future use

    ArduinoFFT<double> FFT;
    double vReal[MiniAudioFftConstants::FFT_SAMPLES];
    double vImag[MiniAudioFftConstants::FFT_SAMPLES];
    double RvReal[MiniAudioFftConstants::FFT_SAMPLES];  // FFT magnitudes

    // Buffers for different modes
    int Rpeak[MiniAudioFftConstants::LOW_RES_BANDS + 1];
    std::vector<std::vector<int>> wabuf;  // For waterfall and envelope, sized in constructor
    int osciSamples[MiniAudioFftConstants::MAX_INTERNAL_WIDTH];
    int highResOffset;

    // Internal helper methods
    void cycleMode();
    void drawModeIndicator();  // Draws mode text within the component
    void clearArea();          // Clears the component's area

    void performFFT(bool collectOsciSamples);

    // Drawing methods for each mode
    void drawSpectrumLowRes();
    void drawSpectrumHighRes();
    void drawOscilloscope();
    void drawWaterfall();
    void drawEnvelope();

    // Helper for LowRes spectrum
    uint8_t getBandVal(int fft_bin_index);
    void displayBand(int band_idx, int magnitude, int actual_start_x_on_screen, int peak_max_height_for_mode);
    // Helper for Waterfall/Envelope color
    uint16_t valueToWaterfallColor(int scaled_value);
};

#endif  // MINI_AUDIO_FFT_H
