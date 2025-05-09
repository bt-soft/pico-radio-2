

#ifndef ISCROLLABLELISTDATASOURCE_H
#define ISCROLLABLELISTDATASOURCE_H

#include <TFT_eSPI.h>

class IScrollableListDataSource {
   public:
    virtual ~IScrollableListDataSource() = default;

    // Visszaadja a lista elemeinek teljes számát.
    virtual int getItemCount() const = 0;

    // Kirajzol egyetlen listaelemet a megadott koordinátákra és állapottal.
    // tft: A TFT objektum, amire rajzolni kell.
    // index: Az elem abszolút indexe az adatforrásban.
    // x, y, w, h: Az elem kirajzolásához rendelkezésre álló terület határai.
    // isSelected: Igaz, ha az elem aktuálisan ki van választva.
    virtual void drawListItem(TFT_eSPI& tft, int index, int x, int y, int w, int h, bool isSelected) = 0;

    // Akkor hívódik meg, amikor egy elem aktiválásra kerül (pl. kattintás, érintés).
    virtual void activateListItem(int index) = 0;

    // Visszaadja egyetlen listaelem magasságát.
    virtual int getItemHeight() const = 0;

    // Opcionális: Akkor hívódik meg, amikor a lista komponensnek szüksége van az adatok betöltésére/frissítésére.
    virtual int loadData() = 0;
};

#endif  // ISCROLLABLELISTDATASOURCE_H
