#include "SstvDisplay.h"

#include <algorithm>  // std::min miatt
#include <cmath>      // round, fabs stb.

#include "utils.h"  // Utils::beepError() stb.

SstvDisplay::SstvDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band) : DisplayBase(tft, si4735, band) { DEBUG("SstvDisplay::SstvDisplay\n"); }

SstvDisplay::~SstvDisplay() {}

void SstvDisplay::drawScreen() {
    tft.fillScreen(TFT_COLOR_BACKGROUND);

    DisplayBase::BuildButtonData exitButtonData[] = {
        {"Exit", TftButton::ButtonType::Pushable, TftButton::ButtonState::Off},
    };
    DisplayBase::buildHorizontalScreenButtons(exitButtonData, ARRAY_ITEM_COUNT(exitButtonData), false);

    TftButton* exitButton = findButtonByLabel("Exit");
    if (exitButton != nullptr) {
        uint16_t exitButtonX = tft.width() - SCREEN_HBTNS_X_START - SCRN_BTN_W;
        uint16_t exitButtonY = getAutoButtonPosition(ButtonOrientation::Horizontal, 0, false);
        exitButton->setPosition(exitButtonX, exitButtonY);
    }
    drawScreenButtons();
}

// A displayLoop most már csak a processAudioAndDecode-ot hívja
void SstvDisplay::displayLoop() {
    if (pDialog != nullptr) {
        return;
    }
}

bool SstvDisplay::handleRotary(RotaryEncoder::EncoderState encoderState) {
    // SSTV képernyőn a rotary encodernek egyelőre nincs funkciója
    // Később lehetne pl. kézi szinkron finomhangolás, módválasztás stb.
    return false;
}

bool SstvDisplay::handleTouch(bool touched, uint16_t tx, uint16_t ty) {
    // SSTV képernyőn a direkt érintésnek (nem gomb) egyelőre nincs funkciója
    // Később lehetne pl. kép mentése, zoom stb.
    return false;
}

void SstvDisplay::processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) {
    if (STREQ("Exit", event.label)) {
        ::newDisplay = DisplayBase::DisplayType::am;  // Vagy az előző képernyőre, ha tároljuk
    }
}
