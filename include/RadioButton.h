#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include <TFT_eSPI.h>

#include <vector>

#include "defines.h"  // Színekhez és DEBUG-hoz

// Előre deklaráció
class RadioButtonGroup;

namespace RadioButtonConstants {
constexpr uint8_t CIRCLE_RADIUS = 6;
constexpr uint8_t DOT_RADIUS = 3;
constexpr uint8_t CIRCLE_X_OFFSET = CIRCLE_RADIUS + 3;                   // Kör X eltolása a gomb bal szélétől
constexpr uint8_t LABEL_X_OFFSET = CIRCLE_X_OFFSET + CIRCLE_RADIUS + 5;  // Felirat X eltolása a kör után
constexpr uint16_t DEFAULT_TEXT_COLOR = TFT_WHITE;
constexpr uint16_t SELECTED_TEXT_COLOR = TFT_YELLOW;  // Vagy más kiemelő szín
constexpr uint16_t CIRCLE_COLOR = TFT_WHITE;
constexpr uint16_t DOT_COLOR = TFT_WHITE;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;  // Vagy a dialógus háttérszíne
}  // namespace RadioButtonConstants

class RadioButton {
   private:
    TFT_eSPI& tft_;
    uint8_t id_;
    uint16_t x_, y_, w_, h_;
    String label_;
    bool selected_ = false;
    RadioButtonGroup* group_ = nullptr;
    bool useMiniFont_ = true;  // Alapértelmezetten mini font

    friend class RadioButtonGroup;

   public:
    RadioButton(uint8_t id, TFT_eSPI& tft, const String& label) : tft_(tft), id_(id), label_(label) {}

    void setPosition(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
        x_ = x;
        y_ = y;
        w_ = w;
        h_ = h;
    }

    void draw() {
        using namespace RadioButtonConstants;
        // Háttér törlése (opcionális, ha a szülő gondoskodik róla, pl. a RadioButtonGroup.draw() vagy AmDisplay)
        // A RadioButton maga ne törölje a teljes w_, h_ területét, mert átfedhetnek,
        // vagy a group rajzolja a hátteret.
        // Ha minden gomb külön területet kap, akkor itt lehetne tft_.fillRect(x_, y_, w_, h_, BACKGROUND_COLOR);

        // Kör kirajzolása
        uint16_t circleCenterX = x_ + CIRCLE_X_OFFSET;
        uint16_t circleCenterY = y_ + h_ / 2;
        tft_.drawCircle(circleCenterX, circleCenterY, CIRCLE_RADIUS, CIRCLE_COLOR);

        // Pötty kirajzolása, ha kiválasztott
        if (selected_) {
            tft_.fillCircle(circleCenterX, circleCenterY, DOT_RADIUS, DOT_COLOR);
        } else {
            // Ha nincs kiválasztva, töröljük a pötty helyét a háttérszínnel
            // Ez akkor fontos, ha a draw() metódust egy korábban kiválasztott gombra hívjuk,
            // ami most már nem kiválasztott.
            tft_.fillCircle(circleCenterX, circleCenterY, DOT_RADIUS, BACKGROUND_COLOR);
        }

        // Felirat kirajzolása
        if (useMiniFont_) {
            tft_.setFreeFont();  // Alapértelmezett (kisebb) font
            tft_.setTextSize(1);
        } else {
            // Nagyobb fontot itt nem definiáltunk, de ha kellene, itt lehetne beállítani
            // tft_.setFreeFont(&FreeSansBold9pt7b);
            tft_.setFreeFont();  // Maradjunk az alapértelmezettnél, ha nincs explicit nagyobb
            tft_.setTextSize(1);
        }
        tft_.setTextColor(selected_ ? SELECTED_TEXT_COLOR : DEFAULT_TEXT_COLOR, BACKGROUND_COLOR);
        tft_.setTextDatum(ML_DATUM);  // Középre-balra igazítás
        tft_.drawString(label_, x_ + LABEL_X_OFFSET, y_ + h_ / 2 + (useMiniFont_ ? 1 : 0));
    }

    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) {
        if (touched && (tx >= x_ && tx < (x_ + w_) && ty >= y_ && ty < (y_ + h_))) {
            // Ha megérintették, a csoportot értesítjük
            // A RadioButton maga nem változtatja meg a 'selected_' állapotát,
            // azt a RadioButtonGroup fogja kezelni a selectButtonById/ByIndex hívásakor.
            // Itt csak jelezzük, hogy ez a gomb lett megérintve.
            return true;
        }
        return false;
    }

    void setSelected(bool selected) {
        if (selected_ != selected) {
            selected_ = selected;
            draw();  // Újrarajzolás az állapotváltozás miatt
        }
    }

    bool isSelected() const { return selected_; }

    uint8_t getId() const { return id_; }

    const String& getLabel() const { return label_; }

    void setGroup(RadioButtonGroup* group) { group_ = group; }

    uint16_t getH() const { return h_; }

    // Szélesség getter, ha szükséges a pozícionáláshoz a group-ban
    uint16_t getW() const { return w_; }
};

class RadioButtonGroup {
   private:
    std::vector<RadioButton*> buttons_;
    int selectedIndex_ = -1;
    TFT_eSPI& tft_;  // Referencia a TFT-re a gombok létrehozásához

