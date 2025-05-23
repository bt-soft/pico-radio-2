#ifndef __TFTBUTTON_H
#define __TFTBUTTON_H

#include "defines.h"
#include "utils.h"

// Egy toggleable button állapotának megállapítása egy logikai kifejezés alapján
#define TFT_TOGGLE_BUTTON_STATE(boolValue) (boolValue ? TftButton::ButtonState::On : TftButton::ButtonState::Off)

// Benyomott gomb háttérszín gradienshez, több iterációs lépés -> erősebb hatás
#define TFT_BUTTON_DARKEN_COLORS_STEPS 6
#define TFT_BUTTON_INVALID_ID 0xFF

#define TFT_BUTTON_LED_ON TFT_GREEN
#define TFT_BUTTON_LED_PUSHED TFT_ORANGE
#define TFT_BUTTON_LED_OFF TFT_COLOR(10, 128, 30)

#define TFT_BUTTON_LONG_PRESS_THRESHOLD 1000  // Hosszú nyomás küszöbértéke (ms)

class TftButton {
   public:
    // A gomb típusa
    enum ButtonType { Toggleable, Pushable };

    // Gomb állapotai
    enum ButtonState {
        Off = 0,
        On,
        Disabled,
        CurrentActive,  // Épp aktív módot jelző gomb, de nem választható (hiszen épp ez van kiválasztva)
        //---- technikai állapotok
        Pushed,       // Csak Pushable gomb felengedésekor küldött esemény
        Clicked,      // Csak Toggleable gomb rövid felengedésekor küldött esemény (opcionális, lehetne csak On/Off)
        LongPressed,  // Hosszú nyomás esemény
        //---
        HOLD,    // Nyomva tartják
        UNKNOWN  // ismeretlen
    };

    struct ButtonTouchEvent {
        uint8_t id;
        const char *label;
        ButtonState state;  // Az esemény típusa (Pushed, Clicked, LongPressed, On, Off...)
    };

    // Érvénytelen gomb esemény kezdeti értéke
    static constexpr ButtonTouchEvent noTouchEvent = {TFT_BUTTON_INVALID_ID, nullptr, ButtonState::UNKNOWN};

    // Barát függvények deklarálása
    friend bool operator==(const ButtonTouchEvent &lhs, const ButtonTouchEvent &rhs);
    friend bool operator!=(const ButtonTouchEvent &lhs, const ButtonTouchEvent &rhs);

   private:
    // A gomb állapotainak megfelelő háttérszín
    const uint16_t TFT_BUTTON_STATE_BG_COLORS[4] = {
        TFT_COLOR(65, 65, 114),   // normal
        TFT_COLOR(65, 65, 114),   // pushed
        TFT_COLOR(95, 95, 95),    // disabled
        TFT_COLOR(243, 179, 105)  // current active
    };

    TFT_eSPI *pTft;                    // Itt pointert használunk a dinamikus tömbök miatt (nem lehet null referenciát használni)
    uint16_t x, y, w, h;               // A gomb pozíciója
    uint8_t id;                        // A gomb ID-je
    char *label;                       // A gomb felirata
    ButtonState state;                 // Állapota
    ButtonState oldState;              // Előző állapota
    ButtonType type;                   // Típusa
    bool buttonPressed;                // Flag a gomb nyomva tartásának követésére
    bool useMiniFont_ = false;         // Kisebb betűtípust használjon-e a felirathoz
    unsigned long pressStartTime = 0;  // Mikor nyomták le a gombot

    /**
     * Ezt nyomták meg?
     */
    inline bool contains(uint16_t tx, uint16_t ty) { return (tx >= x && tx <= x + w && ty >= y && ty <= y + h); }

    /**
     * Lenyomták a gombot
     */
    void pressed() {
        buttonPressed = true;
        oldState = state;
        state = ButtonState::HOLD;
        draw();
    }

    /**
     * Felengedték a gombot
     */
    void released() {
        buttonPressed = false;
        if (type == ButtonType::Toggleable) {
            state = (oldState == ButtonState::Off) ? ButtonState::On : ButtonState::Off;
        } else {
            state = ButtonState::Off;
        }
        oldState = state;

        draw();
    }

