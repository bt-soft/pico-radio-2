#ifndef __MEMORYDISPLAY_H
#define __MEMORYDISPLAY_H
#include <vector>  // std::vector használatához

#include "DisplayBase.h"
#include "IScrollableListDataSource.h"
#include "ScrollableListComponent.h"
#include "StationData.h"   // Szükséges a StationData-hoz
#include "StationStore.h"  // Szükséges a store objektumokhoz

class MemoryDisplay : public DisplayBase, public IScrollableListDataSource {
   private:
    // Enum a dialógus céljának jelzésére
    enum class DialogMode {
        NONE,
        SAVE_NEW_STATION,   // Új állomás mentése (billentyűzet)
        EDIT_STATION_NAME,  // Meglévő állomás nevének szerkesztése (billentyűzet)
        DELETE_CONFIRM      // Törlés megerősítése (üzenet)
    };

    DialogMode currentDialogMode = DialogMode::NONE;  // Aktuális dialógus célja

    DisplayBase::DisplayType prevDisplay = DisplayBase::DisplayType::none;  // Hova térjünk vissza
    bool isFmMode = true;                                                   // FM vagy AM memóriát mutatunk?

    // Rendezett állomások tárolására
    std::vector<StationData> sortedStations;

    ScrollableListComponent scrollListComponent;
    int dynamicLineHeight;  // Kiszámított sormagasság

    // Ideiglenes tároló az új/szerkesztett állomás nevéhez
    // A VirtualKeyboardDialog ebbe írja az eredményt.
    String stationNameBuffer = "";
    // Ideiglenes tároló az új állomás mentéséhez szükséges egyéb adatokhoz
    StationData pendingStationData;

    // Helper metódusok
    void loadAndSortStations();                                // Betölti és rendezi az állomásokat a sortedStations vektorba
    void saveCurrentStation();                                 // Aktuális állomás mentése dialógussal
    void editSelectedStation();                                // Kiválasztott állomás szerkesztése
    void deleteSelectedStation();                              // Kiválasztott állomás törlése (megerősítéssel)
    void tuneToSelectedStation();                              // Behúzza a kiválasztott állomást
    void updateListAfterTuning(int previouslyTunedSortedIdx);  // Frissíti a listát behangolás után

    // Pointer a megfelelő store objektumra
    FmStationStore* pFmStore = nullptr;
    AmStationStore* pAmStore = nullptr;
    uint8_t getCurrentStationCount() const;
    const StationData* getStationData(uint8_t index) const;
    bool addStationInternal(const StationData& station);
    bool updateStationInternal(uint8_t index, const StationData& station);
    bool deleteStationInternal(uint8_t index);

    // Gombok állapotának frissítése
    void updateActionButtonsState();

   protected:
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent& event) override;
    void processDialogButtonResponse(TftButton::ButtonTouchEvent& event) override;
    void displayLoop() override;

    // IScrollableListDataSource implementation
    int getItemCount() const override;
    void drawListItem(TFT_eSPI& tft_ref, int index, int x, int y, int w, int h, bool isSelected) override;
    void activateListItem(int index) override;
    int getItemHeight() const override;
    void loadData() override;

   public:
    MemoryDisplay(TFT_eSPI& tft, SI4735& si4735, Band& band);
    ~MemoryDisplay();

    void drawScreen() override;

    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::memory; };

    /**
     * Az előző képernyőtípus beállítása
     * Itt adjuk át, hogy hova kell visszatérnie a képrnyő bezárása után
     */
    void setPrevDisplayType(DisplayBase::DisplayType prev) override { this->prevDisplay = prev; };
};

#endif  // __MEMORYDISPLAY_H
