#ifndef RTTYDECODER_H
#define RTTYDECODER_H

#include "AudioProcessor.h"  // Szükséges az audio feldolgozáshoz
#include "defines.h"         // DEBUG eléréséhez

class RttyDecoder {
   public:
    RttyDecoder(AudioProcessor& audioProcessor);
    ~RttyDecoder();

    char decodeNextCharacter();  // Dekódolja a következő karaktert az audio bemenetből

    void setFigsShift(bool shift) { figsShift = shift; }

    bool getFigsShift() const { return figsShift; }

   private:
    AudioProcessor& audioProcessor;
    bool figsShift = false;  // Baudot LTRS/FIGS shift állapota

    // RTTY paraméterek (kezdetben fixen)
    static constexpr float RTTY_BAUD_RATE = 45.45f;       // Standard Baud rate
    static constexpr float RTTY_SHIFT_HZ = 170.0f;        // Standard Shift
    static constexpr float RTTY_MARK_FREQ_HZ = 2125.0f;   // Standard Mark frekvencia
    static constexpr float RTTY_SPACE_FREQ_HZ = 2295.0f;  // Standard Space frekvencia

    // RTTY dekódolási állapotgép változói
    enum RttyState { IDLE, WAITING_FOR_START_BIT, RECEIVING_DATA_BIT, RECEIVING_STOP_BIT };

    RttyState currentState = IDLE;
    unsigned long lastBitTime = 0;
    int bitsReceived = 0;
    uint8_t currentByte = 0;

    // Baudot -> ASCII lookup tábla (egyszerűsített)
    char baudotToAscii[2][32];  // [0] = LTRS, [1] = FIGS

    void initBaudotToAscii();
    char decodeBaudot(uint8_t byte, bool& figsShift);

    bool detectMarkSpace(double& markMagnitude, double& spaceMagnitude);
    void getSignalState(bool& outIsSignalPresent, bool& outIsMarkTone);  // Deklaráció hozzáadása

    friend class AmDisplay;  // Hozzáférés a tagváltozókhoz
};

#endif