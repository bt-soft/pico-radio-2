#include "ScrollableListComponent.h"

#include <algorithm>  // std::min és std::max miatt

namespace ScrollbarConstants {
constexpr int SCROLLBAR_WIDTH = 5;    // Görgetősáv szélessége pixelben
constexpr int SCROLLBAR_PADDING = 2;  // Távolság a lista szélétől
}  // namespace ScrollbarConstants

ScrollableListComponent::ScrollableListComponent(TFT_eSPI& tft_ref, int x, int y, int w, int h, IScrollableListDataSource* ds, uint16_t bgCol, uint16_t borderCol)
    : tft(tft_ref),
      dataSource(ds),
      listX(x),
      listY(y),
      listW(w),
      listH(h),
      itemHeight(0),
      visibleItems(0),
      selectedItemIndex(-1),
      topItemIndex(0),
      currentItemCount(0),
      itemBgColor(bgCol),
      listBorderColor(borderCol),
      scrollbarVisible(false),  // Kezdetben nem látható
      scrollbarX(0),
      scrollbarWidth(ScrollbarConstants::SCROLLBAR_WIDTH) {
    if (!dataSource) {
        // Hiba kezelése: a dataSource nem lehet null
        return;
    }
    itemHeight = dataSource->getItemHeight();
    scrollbarX = listX + listW + ScrollbarConstants::SCROLLBAR_PADDING;  // Lista jobb oldalán
    calculateVisibleItems();
}

void ScrollableListComponent::calculateVisibleItems() {
    if (itemHeight > 0) {
        visibleItems = listH / itemHeight;
    } else {
        visibleItems = 0;  // Osztás nullával elkerülése
    }
    if (visibleItems <= 0) visibleItems = 1;  // Biztosítjuk, hogy legalább egy elem látható legyen, ha a listH nagyon kicsi
}

void ScrollableListComponent::clearListArea() { tft.fillRect(listX, listY, listW, listH, itemBgColor); }

void ScrollableListComponent::drawListBorder() {
    tft.drawRect(listX - 1, listY - 1, listW + 2, listH + 2, listBorderColor);  // -1 és +2, hogy a lista területén kívülre rajzoljon
}

void ScrollableListComponent::refresh() {
    if (!dataSource) return;
    int newSelectedItem = dataSource->loadData(); // Adatok betöltése és a javasolt kiválasztott index lekérése
    currentItemCount = dataSource->getItemCount();
    itemHeight = dataSource->getItemHeight();  // Újra lekérjük, hátha változott
    calculateVisibleItems();

    // A dataSource által javasolt indexet használjuk, ha érvényes,
    // egyébként a meglévő logikát a selectedItemIndex érvényesítésére.
    if (newSelectedItem >= 0 && newSelectedItem < currentItemCount) {
        selectedItemIndex = newSelectedItem;
    } else {
        // Meglévő logika a selectedItemIndex érvényesítésére
        if (selectedItemIndex >= currentItemCount) {
            selectedItemIndex = currentItemCount > 0 ? currentItemCount - 1 : -1;
        }
        if (selectedItemIndex < 0 && currentItemCount > 0) {
            selectedItemIndex = 0;  // Alapértelmezetten az első elem, ha nincs kiválasztva és a lista nem üres
        }
    }

    // A topItemIndex igazítása, hogy a selectedItem látható legyen
    if (topItemIndex > std::max(0, currentItemCount - visibleItems)) {  // Biztosítjuk, hogy a topItemIndex ne legyen túl magas
        topItemIndex = std::max(0, currentItemCount - visibleItems);
    }

    if (selectedItemIndex != -1) {  // Csak akkor igazítunk, ha van kiválasztott elem
        if (selectedItemIndex < topItemIndex) {
            topItemIndex = selectedItemIndex;
        } else if (selectedItemIndex >= topItemIndex + visibleItems) {
            topItemIndex = selectedItemIndex - visibleItems + 1;
        }
        // Biztosítjuk, hogy a topItemIndex az igazítás után is érvényes határokon belül legyen
        if (topItemIndex < 0) topItemIndex = 0;
        topItemIndex = std::min(topItemIndex, std::max(0, currentItemCount - visibleItems));
    }
    scrollbarVisible = (currentItemCount > visibleItems);  // Akkor látható, ha van mit görgetni
    draw();
}

void ScrollableListComponent::draw() {
    if (!dataSource) return;

    clearListArea();
    // A görgetősáv területét is törölni kellene, ha a lista háttérszíne eltér a scrollbar háttérszínétől

    if (currentItemCount == 0) {
        // A üres üzenet kirajzolása
        tft.setFreeFont();
        tft.setTextSize(2);
        tft.setTextColor(ScrollableListComponentDefaults::ITEM_TEXT_COLOR, itemBgColor);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Empty List", listX + listW / 2, listY + listH / 2);
        drawListBorder();
        scrollbarVisible = false;  // Nincs elem, nincs scrollbar
        return;
    }

    int currentY = listY;
    for (int i = 0; i < visibleItems; ++i) {
        int actualItemIndex = topItemIndex + i;
        if (actualItemIndex < currentItemCount) {
            dataSource->drawListItem(tft, actualItemIndex, listX, currentY, listW, itemHeight, actualItemIndex == selectedItemIndex);
        } else {
            // Üres sorok kitöltése, ha kevesebb elem van, mint látható hely
            tft.fillRect(listX, currentY, listW, itemHeight, itemBgColor);
        }
        currentY += itemHeight;
    }
    scrollbarVisible = (currentItemCount > visibleItems);  // Frissítjük a láthatóságot
    drawListBorder();
    if (scrollbarVisible) {
        drawScrollbar();
    }
}