   public:
    RadioButtonGroup(TFT_eSPI& tft) : tft_(tft) {}

    ~RadioButtonGroup() {
        for (RadioButton* btn : buttons_) {
            if (btn) {  // Biztonsági ellenőrzés
                delete btn;
            }
        }
        buttons_.clear();
    }

    // Létrehozza és hozzáadja a gombokat a megadott feliratok alapján
    void createButtons(const std::vector<String>& labels, uint8_t startId) {
        // Először töröljük a meglévő gombokat, ha vannak
        for (RadioButton* btn : buttons_) {
            if (btn) delete btn;
        }
        buttons_.clear();
        selectedIndex_ = -1;  // Reseteljük a kiválasztást is

        for (size_t i = 0; i < labels.size(); ++i) {
            RadioButton* newButton = new RadioButton(startId + i, tft_, labels[i]);
            newButton->setGroup(this);
            buttons_.push_back(newButton);
        }
    }

    void selectButtonByIndex(int index) {
        // Először a korábban kiválasztott gombot (ha volt) "Off"-ra állítjuk
        if (selectedIndex_ != -1 && selectedIndex_ < static_cast<int>(buttons_.size())) {
            buttons_[selectedIndex_]->setSelected(false);
        }

        // Majd beállítjuk az új kiválasztott gombot (ha van)
        if (index >= 0 && index < static_cast<int>(buttons_.size())) {  // Új érvényes gomb kiválasztása
            selectedIndex_ = index;
            buttons_[selectedIndex_]->setSelected(true);
        } else {                  // Ha index == -1 (pl. "Off" mód), vagy érvénytelen index
            selectedIndex_ = -1;  // Nincs kiválasztott gomb
        }
    }

    // ID alapján választja ki a gombot
    void selectButtonById(uint8_t buttonId) {
        for (size_t i = 0; i < buttons_.size(); ++i) {
            if (buttons_[i] && buttons_[i]->getId() == buttonId) {  // Ellenőrizzük a nullptr-t is
                selectButtonByIndex(i);
                return;
            }
        }
        // Ha nem található ilyen ID-jú gomb, és "Off"-ra szeretnénk állítani,
        // akkor a selectButtonByIndex(-1) hívható meg.
        // Jelenleg, ha egy nem létező ID-t kapunk, nem csinálunk semmit,
        // vagy expliciten "Off"-ra állíthatnánk: selectButtonByIndex(-1);
    }

    int getSelectedIndex() const { return selectedIndex_; }

    RadioButton* getSelectedButton() const { return (selectedIndex_ != -1 && selectedIndex_ < static_cast<int>(buttons_.size())) ? buttons_[selectedIndex_] : nullptr; }

    // Visszaadja a gombot ID alapján, vagy nullptr-t, ha nem található
    RadioButton* getButtonById(uint8_t buttonId) const {
        for (RadioButton* btn : buttons_) {
            if (btn && btn->getId() == buttonId) {  // Ellenőrizzük a nullptr-t is
                return btn;
            }
        }
        return nullptr;
    }

    // Hozzáférés a buttons_ vektorhoz (csak olvasható), ha az AmDisplay-nek szüksége van rá
    // De jobb lenne, ha a RadioButtonGroup kezelné a saját logikáját.
    // const std::vector<RadioButton*>& getButtons() const { return buttons_; }

    bool handleTouch(bool touched, uint16_t tx, uint16_t ty, uint8_t& pressedButtonId) {
        pressedButtonId = 0xFF;      // Érvénytelen ID alapból
        if (!touched) return false;  // Csak akkor foglalkozunk vele, ha tényleges érintés van

        for (RadioButton* btn : buttons_) {
            if (btn && btn->handleTouch(touched, tx, ty)) {  // Ellenőrizzük a nullptr-t is
                // Ha egy gombot megérintettek, kiválasztjuk azt a csoportban
                // A selectButtonById már gondoskodik a régi kikapcsolásáról és az új bekapcsolásáról
                selectButtonById(btn->getId());
                pressedButtonId = btn->getId();  // Visszaadjuk a megnyomott gomb ID-ját
                return true;
            }
        }
        return false;
    }

    void draw() {
        // Mielőtt a gombokat rajzolnánk, törölhetnénk a teljes csoport területét,
        // ha a gombok háttere nem fedné le teljesen a régi állapotot.
        // De mivel a RadioButton::draw() most már törli a pöttyöt, ha nincs selected,
        // és a felirat hátterét is beállítja, ez talán nem szükséges.
        for (RadioButton* btn : buttons_) {
            if (btn) {  // Ellenőrizzük a nullptr-t is
                btn->draw();
            }
        }
    }

    void setPositionsAndSize(uint16_t startX, uint16_t startY, uint16_t buttonW, uint16_t buttonH, uint16_t gapY) {
        uint16_t currentY = startY;
        for (RadioButton* btn : buttons_) {
            if (btn) {  // Ellenőrizzük a nullptr-t is
                btn->setPosition(startX, currentY, buttonW, buttonH);
                currentY += buttonH + gapY;  // Itt a buttonH-t használjuk, nem a btn->getH()-t, mert egységes magasságot akarunk
            }
        }
    }
};

#endif  // RADIOBUTTON_H
