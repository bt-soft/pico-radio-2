#ifndef SCROLLABLELISTCOMPONENT_H
#define SCROLLABLELISTCOMPONENT_H

#include <TFT_eSPI.h>

#include "IScrollableListDataSource.h"
#include "RotaryEncoder.h"  // A RotaryEncoder::EncoderState miatt

namespace ScrollableListComponentDefaults {
constexpr uint16_t ITEM_TEXT_COLOR = TFT_WHITE;
constexpr uint16_t ITEM_BG_COLOR = TFT_BLACK;
constexpr uint16_t SELECTED_ITEM_TEXT_COLOR = TFT_BLACK;
constexpr uint16_t SELECTED_ITEM_BG_COLOR = TFT_LIGHTGREY;
constexpr uint16_t LIST_BORDER_COLOR = TFT_DARKGREY;
}  // namespace ScrollableListComponentDefaults

class ScrollableListComponent {
   private:
    TFT_eSPI& tft;
    IScrollableListDataSource* dataSource;

    int listX, listY, listW, listH;
    int itemHeight;  // Egy elem magassága, a dataSource-ból származik
    int visibleItems;

    int selectedItemIndex;
    int topItemIndex;
    int currentItemCount;  // Gyorsítótárazott elemszám

    // Megjelenés
    uint16_t itemBgColor;
    uint16_t listBorderColor;
    // Görgetősáv
    bool scrollbarVisible;
    int scrollbarX;
    int scrollbarWidth;

    void calculateVisibleItems();
    void updateSelection(int newIndex, bool fromRotary);
    void drawScrollbar();
    void clearListArea();
    void drawListBorder();

   public:
    ScrollableListComponent(TFT_eSPI& tft_ref, int x, int y, int w, int h, IScrollableListDataSource* ds, uint16_t bgCol = ScrollableListComponentDefaults::ITEM_BG_COLOR,
                            uint16_t borderCol = ScrollableListComponentDefaults::LIST_BORDER_COLOR);

    void draw();
    bool handleRotaryScroll(RotaryEncoder::EncoderState encoderState);
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty, bool activateOnTouch = true);

    int getSelectedItemIndex() const;
    void setSelectedItemIndex(int index);  // Lehetővé teszi a szülő számára a kiválasztás beállítását
    void refresh();                        // Újratölti az adatokat és újrarajzol

    // Lehetővé teszi a szülő számára az aktiválás indítását (pl. dupla koppintás után)
    void activateSelectedItem();
};

#endif  // SCROLLABLELISTCOMPONENT_H
