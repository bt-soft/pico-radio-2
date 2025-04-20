#ifndef __IGUIEVENTS_H
#define __IGUIEVENTS_H

#include "RotaryEncoder.h"

/**
 *
 */
class IGuiEvents {

   public:
    // Virtuális destruktor a származtatott osztályok megfelelő kezeléséhez
    virtual ~IGuiEvents() = default;

    /**
     * Esemény nélküli képernyőfrissítés ciklus, a leszármazott felülírhatja
     */
    virtual void displayLoop() {};

    /**
     * Rotary encoder esemény lekezelése, a leszármazott felülírhatja
     */
    virtual bool handleRotary(RotaryEncoder::EncoderState encoderState) { return false; };

    /**
     * Touch esemény lekezelése, a leszármazott felülírhatja
     * true, ha valaki rámozdult az eseményre
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; };
};

#endif  // __IGUIEVENTS_H
