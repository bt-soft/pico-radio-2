#ifndef __IDIALOGPARENT_H
#define __IDIALOGPARENT_H

#include "TftButton.h"

/**
 *
 */
class IDialogParent {

   public:
    /**
     * Dialog button adat átadása a szülő képernyőnek
     * (Nem referenciát adunk át, mert a dialóg megszűniuk majd!!)
     */
    virtual void setDialogResponse(TftButton::ButtonTouchEvent event) = 0;

    /**
     * Cancelt vagy 'X'-et nyomtak a dialogon?
     */
    virtual bool isDialogResponseCancelOrCloseX() = 0;
};

#endif  // __IDIALOGPARENT_H