void ScrollableListComponent::drawScrollbar() {
    if (!scrollbarVisible || itemHeight == 0) {  // Ha nem látható, vagy nincs elem magasság, nem rajzolunk
        // Esetleg töröljük a régi scrollbart, ha volt
        tft.fillRect(scrollbarX, listY, scrollbarWidth, listH, itemBgColor);  // Törlés a lista háttérszínével
        return;
    }

    // Görgetősáv háttere (opcionális, ha eltér a lista hátterétől)
    // tft.fillRect(scrollbarX, listY, scrollbarWidth, listH, ScrollableListComponentDefaults::LIST_BORDER_COLOR); // Példa: sötétszürke háttér

    // Pöcök magasságának kiszámítása
    // Arányos a látható elemek számával a teljes elemszámhoz képest
    int thumbHeight = (static_cast<float>(visibleItems) / currentItemCount) * listH;
    if (thumbHeight < 5) thumbHeight = 5;          // Minimális pöcök magasság
    if (thumbHeight > listH) thumbHeight = listH;  // Maximális pöcök magasság

    // Pöcök Y pozíciójának kiszámítása
    // Arányos a topItemIndex-szel a görgethető elemek számához képest
    int scrollableItems = currentItemCount - visibleItems;
    int thumbY = listY;
    if (scrollableItems > 0) {
        thumbY += (static_cast<float>(topItemIndex) / scrollableItems) * (listH - thumbHeight);
    }
    // Biztosítjuk, hogy a pöcök a sávon belül maradjon
    thumbY = std::max(listY, std::min(thumbY, listY + listH - thumbHeight));

    // Először töröljük a scrollbar teljes területét a lista háttérszínével (vagy egy dedikált scrollbar háttérszínnel)
    tft.fillRect(scrollbarX, listY, scrollbarWidth, listH, itemBgColor);  // Vagy egy másik háttérszín
    // Pöcök kirajzolása
    tft.fillRect(scrollbarX, thumbY, scrollbarWidth, thumbHeight, ScrollableListComponentDefaults::SELECTED_ITEM_BG_COLOR);  // Pöcök színe lehet pl. a kiválasztott elem háttere
}

void ScrollableListComponent::updateSelection(int newIndex, bool fromRotary) {
    if (!dataSource || currentItemCount == 0) {
        selectedItemIndex = -1;  // Nincs elem, nincs kiválasztás
        topItemIndex = 0;
        return;
    }

    if (newIndex < 0 || newIndex >= currentItemCount) return;  // Érvénytelen index

    int oldSelectedItemIndex = selectedItemIndex;
    int oldTopItemIndex = topItemIndex;
    selectedItemIndex = newIndex;

    // Görgetési pozíció (topItemIndex) igazítása, ha szükséges
    if (selectedItemIndex < topItemIndex) {
        topItemIndex = selectedItemIndex;
    } else if (selectedItemIndex >= topItemIndex + visibleItems) {
        topItemIndex = selectedItemIndex - visibleItems + 1;
    }
    // Biztosítjuk, hogy a topItemIndex érvényes határokon belül legyen
    if (topItemIndex < 0) topItemIndex = 0;
    topItemIndex = std::min(topItemIndex, std::max(0, currentItemCount - visibleItems));

    if (oldTopItemIndex != topItemIndex) {
        draw();                                              // Teljes lista újrarajzolása, ha görgetődött
    } else if (oldSelectedItemIndex != selectedItemIndex) {  // Csak akkor, ha a kiválasztás tényleg változott
        // Ha a topItemIndex nem változott, de a selectedItemIndex igen,
        // akkor csak a régi és az új kiválasztott elemet kell újrarajzolni.
        // Régi elem kijelölésének megszüntetése
        if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < currentItemCount && oldSelectedItemIndex >= oldTopItemIndex &&
            oldSelectedItemIndex < oldTopItemIndex + visibleItems) {
            dataSource->drawListItem(tft, oldSelectedItemIndex, listX, listY + (oldSelectedItemIndex - topItemIndex) * itemHeight, listW, itemHeight, false);
        }
        // Új elem kijelölése
        if (selectedItemIndex >= topItemIndex && selectedItemIndex < topItemIndex + visibleItems) {
            dataSource->drawListItem(tft, selectedItemIndex, listX, listY + (selectedItemIndex - topItemIndex) * itemHeight, listW, itemHeight, true);
        }
        // A görgetősávot is frissíteni kell, ha látható, mert a pöcök pozíciója a topItemIndex-től függ,
        // de a kiválasztás változása miatt is érdemes lehet frissíteni, ha pl. a pöcök színe a kiválasztástól függne.
        // Jelen esetben a topItemIndex nem változott, így a pöcök pozíciója sem, de a láthatóságát ellenőrizzük.
        if (scrollbarVisible) {
            drawScrollbar();
        }
    } else if (oldSelectedItemIndex != selectedItemIndex) {
        // Csak a megváltozott elemek újrarajzolása, ha nem görgetődött
        // Régi elem kijelölésének megszüntetése
        if (oldSelectedItemIndex >= 0 && oldSelectedItemIndex < currentItemCount && oldSelectedItemIndex >= oldTopItemIndex &&
            oldSelectedItemIndex < oldTopItemIndex + visibleItems) {  // Ellenőrizzük, hogy a régi elem látható volt-e
            dataSource->drawListItem(tft, oldSelectedItemIndex, listX, listY + (oldSelectedItemIndex - topItemIndex) * itemHeight, listW, itemHeight, false);
        }
        // Új elem kijelölése
        if (selectedItemIndex >= topItemIndex && selectedItemIndex < topItemIndex + visibleItems) {  // Ellenőrizzük, hogy az új elem látható-e
            dataSource->drawListItem(tft, selectedItemIndex, listX, listY + (selectedItemIndex - topItemIndex) * itemHeight, listW, itemHeight, true);
        }
    }
    // Biztosítjuk, hogy a scrollbar láthatósága frissüljön
    if (scrollbarVisible != (currentItemCount > visibleItems)) {
        draw();  // Ha a scrollbar láthatósága változott, teljes újrarajzolás
    }
}

