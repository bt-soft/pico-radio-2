#ifndef __SETUPDISPLAY_H
#define __SETUPDISPLAY_H

#include "DisplayBase.h"

namespace SetupList {
enum class ItemAction { BRIGHTNESS, INFO, SQUELCH_BASIS, SAVER_TIMEOUT, INACTIVE_DIGIT_LIGHT, NONE };

struct SettingItem {
    const char *label;
    ItemAction action;
};
}  // namespace SetupList

class SetupDisplay : public DisplayBase {

   private:
    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;

    // Lista alapú menühöz
    static const int MAX_SETTINGS = 5;  // Maximális beállítási elemek száma
    SetupList::SettingItem settingItems[MAX_SETTINGS];
    int itemCount = 0;
    int selectedItemIndex = 0;
    int topItemIndex = 0;  // A lista tetején lévő elem indexe (görgetéshez)

   protected:
    /**
     * Rotary encoder esemény lekezelése
     */
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;

    /**
     * Touch (nem képrnyő button) esemény lekezelése
     * A további gui elemek vezérléséhez
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;

    /**
     * Képernyő menügomb esemény feldolgozása
     */
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) override;

   public:
    SetupDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);
    ~SetupDisplay();
    void drawScreen() override;
    void displayLoop() override;

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::setup; };

    void setPrevDisplayType(DisplayType prev) { prevDisplay = prev; }

   private:
    void drawSettingsList();
    void drawSettingItem(int itemIndex, int yPos, bool isSelected);
    void activateSetting(SetupList::ItemAction action);
    void updateSelection(int newIndex, bool fromRotary);
};

#endif  // __SETUPDISPLAY_H
