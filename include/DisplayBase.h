#ifndef __DISPLAY_BASE_H
#define __DISPLAY_BASE_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Band.h"
#include "Config.h"
#include "DialogBase.h"
#include "IDialogParent.h"
#include "IGuiEvents.h"
#include "MessageDialog.h"
#include "MultiButtonDialog.h"
#include "Si4735Utils.h"
#include "TftButton.h"
#include "ValueChangeDialog.h"
#include "rtVars.h"
#include "utils.h"

// A képernyő változó adatok frissítési ciklusideje msec-ben
#define SCREEN_COMPS_REFRESH_TIME_MSEC 500

// Képernyőgombok mérete
#define SCRN_BTN_H 35      // Gombok magassága
#define SCRN_BTN_W 63      // Gombok szélessége
#define SCREEN_BTNS_GAP 5  // Gombok közötti gap

#define SCRN_HBTNS_ID_START 25  // A horizontális képernyő menübuttonok kezdő ID-je
#define SCRN_VBTNS_ID_START 50  // A vertikális képernyő menübuttonok kezdő ID-je

/**
 * DisplayBase base osztály
 */
class DisplayBase : public Si4735Utils, public IGuiEvents, public IDialogParent {

   public:
    // Lehetséges képernyő típusok
    enum DisplayType { none, fm, am, freqScan, screenSaver, setup };

   private:
    // Gombok orientációja
    enum ButtonOrientation { Horizontal, Vertical };

    // Vízszintes gombsor
    TftButton **horizontalScreenButtons = nullptr;  // A dinamikusan létrehozott gombok tömbjére mutató pointer
    uint8_t horizontalScreenButtonsCount = 0;       // A dinamikusan létrehozott gombok száma

    // Függőleges gombsor
    TftButton **verticalScreenButtons = nullptr;  // Új: Vertikális gombok tömbje
    uint8_t verticalScreenButtonsCount = 0;       // Új: Vertikális gombok száma

    // A lenyomott képernyő menügomb adatai
    TftButton::ButtonTouchEvent screenButtonTouchEvent = TftButton::noTouchEvent;

    // A dialógban megnyomott gomb adatai
    TftButton::ButtonTouchEvent dialogButtonResponse = TftButton::noTouchEvent;

    /**
     * Megkeresi a gombot a label alapján a megadott tömbben
     *
     * @param buttons A gombok tömbje
     * @param buttonsCount A gombok száma
     * @param label A keresett gomb label-je
     * @return A TftButton pointere, vagy nullptr, ha nincs ilyen gomb
     */
    TftButton *findButtonInArray(TftButton **buttons, uint8_t buttonsCount, const char *label);

   protected:
    // Képernyőgombok legyártását segítő rekord
    struct BuildButtonData {
        const char *label;
        TftButton::ButtonType type;
        TftButton::ButtonState state;
    };

    // TFT objektum
    TFT_eSPI &tft;

    // A képernyőn megjelenő dialog pointere
    DialogBase *pDialog = nullptr;

    // Frekvencia változott-e az utolsó kijelzés frissítés óta?
    bool frequencyChanged = false;

    /**
     * Gombok automatikus pozicionálása
     */
    uint16_t getAutoButtonPosition(ButtonOrientation orientation, uint8_t index, bool isX);

    /**
     * Gombok legyártása
     */
    TftButton **buildScreenButtons(ButtonOrientation orientation, BuildButtonData buttonsData[], uint8_t buttonsDataLength, uint8_t startId, uint8_t &buttonsCount);

    /**
     * Vertikális képernyő menügombok legyártása
     *
     * @param buttonsData A gombok adatai
     * @param buttonsDataLength A gombok száma
     * @param isMandatoryNeed Ha true, akkor a kötelező gombokat az elejéhez másolja
     */
    void buildVerticalScreenButtons(BuildButtonData buttonsData[], uint8_t buttonsDataLength, bool isMandatoryNeed = true);

    /**
     * Horizontális képernyő menügombok legyártása
     *
     * @param buttonsData A gombok adatai
     * @param buttonsDataLength A gombok száma
     * @param isMandatoryNeed Ha true, akkor a kötelező gombokat az elejéhez másolja
     */
    void buildHorizontalScreenButtons(BuildButtonData buttonsData[], uint8_t buttonsDataLength, bool isMandatoryNeed = true);

    /**
     * Gombok kirajzolása
     */
    void drawButtons(TftButton **buttons, uint8_t buttonsCount);