    /**
     * Benyomott gomb háttérszín gradiens
     */
    inline uint16_t darkenColor(uint16_t color, uint8_t amount) const {
        // Kivonjuk a piros, zöld és kék színösszetevőket
        uint8_t r = (color & 0xF800) >> 11;
        uint8_t g = (color & 0x07E0) >> 5;
        uint8_t b = (color & 0x001F);

        // Finomítjuk a csökkentési mértéket, figyelembe véve a színösszetevők közötti eltéréseket
        uint8_t darkenAmount = amount > 0 ? (amount >> 3) : 0;

        // A csökkentésnél biztosítjuk, hogy ne menjenek 0 alá az értékek
        r = (r > darkenAmount) ? r - darkenAmount : 0;
        g = (g > darkenAmount) ? g - darkenAmount : 0;
        b = (b > darkenAmount) ? b - darkenAmount : 0;

        // Visszaalakítjuk a színt 16 bites RGB formátumba
        return (r << 11) | (g << 5) | b;
    }

   public:
    /**
     * Default konstruktor
     * (pl.: a dinamikus tömb deklarációhoz)
     */
    TftButton() {}

    /**
     * Konstruktor
     */
    TftButton(uint8_t id, TFT_eSPI &tft, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char *label, ButtonType type, ButtonState state = ButtonState::Off)
        : id(id), pTft(&tft), x(x), y(y), w(w), h(h), type(type), state(state), oldState(state), buttonPressed(false) {

        this->label = new char[strlen(label) + 1];  // Dinamikusan lefoglaljuk a helyet
        strcpy(this->label, label);                 // Átmásoljuk az eredeti stringet
    }

    /**
     * Konstruktor X/Y pozíció nélkül
     * Automatikus elrendezéshez csak a szélesség és a magasság van megadva
     */
    TftButton(uint8_t id, TFT_eSPI &tft, uint16_t w, uint16_t h, const char *label, ButtonType type, ButtonState state = ButtonState::Off)
        : id(id), pTft(&tft), x(0), y(0), w(w), h(h), type(type), buttonPressed(false) {

        this->label = new char[strlen(label) + 1];  // Dinamikusan lefoglaljuk a helyet
        strcpy(this->label, label);                 // Átmásoljuk az eredeti stringet

        if ((state != ButtonState::Off and state != ButtonState::CurrentActive) and type != ButtonType::Toggleable) {
            DEBUG("TftButton::TftButton -> Hiba!! Nem toggleable a gomb, nem lehet a state állapotot beállítani!\n");
        } else {
            this->state = state;
            this->oldState = state;
        }
    }

    /**
     * Destruktor
     */
    virtual ~TftButton() {
        delete[] label;  // Töröljük a label-t
    }

    /**
     * Button szélességének lekérése
     */
    inline uint8_t getWidth() { return w; }

    /**
     * Button x/y pozíciójának beállítása
     * @param x
     * @param y
     */
    inline void setPosition(uint16_t x, uint16_t y) {
        this->x = x;
        this->y = y;
    }

    /**
     * Beállítja, hogy a gomb felirata kisebb ("mini") betűtípussal jelenjen-e meg.
     * @param mini Igaz, ha kisebb betűtípust kell használni.
     */
    inline void setMiniFont(bool mini) { useMiniFont_ = mini; }

