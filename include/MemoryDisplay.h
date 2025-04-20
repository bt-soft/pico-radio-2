#ifndef __MEMORYDISPLAY_H
#define __MEMORYDISPLAY_H

#include "DisplayBase.h"
#include "StationData.h"   // Szükséges a StationData-hoz
#include "StationStore.h"  // Szükséges a store objektumokhoz

class MemoryDisplay : public DisplayBase {
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

    // Lista megjelenítéséhez szükséges változók
    int listScrollOffset = 0;             // Hányadik elemtől listázunk
    int selectedListIndex = -1;           // Kiválasztott elem indexe (-1, ha nincs)
    uint8_t visibleLines = 0;             // Hány sor fér ki a listából
    uint16_t listX, listY, listW, listH;  // Lista területének koordinátái
    uint8_t lineHeight;                   // Egy sor magassága

    // Ideiglenes tároló az új/szerkesztett állomás nevéhez
    // A VirtualKeyboardDialog ebbe írja az eredményt.
    String stationNameBuffer = "";
    // Ideiglenes tároló az új állomás mentéséhez szükséges egyéb adatokhoz
    StationData pendingStationData;

    // Helper metódusok
    void drawListItem(int index);  // Kirajzol egyetlen listaelemet a megadott indexre
    void drawStationList();
    void saveCurrentStation();     // Aktuális állomás mentése dialógussal
    void editSelectedStation();    // Kiválasztott állomás szerkesztése
    void deleteSelectedStation();  // Kiválasztott állomás törlése (megerősítéssel)
    void tuneToSelectedStation();  // Behúzza a kiválasztott állomást

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