    /**
     * Gombok állapotának frissítése
     */
    void updateButtonStatus();

    /**
     * Képernyő menügombok kirajzolása
     */
    void drawScreenButtons();

    /**
     * Státusz kirajzolása
     */
    void drawBfoStatus(bool initFont = false);
    void drawAgcAttStatus(bool initFont = false);
    void drawStepStatus(bool initFont = false);
    void drawAntCapStatus(bool initFont = false);
    void dawStatusLine();

    /**
     * Gombok törlése
     */
    void deleteButtons(TftButton **buttons, uint8_t buttonsCount);

    /**
     * Gombok touch eseményének kezelése
     */
    bool handleButtonTouch(TftButton **buttons, uint8_t buttonsCount, bool touched, uint16_t tx, uint16_t ty);

    /**
     * Megkeresi a gombot a label alapján
     *
     * @param label A keresett gomb label-je
     * @return A TftButton pointere, vagy nullptr, ha nincs ilyen gomb
     */
    TftButton *findButtonByLabel(const char *label);

    /**
     * Közös gombok touch handlere
     */
    bool processMandatoryButtonTouchEvent(TftButton::ButtonTouchEvent &event);

   public:
    /**
     * Konstruktor
     */
    DisplayBase(TFT_eSPI &tft, SI4735 &si4735, Band &band);

    /**
     * Destruktor
     */
    virtual ~DisplayBase();

    /**
     * Aktuális képernyő típusának lekérdezése, implemnetálnia kell a leszármazottnak
     */
    virtual inline DisplayType getDisplayType() = 0;

    /**
     * Dialóg pointer lekérése
     */
    inline DialogBase *getPDialog() { return pDialog; }

    /**
     * A dialog által átadott megnyomott gomb adatai
     * Az IDialogParent-ből jön, a dialóg hívja, ha nyomtak rajta valamit
     */
    inline void setDialogResponse(TftButton::ButtonTouchEvent event) override {
        // A dialogButtonResponse saját másolatot kap, független az eredeti event forrástól, a dialogot lehet törölni
        dialogButtonResponse = event;
    }

    /**
     * Cancelt vagy 'X'-et nyomtak a dialogon?
     */
    inline bool isDialogResponseCancelOrCloseX() override {
        // Ha 'Cancel'-t vagy 'X'-et nyomtak, akkor true-val térünk vissza
        return (dialogButtonResponse.id == DLG_CLOSE_BUTTON_ID or dialogButtonResponse.id == DLG_CANCEL_BUTTON_ID);
    }

    /**
     * Képernyő kirajzolása, implemnetálnia kell a leszármazottnak
     */
    virtual void drawScreen() = 0;

    /**
     * ScreenButton touch esemény feldolgozása
     */
    virtual void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) {};

    /**
     * Dialóg Button touch esemény feldolgozása
     * - alapesetben csak becsukjuk a dialógot
     * - újrarajzoljuk a képernyőt
     * (Ha kell a leszármazottnak akkor majd felülírja)
     */
    virtual void processDialogButtonResponse(TftButton::ButtonTouchEvent &event) {

        DEBUG("DisplayBase::processDialogButtonResponse() -> id: %d, label: %s, state: %s\n", event.id, event.label, TftButton::decodeState(event.state));

        // Töröljük a dialógot
        delete this->pDialog;
        this->pDialog = nullptr;

        // Újrarajzoljuk a leszármazott képernyőjét
        this->drawScreen();
    };

    /**
     * Esemény nélküli display loop -> Adatok periódikus megjelenítése, implemnetálnia kell a leszármazottnak
     */
    virtual void displayLoop() = 0;

    /**
     * Arduino loop hívás (a leszármazott nem írhatja felül)
     * @return true -> ha volt valalami touch vagy rotary esemény kezelés, a screensavert resetelni kell ilyenkor
     */
    virtual bool loop(RotaryEncoder::EncoderState encoderState) final;

    /**
     * Az előző képernyőtípus beállítása
     * A SetupDisplay esetén használjuk, itt adjuk át, hogy hova kell visszatérnie a képrnyő bezárása után
     */
    virtual void setPrevDisplayType(DisplayBase::DisplayType prevDisplay) {};
};

// Globális változó az aktuális kijelző váltásának jelzésére (a főprogramban deklarálva)
extern DisplayBase::DisplayType newDisplay;

#endif  //__DISPLAY_BASE_H