    /**
     * A button kirajzolása
     */
    void draw() {
        // A gomb teljes szélességét és magasságát kihasználó sötétedés -> benyomás hatás keltés
        if (buttonPressed) {
            uint8_t stepWidth = w / TFT_BUTTON_DARKEN_COLORS_STEPS;
            uint8_t stepHeight = h / TFT_BUTTON_DARKEN_COLORS_STEPS;
            for (uint8_t i = 0; i < TFT_BUTTON_DARKEN_COLORS_STEPS; i++) {
                uint16_t fadedColor = darkenColor(TFT_BUTTON_STATE_BG_COLORS[oldState], i * 30);  // Erősebb sötétítés
                pTft->fillRoundRect(x + i * stepWidth / 2, y + i * stepHeight / 2, w - i * stepWidth, h - i * stepHeight, 5, fadedColor);
            }
        } else {
            pTft->fillRoundRect(x, y, w, h, 5, TFT_BUTTON_STATE_BG_COLORS[state]);
        }

        // A gomb keretének színe a gomb állapotától függően változik
        // Ha a gomb tiltott, akkor sötétszürke, ha épp aktív, akkor kék, egyébként fehér
        uint32_t color;
        if (buttonPressed) {
            color = TFT_ORANGE;
        } else {
            switch (state) {
                case ButtonState::Disabled:
                    color = TFT_DARKGREY;
                    break;

                case ButtonState::On:
                    color = TFT_GREEN;
                    break;

                case ButtonState::CurrentActive:
                    color = TFT_BLUE;
                    break;

                default:
                    color = TFT_WHITE;
                    break;
            }
        }
        // A gomb keretének kirajzolása
        pTft->drawRoundRect(x, y, w, h, 5, color);

        pTft->setTextColor(color);

        // Az (x, y) koordináta a szöveg középpontja
        pTft->setTextDatum(MC_DATUM);

        // Betűtípus beállítása a felirathoz
        if (useMiniFont_) {
            pTft->setFreeFont();  // Alapértelmezett (kisebb) font
            pTft->setTextSize(1);
        } else {
            pTft->setFreeFont(&FreeSansBold9pt7b);
            pTft->setTextSize(1);
        }
        pTft->setTextPadding(0);
        // A felirat Y pozíciójának finomhangolása, hogy középen legyen
        // A setTextDatum(MC_DATUM) miatt a h/2 a szöveg közepét jelenti.
        // A kisebb font esetén lehet, hogy kicsit lejjebb kell tolni a jobb vizuális középre igazításhoz.
        pTft->drawString(label, x + w / 2, y + h / 2 + (useMiniFont_ ? 1 : 0));  // Mini font esetén +1 pixel lejjebb

        // LED csík kirajzolása ha a gomb aktív vagy push, és nyomják
        uint16_t ledColor = 0;
        if (state == ButtonState::On) {
            // Ha On állapotú, akkor zöld a LED csík
            ledColor = TFT_BUTTON_LED_ON;
        } else if (type == ButtonType::Pushable && buttonPressed) {
            // Ha Pushable típusú és épp nyomva tartják, akkor a LED narancs
            ledColor = TFT_BUTTON_LED_PUSHED;
        } else if (type == ButtonType::Toggleable && state == ButtonState::Off) {
            // Ha Toggleable típusú és Off állapotú, akkor a LED sötétzöld
            ledColor = TFT_BUTTON_LED_OFF;  // Sötétzöld
        }
        // Ha kell állítani a LED színt
        if (ledColor) {
            constexpr uint8_t BUTTON_LED_HEIGHT = 5;
            pTft->fillRect(x + 10, y + h - BUTTON_LED_HEIGHT - 3, w - 20, BUTTON_LED_HEIGHT, ledColor);
        }
    }

