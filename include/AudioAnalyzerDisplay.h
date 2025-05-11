#ifndef AUDIO_ANALYZER_DISPLAY_H
#define AUDIO_ANALYZER_DISPLAY_H

#include <arduinoFFT.h>  // FFT könyvtár

#include "DisplayBase.h"
#include "defines.h"

// Konstansok a képernyőhöz és az FFT-hez
namespace AudioAnalyzerConstants {
constexpr uint16_t FFT_SAMPLES = 512;         // Minták száma az FFT-hez (2 hatványa kell legyen)
constexpr double SAMPLING_FREQUENCY = 10000;  // Mintavételezési frekvencia Hz-ben

constexpr int FFT_START_BIN_OFFSET = 5;        // Kb. 100Hz-től induljon a kijelzés (5 * (10000/512) = ~97.6Hz)
constexpr int ANALYZER_DISPLAY_NUM_BINS = 64;  // Megjelenítendő FFT bin-ek száma az offset után (pl. 64 bin ~1.25kHz sávszélességet ad 97Hz-től)
constexpr float AMPLITUDE_SCALE = 1000.0;      // Növelt érték az érzékenység csökkentéséhez. Kísérletezz ezzel (pl. 500.0 - 2000.0 tartományban)
constexpr int ANALYZER_BOTTOM_MARGIN = 20;     // Hely a képernyő alján a frekvencia skálának
constexpr int WATERFALL_TOP_Y = 20;            // A vízesés rajzolásának felső Y koordinátája (a státuszsor alatt)
}  // namespace AudioAnalyzerConstants

class AudioAnalyzerDisplay : public DisplayBase {
   public:
    AudioAnalyzerDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band);
    ~AudioAnalyzerDisplay() {};

    void drawScreen() override;
    void displayLoop() override;
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) override;

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayType::audioAnalyzer; }

    /**
     * Az előző képernyőtípus beállítása
     * Itt adjuk át, hogy hova kell visszatérnie a képrnyő bezárása után
     */
    void setPrevDisplayType(DisplayBase::DisplayType prev) override { this->prevDisplay = prev; };

   private:
    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;  // Hova térjünk vissza

    // FFT Változók
    ArduinoFFT<double> FFT;  // Javítva: kis 'a' -> nagy 'A'
    double vReal[AudioAnalyzerConstants::FFT_SAMPLES];
    double vImag[AudioAnalyzerConstants::FFT_SAMPLES];

    // Vízesés/Analizátor Kijelző Változók
    int current_y_analyzer;  // Az aktuális Y pozíció a vízesés vonalának rajzolásához

    // Segédfüggvények
    void FFTSampleAnalyzer();
    void audioScaleAnalyzer(uint16_t occupiedBottomHeight);  // Paraméter hozzáadva
    uint16_t valueToWaterfallColorAnalyzer(float val, float min_val, float max_val, byte colorProfileIndex);
};

#endif  // AUDIO_ANALYZER_DISPLAY_H
