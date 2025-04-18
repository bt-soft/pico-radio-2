#ifndef __VALUECHANGEDIALOG_H
#define __VALUECHANGEDIALOG_H

#include <functional>
#include <variant>

#include "MessageDialog.h"

/**
 *
 */
class ValueChangeDialog : public MessageDialog {
   public:
    enum class ValueType { Uint8, Integer, Float, Boolean };
    enum class ValuePtrType { Uint8, Integer, Float, Boolean, Unknown };

   private:
    ValueType valueType;                                    // A változtatott érték típusa
    std::variant<uint8_t, int, float, bool> value;          // A kezelt érték
    std::variant<uint8_t, int, float, bool> originalValue;  // Eredeti érték
    double minVal, maxVal, step;                            // step- és határértékek

    // A pointer által mutatott érték megváltoztatásához (ha nincs callback, akkor csak ez változtatja az értéket)
    void *valuePtr;             // A konstruktorban átadott pointer
    ValuePtrType valuePtrType;  // A valuePtr típusa

    // Általános callback függvény (különböző adattípusok <uint8_t, int, float, bool> beleférnek)
    std::function<void(double)> onValueChanged;

    /**
     * Ha eltérő az eredeti és a jelenlegi érték, akkor azt más színnel jelenítjük meg
     */
    uint16_t getTextColor() { return (value == originalValue) ? TFT_COLOR(40, 64, 128) : TFT_WHITE; }

    /**
     * Eredeti érték visszaállítása
     */
    void restoreOriginalValue() { value = originalValue; }

    /**
     * Érték kirajzolása
     */
    void drawValue() {
        tft.setFreeFont(&FreeSansBold9pt7b);
        tft.setTextSize(2);
        tft.setCursor(x + 50, contentY + 30);
        tft.fillRect(x + 30, contentY, w - 40, 40, DLG_BACKGROUND_COLOR);  // Korábbi érték törlése
        tft.setTextColor(getTextColor(), DLG_BACKGROUND_COLOR);

        if (std::holds_alternative<uint8_t>(value)) {
            tft.print(std::get<uint8_t>(value));
        } else if (std::holds_alternative<int>(value)) {
            tft.print(std::get<int>(value));
        } else if (std::holds_alternative<float>(value)) {
            tft.print(std::get<float>(value), 2);
        } else if (std::holds_alternative<bool>(value)) {
            tft.print(std::get<bool>(value) ? "ON" : "OFF");
        }
    }

    /**
     *
     */
    void setValue() {

        // Frissítjük a valuePtr által mutatott értéket
        if (valuePtrType == ValuePtrType::Uint8) {
            *static_cast<uint8_t *>(valuePtr) = std::get<uint8_t>(value);
        } else if (valuePtrType == ValuePtrType::Integer) {
            *static_cast<int *>(valuePtr) = std::get<int>(value);
        } else if (valuePtrType == ValuePtrType::Float) {
            *static_cast<float *>(valuePtr) = std::get<float>(value);
        } else if (valuePtrType == ValuePtrType::Boolean) {
            *static_cast<bool *>(valuePtr) = std::get<bool>(value);
        }

        // Ha van callback, akkor azt is meghívjuk
        if (onValueChanged) {
            if (valueType == ValueType::Uint8) {
                onValueChanged(std::get<uint8_t>(value));
            } else if (valueType == ValueType::Integer) {
                onValueChanged(std::get<int>(value));
            } else if (valueType == ValueType::Float) {
                onValueChanged(std::get<float>(value));
            } else if (valueType == ValueType::Boolean) {
                onValueChanged(std::get<bool>(value));
            }
        }
    }

   public:
    /**
     * Generikus konstruktor
     */
    template <typename T>
    ValueChangeDialog(IDialogParent *pParent, TFT_eSPI &tft, uint16_t w, uint16_t h, const __FlashStringHelper *title, const __FlashStringHelper *message, T *valuePtr, T minVal,
                      T maxVal, T step, std::function<void(double)> callback = nullptr)
        : MessageDialog(pParent, tft, w, h, title, message, "OK", "Cancel"),
          minVal(static_cast<double>(minVal)),
          maxVal(static_cast<double>(maxVal)),
          step(static_cast<double>(step)),
          valuePtr(static_cast<void *>(valuePtr)) {

        if constexpr (std::is_same_v<T, uint8_t>) {
            valueType = ValueType::Uint8;
            value = *valuePtr;
            originalValue = *valuePtr;
            this->valuePtrType = ValuePtrType::Uint8;

        } else if constexpr (std::is_same_v<T, int>) {
            valueType = ValueType::Integer;
            value = *valuePtr;
            originalValue = *valuePtr;
            this->valuePtrType = ValuePtrType::Integer;

        } else if constexpr (std::is_same_v<T, float>) {
            valueType = ValueType::Float;
            value = *valuePtr;
            originalValue = *valuePtr;
            this->valuePtrType = ValuePtrType::Float;

        } else if constexpr (std::is_same_v<T, bool>) {
            valueType = ValueType::Boolean;
            value = *valuePtr;
            originalValue = *valuePtr;
            this->valuePtrType = ValuePtrType::Boolean;
            // A bool típusnál a step, min, max értékeket felülírjuk, mert nincs értelmük
            this->step = 1;
            this->minVal = 0;
            this->maxVal = 1;

        } else {
            this->valuePtrType = ValuePtrType::Unknown;
            Utils::beepTick();
            DEBUG("ValueChangeDialog: Ismeretlen adattípus!\n");
        }

        // Callbackot általánosítjuk
        onValueChanged = callback;

        // Megjelenítjük a dialógot
        drawDialog();
    }