    /**
     * A gomb touch eseményeinek kezelése
     * @param touched Jelzi, hogy történt-e érintési esemény.
     * @param tx Az érintési esemény x-koordinátája.
     * @param ty Az érintési esemény y-koordinátája.
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) {
        bool eventGenerated = false;  // Jelzi, ha eseményt kell küldeni

        // Tiltott gomb nem reagál.
        // A CurrentActive állapotú Toggleable gomb sem reagál (de a Pushable igen).
        if (state == ButtonState::Disabled || (state == ButtonState::CurrentActive && type == ButtonType::Toggleable)) {
            buttonPressed = false;  // Biztos, ami biztos
            pressStartTime = 0;
            return false;
        }

        bool isInside = contains(tx, ty);

        if (touched && isInside && !buttonPressed) {
            // --- Gomb lenyomásának kezdete ---
            pressed();                  // Beállítja: buttonPressed = true, state = HOLD, oldState = (ami volt)
            pressStartTime = millis();  // Lenyomás idejének rögzítése
            // Még nem generálunk eseményt

        } else if (touched && buttonPressed) {
            // --- Gomb nyomva tartva ---
            if (state == ButtonState::HOLD) {  // Csak akkor vizsgáljuk, ha még HOLD állapotban van
                if (millis() - pressStartTime >= TFT_BUTTON_LONG_PRESS_THRESHOLD) {
                    // --- Hosszú nyomás érzékelve ---
                    state = ButtonState::LongPressed;  // Állapot váltása LongPressed-re
                    eventGenerated = true;             // Eseményt generálunk MOST
                    // A buildButtonTouchEvent majd LongPressed állapotot ad vissza
                    // Fontos: A gomb még le van nyomva (buttonPressed = true)
                    draw();  // Opcionális: Vizuális visszajelzés a hosszú nyomásról
                }
            }
            // Ha már LongPressed állapotban van, nem csinálunk semmit, amíg nyomva tartják

        } else if (!touched && buttonPressed) {
            // --- Gomb felengedése ---
            ButtonState stateBeforeRelease = state;  // Mentsük el az állapotot a felengedés előtt

            // --- MÓDOSÍTÁS KEZDETE ---
            if (stateBeforeRelease == ButtonState::LongPressed) {
                // Hosszú nyomás utáni felengedés:
                // Az állapotot 'On'-ra állítjuk (mivel a Manual mód aktív állapot)
                // és NEM generálunk újabb eseményt.
                state = ButtonState::On;  // Végleges állapot: On
                oldState = state;         // oldState frissítése
                buttonPressed = false;    // Nyomás vége
                pressStartTime = 0;       // Időzítő nullázása
                draw();                   // Gomb újrarajzolása 'On' állapotban
                // eventGenerated marad false

            } else {
                // Rövid nyomás utáni felengedés:
                // Meghívjuk a normál released() logikát (ami váltogat) és generálunk eseményt.
                released();  // Ez beállítja: buttonPressed = false, state = (On/Off), oldState = state, és rajzol
                // Az eseményt a buildButtonTouchEvent fogja helyesen összeállítani (Pushed vagy On/Off)
                eventGenerated = true;
            }
            // --- MÓDOSÍTÁS VÉGE ---

        } else if (touched && !isInside && buttonPressed) {
            // --- Ujj lehúzása a gombról lenyomás közben ---
            buttonPressed = false;  // Megszakítjuk a nyomást
            state = oldState;       // Visszaállítjuk az eredeti állapotot
            pressStartTime = 0;
            draw();  // Újrarajzolás az eredeti állapotban
                     // Nincs esemény generálva (Cancel)

        } else if (!touched && !buttonPressed) {
            // Nincs érintés, nincs lenyomás -> nincs teendő
            pressStartTime = 0;  // Biztonsági nullázás
        }

        return eventGenerated;  // Visszaadjuk, hogy kell-e eseményt küldeni
    }

    /**
     * ButtonTouchEvent legyártása
     */
    ButtonTouchEvent buildButtonTouchEvent() {

        // Ha Pushable gombot engedtek fel (és nem volt LongPress)
        if (type == ButtonType::Pushable && state == ButtonState::Off && !buttonPressed) {
            // A state most Off, de mi Pushed eseményt küldünk
            return {id, label, ButtonState::Pushed};
        }
        // Ha LongPressed állapotban vagyunk (ezt a handleTouch állította be)
        if (state == ButtonState::LongPressed) {
            // Közvetlenül a LongPressed állapotot küldjük eseményként
            // Fontos: Lehet, hogy a process...TouchEvent-ben vissza kell állítani az állapotot HOLD-ról,
            // miután feldolgoztuk a LongPressed eseményt, hogy ne ragadjon be.
            return {id, label, ButtonState::LongPressed};
        }
        // Minden más esetben (pl. Toggleable felengedésekor On/Off)
        // a gomb aktuális állapotát küldjük.
        return {id, label, state};
    }

    /**
     * Button állapotának beállítása
     * @param newState új állapot
     */
    inline void setState(ButtonState newState) {
        state = newState;
        oldState = newState;
        draw();
    }

    /**
     * Button állapotának lekérése
     * @return állapot
     */
    inline ButtonState getState() const { return state; }

    /**
     *
     */
    inline uint8_t getId() const { return id; }

    /**
     *
     */
    inline const char *getLabel() const { return label; }

    /**
     * Gomb feliratának beállítása futás közben.
     * @param newLabel Az új felirat.
     */
    void setLabel(const char *newLabel) {
        if (label != nullptr) {
            delete[] label;  // Régi label memória felszabadítása
        }
        label = new char[strlen(newLabel) + 1];  // Új memória foglalása
        strcpy(label, newLabel);                 // Új label másolása
        draw();                                  // Gomb újrarajzolása az új felirattal
    }

    /**
     * Button állapot
     */
    static const char *decodeState(ButtonState state) {

        switch (state) {
            case ButtonState::Off:
                return "Off";
            case ButtonState::On:
                return "On";
            case ButtonState::Disabled:
                return "Disabled";
            case ButtonState::CurrentActive:
                return "CurrentActive";
            case ButtonState::HOLD:
                return "HOLD";
            case ButtonState::Pushed:
                return "Pushed";
            case ButtonState::UNKNOWN:
            default:
                return "UNKNOWN";
        }
    }
};

// == operátor túlterhelése (nem-tagfüggvény)
inline bool operator==(const TftButton::ButtonTouchEvent &lhs, const TftButton::ButtonTouchEvent &rhs) {
    return (lhs.id == rhs.id) && (lhs.state == rhs.state) && (lhs.label == rhs.label);
}

// != operátor túlterhelése (nem-tagfüggvény)
inline bool operator!=(const TftButton::ButtonTouchEvent &lhs, const TftButton::ButtonTouchEvent &rhs) {
    return !(lhs == rhs);  // Ha nem egyenlő, akkor a == operátor segítségével negáljuk az eredményt
}

#endif  //__TFTBUTTON_H