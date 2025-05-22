#ifndef AUDIO_ANALYZER_DISPLAY_H
#define AUDIO_ANALYZER_DISPLAY_H

#include "AudioProcessor.h"
#include "DisplayBase.h"

// Konstansok a képernyőhöz és az FFT-hez
namespace AudioAnalyzerConstants {
// Kijelzési paraméterek
constexpr float ANALYZER_MIN_FREQ_HZ = 300.0f;
constexpr float ANALYZER_MAX_FREQ_HZ = 15000.0f;
constexpr float AMPLITUDE_SCALE = 1500.0f;  // Skálázási faktor a vizuális megjelenítéshez (AudioProcessor után)

// Vízesés elrendezési konstansok
constexpr uint16_t WATERFALL_TOP_Y = 20;         // A vízesés diagram tetejének Y koordinátája (a státuszsor alatt)
constexpr uint16_t ANALYZER_BOTTOM_MARGIN = 20;  // Alsó margó a skálának és a vízesésnek
}  // namespace AudioAnalyzerConstants

class AudioAnalyzerDisplay : public DisplayBase {
   public:
    AudioAnalyzerDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band, float& audioAnalyzerGainConfigRef);
    ~AudioAnalyzerDisplay();

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

    AudioProcessor* pAudioProcessor;     // Pointer az AudioProcessor-ra
    float& audioAnalyzerGainConfigRef_;  // Referencia a gain configra

    // Vízesés/Analizátor Kijelző Változók
    int current_y_analyzer;  // Az aktuális Y pozíció a vízesés vonalának rajzolásához

    // Segédfüggvények
    void FFTSampleAnalyzer();
    void audioScaleAnalyzer(uint16_t occupiedBottomHeight);  // Paraméter hozzáadva
    uint16_t valueToWaterfallColorAnalyzer(float val, float min_val, float max_val, byte colorProfileIndex);
};

#endif  // AUDIO_ANALYZER_DISPLAY_H