/**
 * Rotary encoder esemény lekezelése
 */
bool ScrollableListComponent::handleRotaryScroll(RotaryEncoder::EncoderState encoderState) {
    if (!dataSource || currentItemCount == 0) return false;

    int newIdx = selectedItemIndex == -1 && currentItemCount > 0 ? 0 : selectedItemIndex;  // 0-ról indulunk, ha nincs kiválasztva semmi
    bool changed = false;

    if (encoderState.direction == RotaryEncoder::Direction::Up) {  // Vizuálisan felfelé, index csökken
        if (newIdx > 0) {
            newIdx--;
            changed = true;
        }
    } else if (encoderState.direction == RotaryEncoder::Direction::Down) {  // Vizuálisan lefelé, index nő
        if (newIdx < currentItemCount - 1) {
            newIdx++;
            changed = true;
        }
    }

    if (changed) {
        updateSelection(newIdx, true);
    }
    return changed;
}

/**
 * Képernyő érintés esemény lekezelése
 */
bool ScrollableListComponent::handleTouch(bool touched, uint16_t tx, uint16_t ty, bool activateOnTouch) {
    if (!dataSource || currentItemCount == 0 || !touched) return false;

    if (tx >= listX && tx < (listX + listW) && ty >= listY && ty < (listY + listH)) {
        int touchedItemRelative = (ty - listY) / itemHeight;
        int touchedItemAbsolute = topItemIndex + touchedItemRelative;

        if (touchedItemAbsolute >= 0 && touchedItemAbsolute < currentItemCount) {
            updateSelection(touchedItemAbsolute, false);
            if (activateOnTouch && selectedItemIndex == touchedItemAbsolute) {  // Biztosítjuk, hogy a kiválasztás sikeres volt
                dataSource->activateListItem(selectedItemIndex);
            }
            return true;
        }
    }
    return false;
}

/**
 *  Kiválasztott elem indexének beállítása
 */
void ScrollableListComponent::setSelectedItemIndex(int index) {
    // Biztosítjuk, hogy az index érvényes legyen, vagy -1 a nincs kiválasztás esetén
    if (index >= -1 && index < currentItemCount) {
        updateSelection(index, false);  // Nem forgókódolós frissítésként kezeljük
    }
}

/**
 * Kiválasztott elem aktiválása
 */
void ScrollableListComponent::activateSelectedItem() {
    if (dataSource && selectedItemIndex >= 0 && selectedItemIndex < currentItemCount) {
        dataSource->activateListItem(selectedItemIndex);
    }
}

/**
 * Újrarajzol egyetlen elemet a listában az adott indexen.
 * @param itemIndex Az újrarajzolandó elem indexe.
 */
void ScrollableListComponent::redrawItem(int itemIndex) {
    if (!dataSource || itemIndex < 0 || itemIndex >= currentItemCount) return;

    // Ellenőrizzük, hogy az elem látható-e
    if (itemIndex >= topItemIndex && itemIndex < topItemIndex + visibleItems) {
        int itemScreenY = listY + (itemIndex - topItemIndex) * itemHeight;
        dataSource->drawListItem(tft, itemIndex, listX, itemScreenY, listW, itemHeight, itemIndex == selectedItemIndex);
    }
}