    /**
     * Destruktor
     */
    ~ValueChangeDialog() {}

    /**
     * Dialog kirajzolása
     */
    void drawDialog() override {
        // Kirajzoljuk az értéket
        drawValue();
    }

    /**
     * Rotary handler
     */
    virtual bool handleRotary(RotaryEncoder::EncoderState encoderState) override {

        // Ha klikkeltek, akkor becsukjuk a dialogot, ugyan az mint az OK gomb
        if (MessageDialog::handleRotary(encoderState)) {
            return true;
        }

        // Ha nincs tekergetés akkor nem megyünk tovább
        if (encoderState.direction == RotaryEncoder::Direction::None && encoderState.value == 0) {
            return false;
        }

        // Megnézzük, hogy a step 0-e és nem bool típusról van-e szó
        bool useEncoderValue = (step == 0.0 && valueType != ValueType::Boolean);

        // Az érték változtatása a Rotary irányának vagy értékének megfelelően
        if (valueType == ValueType::Uint8) {
            uint8_t &val = std::get<uint8_t>(value);
            int tempVal = val;  // nagyobb típusra

            if (useEncoderValue) {
                tempVal += encoderState.value;  // Közvetlenül az encoder értékével növelünk/csökkentünk
            } else {
                if (encoderState.direction == RotaryEncoder::Direction::Up) {
                    tempVal += static_cast<int>(step);
                } else {
                    tempVal -= static_cast<int>(step);
                }
            }
            // Érvényes tartomány rögzítése
            tempVal = std::max(static_cast<int>(minVal), std::min(tempVal, static_cast<int>(maxVal)));
            val = static_cast<uint8_t>(tempVal);  // uint8_t-re vissza cast

        } else if (valueType == ValueType::Integer) {
            int &val = std::get<int>(value);
            long long tempVal = val;  // nagyobb típusra

            if (useEncoderValue) {
                tempVal += encoderState.value;  // Közvetlenül az encoder értékével növelünk/csökkentünk
                DEBUG("ValueChangeDialog::handleRotary(long long) -> encoderState.value: %d\n", encoderState.value);
            } else {
                if (encoderState.direction == RotaryEncoder::Direction::Up) {
                    tempVal += static_cast<long long>(step);
                } else {
                    tempVal -= static_cast<long long>(step);
                }
            }
            // Érvényes tartomány rögzítése
            tempVal = std::max(static_cast<long long>(minVal), std::min(tempVal, static_cast<long long>(maxVal)));
            val = static_cast<int>(tempVal);  // int-re vissza cast

        } else if (valueType == ValueType::Float) {
            float &val = std::get<float>(value);
            double tempVal = val;  // nagyobb típusra

            if (useEncoderValue) {
                tempVal += static_cast<double>(encoderState.value);  // Közvetlenül az encoder értékével növelünk/csökkentünk
            } else {
                if (encoderState.direction == RotaryEncoder::Direction::Up) {
                    tempVal += static_cast<double>(step);
                } else {
                    tempVal -= static_cast<double>(step);
                }
            }
            // Érvényes tartomány rögzítése
            tempVal = std::max(static_cast<double>(minVal), std::min(tempVal, static_cast<double>(maxVal)));
            val = static_cast<float>(tempVal);  // float-ra vissza cast

        } else if (valueType == ValueType::Boolean) {
            // A bool típusnál marad a régi logika (irány alapján váltás)
            bool &val = std::get<bool>(value);
            if (encoderState.direction != RotaryEncoder::Direction::None) {
                // Csak akkor váltunk, ha volt elmozdulás
                val = !val;  // Egyszerűen invertáljuk az értéket
            }
        }

        // Kiírjuk az új értéket
        drawValue();

        // beállítjuk az új értéket referencia pointer oldalon és callback visszatérési értékben is ezzel hívunk vissza
        setValue();

        return true;
    }

    /**
     * Touch esemény lekezelése
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override {

        if (MessageDialog::handleTouch(touched, tx, ty)) {

            // Az 'X'-et vagy 'Cancel'-t nyomták meg?
            if (DialogBase::pParent->isDialogResponseCancelOrCloseX()) {
                // Visszaállítjuk az eredeti értéket
                restoreOriginalValue();
            }

            // beállítjuk az új értéket (pointer + callback)
            // Az OK gomb megnyomásakor is lefut ez a setValue()
            setValue();

            return true;
        }

        return false;
    }
};

#endif  // __VALUECHANGEDIALOG_H
