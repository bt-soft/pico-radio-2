#ifndef __SETUPDISPLAY_H
#define __SETUPDISPLAY_H

#include "DisplayBase.h"
#include "IScrollableListDataSource.h"
#include "ScrollableListComponent.h"

namespace SetupList {
enum class ItemAction {
    BRIGHTNESS,
    INFO,
    SQUELCH_BASIS,
    SAVER_TIMEOUT,
    INACTIVE_DIGIT_LIGHT,
    BEEPER_ENABLED,
    FFT_CONFIG_AM,       // Új, összevont
    FFT_CONFIG_FM,       // Új, összevont
    CW_RECEIVER_OFFSET,  // CW vételi eltolás
    RTTY_FREQUENCIES,    // RTTY Mark és Shift frekvenciák
    FACTORY_RESET,
    NONE
};

struct SettingItem {
    const char *label;
    ItemAction action;
};
}  // namespace SetupList

class SetupDisplay : public DisplayBase, public IScrollableListDataSource {

   private:
    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;

    // Lista alapú menühöz
    static const int MAX_SETTINGS = 11;  // RTTY_FREQUENCIES hozzáadva
    SetupList::SettingItem settingItems[MAX_SETTINGS];
    ScrollableListComponent scrollListComponent;
    bool nestedDialogOpened = false;           // Beágyazott dialógus nyitva van-e?
    DialogBase *pendingCloseDialog = nullptr;  // Az előző dialógus, amit be kell zárni beágyazott nyitáskor

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

    /**
     * Dialóg Button touch esemény feldolgozása
     */
    void processDialogButtonResponse(TftButton::ButtonTouchEvent &event) override;

    // IScrollableListDataSource implementáció
    int getItemCount() const override;
    void drawListItem(TFT_eSPI &tft_ref, int index, int x, int y, int w, int h, bool isSelected) override;
    void activateListItem(int index) override;
    int getItemHeight() const override;
    int loadData() override;

   public:
    SetupDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);
    ~SetupDisplay();
    void drawScreen() override;
    void displayLoop() override {};

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::setup; };

    void setPrevDisplayType(DisplayType prev) { prevDisplay = prev; }

   private:
    void drawSettingsList();
    void drawSettingItem(int itemIndex, int yPos, bool isSelected);
    void activateSetting(SetupList::ItemAction action, int itemIndex);  // itemIndex hozzáadva a callback-hez
    void updateSelection(int newIndex, bool fromRotary);
};

#endif  // __SETUPDISPLAY_H
