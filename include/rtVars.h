#ifndef __RTVARS_H
#define __RTVARS_H

#include <cstdint>

/**
 *
 */
namespace rtv {

// Némítás
extern bool mute;

// Frekvencia kijelzés pozíciója
extern uint16_t freqDispX;
extern uint16_t freqDispY;

extern uint8_t freqstepnr;  // A frekvencia kijelző digit száma, itt jelezzük SSB/CW-ben, hogy mi a frekvencia lépés
extern uint16_t freqstep;
extern int16_t freqDec;

// BFO
extern bool bfoOn;      // BFO mód aktív?
extern bool bfoTr;      // BFO kijelző animáció trigger
// extern int16_t currentBFOmanu; // Ezt a configban tároljuk: config.data.currentBFOmanu
// extern uint8_t currentBFOStep; // Ezt a configban tároljuk: config.data.currentBFOStep

// Mute
#define AUDIO_MUTE_ON true
#define AUDIO_MUTE_OFF false
extern bool muteStat;

// Squelch
#define SQUELCH_DECAY_TIME 500
#define MIN_SQUELCH 0
#define MAX_SQUELCH 50
extern long squelchDecay;

// Scan
extern bool SCANbut;
extern bool SCANpause;

// Seek
extern bool SEEK;

// CW shift
extern bool CWShift;

}  // namespace rtv

#endif  //__RTVARS_H
