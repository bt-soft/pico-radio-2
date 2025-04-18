#ifndef __FREQSCANDISPLAY_H
#define __FREQSCANDISPLAY_H

#include <vector>  // std::vector használatához

#include "DisplayBase.h"

class FreqScanDisplay : public DisplayBase {

   protected:
    /**
     * Rotary encoder esemény lekezelése
     */
    bool handleRotary(RotaryEncoder::EncoderState encoderState) override;

    /**
     * Touch (nem képernyő button) esemény lekezelése
     */
    bool handleTouch(bool touched, uint16_t tx, uint16_t ty) override;

    /**
     * Esemény nélküli display loop
     */
    void displayLoop() override;

    /**
     * Képernyő menügomb esemény feldolgozása
     */
    void processScreenButtonTouchEvent(TftButton::ButtonTouchEvent &event) override;

   public:
    FreqScanDisplay(TFT_eSPI &tft, SI4735 &si4735, Band &band);
    ~FreqScanDisplay();

    /**
     * Képernyő kirajzolása
     * (Az esetleges dialóg eltűnése után a teljes képernyőt újra rajzoljuk)
     */
    void drawScreen() override;

    /**
     * Aktuális képernyő típusának lekérdezése
     */
    inline DisplayBase::DisplayType getDisplayType() override { return DisplayBase::DisplayType::freqScan; };

   private:
    // --- Konstansok ---
    // A spektrum mérete és pozíciója (igazodik a 480x320 kijelzőhöz)
    static constexpr int spectrumX = 5;         // Bal oldali margó
    static constexpr int spectrumY = 60;        // Y pozíció
    static constexpr int spectrumWidth = 470;   // Szélesség
    static constexpr int spectrumHeight = 180;  // Magasság

    // Számított konstansok (ezek automatikusan frissülnek)
    static constexpr int spectrumEndY = spectrumY + spectrumHeight;
    static constexpr int spectrumEndScanX = spectrumX + spectrumWidth;

    // --- Állapotváltozók (sample.cpp alapján) ---
    bool scanning = false;          // Szkennelés folyamatban van?
    bool scanPaused = true;         // Szkennelés szüneteltetve?
    bool scanEmpty = true;          // A spektrum adatok üresek?
    uint16_t currentFrequency = 0;  // Jelenlegi frekvencia a szkenneléshez/hangoláshoz
    uint16_t startFrequency = 0;    // Szkennelés kezdő frekvenciája (teljes sáv)
    uint16_t endFrequency = 0;      // Szkennelés végfrekvenciája (teljes sáv)
    float scanStep = 0.0f;          // Aktuális lépésköz (kHz) pixelre vetítve
    float minScanStep = 0.125f;     // Minimális lépésköz
    float maxScanStep = 8.0f;       // Maximális lépésköz
    bool autoScanStep = true;       // Automatikus lépésköz?
    bool scanAccuracy = true;       // Szkennelés pontossága (befolyásolja a countScanSignal-t)
    int countScanSignal = 3;        // Hány mérés átlaga legyen egy ponton
    uint8_t scanAGC = 0;            // AGC állapota a szkennelés indításakor

    // Spektrum adatok
    std::vector<uint8_t> scanValueRSSI;  // RSSI értékek (Y koordináták)
    std::vector<uint8_t> scanValueSNR;   // SNR értékek
    std::vector<bool> scanMark;          // Jelölők (pl. erős jel)
    std::vector<uint8_t> scanScaleLine;  // Skálavonal típusok

    // Pozícionálás és skálázás
    float currentScanLine = 0.0f;     // Az aktuális frekvenciának megfelelő X pozíció a spektrumon (piros kurzor)
    float deltaScanLine = 0.0f;       // Eltolás a spektrumon (pásztázás) - lépésekben a startFrequency-től a középig
    float signalScale = 1.5f;         // Jelerősség skálázási faktor (nagyítás)
    int posScan = 0;                  // Aktuális szkennelési pozíció (index)
    int posScanLast = 0;              // Előző szkennelési pozíció
    uint16_t posScanFreq = 0;         // Az aktuális szkennelési pozíciónak megfelelő frekvencia
    int scanBeginBand = -1;           // Sáv elejének X koordinátája (-1, ha látható)
    int scanEndBand = spectrumWidth;  // Sáv végének X koordinátája (kezdetben a spektrum vége)
    uint8_t scanMarkSNR = 3;          // SNR küszöb a jelöléshez
    bool prevScaleLine = false;       // Segédváltozó a skálavonal rajzoláshoz

    int prevTouchedX = -1;  // Előző érintett oszlop X koordinátája a spektrumon (sárga kurzor)
    // int prevRssiY = -1;     // Előző RSSI Y koordináta a vonalrajzoláshoz (már nem használt)

    // --- ÚJ: Változók az érintéses húzáshoz ---
    int dragStartX = -1;                              // A húzás kezdő X koordinátája
    int lastDragX = -1;                               // Az előző X koordináta húzás közben
    bool isDragging = false;                          // Húzás folyamatban van?
    unsigned long touchStartTime = 0;                 // Érintés kezdetének ideje
    static const unsigned long tapMaxDuration = 200;  // ms - Max időtartam, ami még tap-nak számít
    static const int dragMinDistance = 5;             // pixel - Minimális elmozdulás, ami már húzásnak számít
    // --- ÚJ VÉGE ---

    // --- Metódusok (sample.cpp alapján) ---
    void drawScanGraph(bool erase);  // Spektrum alapjának és skálájának rajzolása
    void drawScanLine(int xPos);     // Spektrum rajzolása (X pozíció alapján) - kurzor nélkül
    void drawScanText(bool all);     // Frekvencia címkék rajzolása
    void displayScanSignal();        // Aktuális RSSI/SNR kiírása
    int getSignal(bool rssi);        // Jelerősség (RSSI vagy SNR) lekérése (átlagolással)
    void setFreq(uint16_t f);        // Frekvencia beállítása
    void freqUp();                   // Frekvencia léptetése felfelé
    void pauseScan();                // Szkennelés szüneteltetése/folytatása
    void startScan();                // Szkennelés indítása
    void stopScan();                 // Szkennelés leállítása
    void changeScanScale();          // Szkennelési skála (lépésköz) váltása

    // --- ÚJ KURZOR KEZELŐ FÜGGVÉNYEK ---
    void eraseCursor(int xPos);       // Visszarajzolja az alapot kurzor nélkül
    void drawYellowCursor(int xPos);  // Kirajzolja a sárga kurzort
    void drawRedCursor(int xPos);     // Kirajzolja a piros kurzort
    void redrawCursors();             // Újraszámolja és újrarajzolja a megfelelő kurzort
    // --- ÚJ VÉGE ---
};

#endif  //__FREQSCANDISPLAY_H
