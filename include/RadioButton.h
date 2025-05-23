#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include <vector>

#include "TftButton.h"

class RadioButtonGroup;  // Forward declaration

class RadioButton : public TftButton {
   private:
    RadioButtonGroup* group_ = nullptr;  // Pointer a csoporthoz, amihez tartozik

    friend class RadioButtonGroup;  // A csoport hozzáférhet a privát tagokhoz

    // A setState-et priváttá tesszük, hogy csak a csoport tudja állítani
    // Vagy a TftButton::setState-et használjuk, de a RadioButtonGroup kezeli a logikát
    // void setStateInternal(ButtonState newState) {
    //     TftButton::setState(newState);
    // }

   public:
    RadioButton(uint8_t id, TFT_eSPI& tft, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, RadioButtonGroup* group = nullptr)
        : TftButton(id, tft, x, y, w, h, label, ButtonType::Pushable, ButtonState::Off), group_(group) {
        // A RadioButton mindig Pushable, de a kiválasztottságot az "On" állapot jelzi
        // A mini fontot itt is beállíthatjuk, ha ez az alapértelmezett a rádiógombokhoz
        setMiniFont(true);
    }

    // A handleTouch felülírása vagy egy új metódus a csoportnak
    // A TftButton::handleTouch-t használjuk, de a feldolgozás a csoportban történik
    void setGroup(RadioButtonGroup* group) { group_ = group; }

    // Felülírjuk a draw metódust, hogy a kiválasztott állapotot másképp jelezze,
    // pl. kitöltött körrel vagy más keretszínnel, LED nélkül.
    // Vagy a TftButton::draw()-t használjuk, és az "On" állapotot a kívánt módon jeleníti meg.
    // A LED eltávolítása a miniFont-os gomboknál már megtörtént a TftButton-ban.
};

class RadioButtonGroup {
   private:
    std::vector<RadioButton*> buttons_;
    int selectedIndex_ = -1;  // -1, ha egyik sincs kiválasztva (pl. "Off" mód)

   public:
    RadioButtonGroup() = default;

    ~RadioButtonGroup() {
        // Dinamikusan allokált RadioButton objektumok felszabadítása
        for (RadioButton* btn : buttons_) {
            if (btn) {
                delete btn;
            }
        }
        buttons_.clear();  // A vektor kiürítése
    }

    void addButton(RadioButton* button) {
        if (button) {
            buttons_.push_back(button);
            button->setGroup(this);  // Beállítjuk a gomb csoportját
        }
    }

    // Visszaadja a gombot ID alapján, vagy nullptr-t, ha nem található
    RadioButton* getButtonById(uint8_t buttonId) const {
        for (RadioButton* btn : buttons_) {
            if (btn->getId() == buttonId) {
                return btn;
            }
        }
        return nullptr;
    }

    // Ezt hívná meg az AmDisplay, amikor egy RadioButton-ra kattintanak
    // Vagy a RadioButton handleTouch-a hívná ezt
    void selectButton(uint8_t buttonId) {
        int newlySelectedIndex = -1;
        for (size_t i = 0; i < buttons_.size(); ++i) {
            if (buttons_[i]->getId() == buttonId) {
                newlySelectedIndex = i;
                break;
            }
        }

        if (newlySelectedIndex != -1 && newlySelectedIndex != selectedIndex_) {
            // Régi kiválasztott gomb "Off" állapotba
            if (selectedIndex_ != -1 && selectedIndex_ < buttons_.size()) {
                buttons_[selectedIndex_]->setState(TftButton::ButtonState::Off);
            }
            // Új gomb "On" állapotba
            buttons_[newlySelectedIndex]->setState(TftButton::ButtonState::On);
            selectedIndex_ = newlySelectedIndex;
        } else if (newlySelectedIndex != -1 && newlySelectedIndex == selectedIndex_) {
            // Ha ugyanazt a gombot nyomtuk meg, ami már ki van választva,
            // akkor nem csinálunk semmit, vagy esetleg "Off"-ra állítjuk, ha ez a kívánt viselkedés.
            // Jelenleg marad "On".
        }
    }

    void selectButtonByIndex(int index) {
        if (index >= 0 && index < buttons_.size()) {
            selectButton(buttons_[index]->getId());
        } else if (index == -1 && selectedIndex_ != -1) {  // "Off" mód
            if (selectedIndex_ < buttons_.size()) {        // Biztonsági ellenőrzés
                buttons_[selectedIndex_]->setState(TftButton::ButtonState::Off);
            }
            selectedIndex_ = -1;
        }
    }

    int getSelectedIndex() const { return selectedIndex_; }

    RadioButton* getSelectedButton() const {
        if (selectedIndex_ != -1 && selectedIndex_ < buttons_.size()) {
            return buttons_[selectedIndex_];
        }
        return nullptr;
    }

    // Kezeli az összes gomb érintését
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) {
        for (RadioButton* btn : buttons_) {
            if (btn->handleTouch(touched, tx, ty)) {
                // A TftButton::handleTouch már generál egy "Pushed" eseményt a felengedéskor.
                // Ezt az eseményt kellene az AmDisplay::processScreenButtonTouchEvent-ben
                // elkapni, és az alapján meghívni a RadioButtonGroup::selectButton(id)-t.
                return true;  // Jelezzük, hogy valamelyik gomb kezelte
            }
        }
        return false;
    }

    void draw() {
        for (RadioButton* btn : buttons_) {
            btn->draw();
        }
    }

    void setPositions(uint16_t startX, uint16_t startY, uint16_t gapY) {
        uint16_t currentY = startY;
        for (RadioButton* btn : buttons_) {
            btn->setPosition(startX, currentY);
            currentY += btn->getH() + gapY;
        }
    }
};

#endif  // RADIOBUTTON_H
