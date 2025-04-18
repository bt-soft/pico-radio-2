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
     * Rotary encoder esemény lekezelése
     */
    virtual bool handleRotary(RotaryEncoder::EncoderState encoderState) { return false; };

    /**
     * Touch esemény lekezelése
     * true, ha valaki rámozdult az eseményre
     */
    virtual bool handleTouch(bool touched, uint16_t tx, uint16_t ty) { return false; };
};

#endif  // __IGUIEVENTS_H
