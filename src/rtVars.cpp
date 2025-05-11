#include "rtVars.h"

namespace rtv {

// Némítás
bool mute = false;

// Frekvencia kijelzés pozíciója
uint16_t freqDispX = 0;
uint16_t freqDispY = 0;

// Frekvencia lépés, a kijelző digitje alatti aláhúzással jelezve
uint8_t freqstepnr = 0;    // A frekvencia kijelző digit száma, itt jelezzük SSB/CW-ben, hogy mi a frekvencia lépés
                           // freqstepnr==0 -> freqstep = 1000 Hz;
                           // freqstepnr==1 -> freqstep = 100 Hz;
                           // freqstepnr==2 -> freqstep = 10 Hz;
uint16_t freqstep = 1000;  // A frekvencia lépés értéke Hz-ben
int16_t freqDec = 0;       // A frekvencia változtatásának mértéke és iránya

// BFO
bool bfoOn = false;  // BFO mód kezdetben inaktív
bool bfoTr = false;  // BFO kijelző animáció trigger
// int16_t currentBFOmanu = 0; // Ezt a configban tároljuk
// uint8_t currentBFOStep = 25; // Ezt a configban tároljuk

// Mute
bool muteStat = false;

// Squelch
long squelchDecay = 0;

// Scan
bool SCANbut = false;   // Scan aktív?
bool SCANpause = true;  // LWH - SCANpause must be initialized to a value else the squelch function will

// Seek
bool SEEK = false;

// CW shift
bool CWShift = false;

}  // namespace rtv