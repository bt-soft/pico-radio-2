#ifndef __BAND_H
#define __BAND_H

#include <SI4735.h>

#include "Config.h"
#include "rtVars.h"

// Band index
#define FM_BAND_TYPE 0
#define MW_BAND_TYPE 1
#define SW_BAND_TYPE 2
#define LW_BAND_TYPE 3

// DeModulation types
#define FM 0
#define LSB 1
#define USB 2
#define AM 3
#define CW 4

#define CW_DEMOD_MODE LSB       // CW demodulációs mód = LSB + 700Hz offset
#define CW_SHIFT_FREQUENCY 700  // CW alap offset

struct BandTable {
    // BandTable állandó része (Nincs értelme a PROGMEM-ben tárolni, Pico-nál nincs ilyen)
    const char *bandName;        // Sáv neve (PROGMEM mutató)
    const uint8_t bandType;      // Sáv típusa (FM, MW, LW vagy SW)
    const uint16_t prefMod;      // Preferált moduláció (AM, FM, USB, LSB, CW)
    const uint16_t minimumFreq;  // A sáv minimum frekvenciája
    const uint16_t maximumFreq;  // A sáv maximum frekvenciája
    const uint16_t defFreq;      // Alapértelmezett frekvencia vagy aktuális frekvencia
    const uint8_t defStep;       // Alapértelmezett lépésköz (növelés/csökkentés)
    const bool isHam;            // HAM sáv-e?

    // BandTable változó része
    uint16_t currFreq;    // Aktuális frekvencia
    uint8_t currStep;     // Aktuális lépésköz (növelés/csökkentés)
    uint8_t currMod;      // Aktuális mód/modulációs típus (FM, AM, LSB, USB, CW)
    uint16_t antCap;      // Antenna Tuning Capacitor
    int16_t lastBFO;      // Utolsó BFO érték
    int16_t lastmanuBFO;  // Utolsó manuális BFO érték X-Tal segítségével
};

// Sávszélesség struktúra (Címke és Érték)
struct BandWidth {
    const char *label;  // Megjelenítendő felirat
    uint8_t index;      // Az si4735-nek átadandó index
};

// Lépésköz struktúra (Címke és Érték)
struct FrequencyStep {
    const char *label;  // Megjelenítendő felirat (pl. "1kHz")
    uint8_t value;      // A lépésköz értéke (pl. 1, 5, 9, 10, 100)
};

/**
 * Band class
 */
class Band {
   private:
    // Si4735 referencia
    SI4735 &si4735;

    // SSB betöltve?
    bool ssbLoaded = false;

    void setBandWidth();
    void loadSSB();

   public:
    // BandMode description
    static const char *bandModeDesc[5];

    // Sávszélesség struktúrák tömbjei
    static const BandWidth bandWidthFM[5];
    static const BandWidth bandWidthAM[7];
    static const BandWidth bandWidthSSB[6];

    // Lépésköz konfigurációk (érték beállításához)
    static const FrequencyStep stepSizeAM[4];
    static const FrequencyStep stepSizeFM[3];

    Band(SI4735 &si4735);
    virtual ~Band() = default;

    /**
     *
     */
    void bandInit(bool sysStart = false);

    /**
     * Band beállítása
     * @param useDefaults default adatok betültése?
     */
    void bandSet(bool useDefaults = false);

    /**
     * A Default Antenna Tuning Capacitor értékének lekérdezése
     * @return Az alapértelmezett antenna tuning capacitor értéke
     */
    uint16_t getDefaultAntCapValue() {

        // Kikeressük az aktuális Band rekordot
        BandTable &currentBand = getCurrentBand();
        uint8_t currentBandType = getCurrentBandType();
        switch (currentBandType) {

            case FM_BAND_TYPE:
                return 0;  // FM band esetén nincs antenna tuning capacitor

            case MW_BAND_TYPE:
            case LW_BAND_TYPE:
                return 0;  // Sima AM esetén sem kell antenna tuning capacitor

            case SW_BAND_TYPE:
                return 1;  // SW band esetén antenna tuning capacitor szükséges

            default:
                return 0;  // Alapértelmezett érték
        }
    }

    /**
     * Band beállítása
     */
    void useBand();

    /**
     * A Band egy rekordjának elkérése az index alapján
     * @param bandIdx a band indexe
     * @return A BandTableVar rekord referenciája, vagy egy üres rekord, ha nem található
     */
    BandTable &getBandByIdx(uint8_t bandIdx);

    /**
     *
     */
    inline BandTable &getCurrentBand() { return getBandByIdx(config.data.bandIdx); }

    /**
     * A Band indexének elkérése a bandName alapján
     *
     * @param bandName A keresett sáv neve
     * @return A BandTable rekord indexe, vagy -1, ha nem található
     */
    int8_t getBandIdxByBandName(const char *bandName);

    /**
     * Aktuális mód/modulációs típus (FM, AM, LSB, USB, CW)
     */
    inline const char *getCurrentBandModeDesc() { return bandModeDesc[getCurrentBand().currMod]; }

    /**
     * A lehetséges AM demodulációs módok kigyűjtése
     */
    inline const char **getAmDemodulationModes(uint8_t &count) {
        // count = sizeof(bandModeDesc) / sizeof(bandModeDesc[0]) - 1;
        count = ARRAY_ITEM_COUNT(bandModeDesc) - 1;
        return &bandModeDesc[1];
    }

    /**
     * Az aktuális sávszélesség labeljének lekérdezése
     * @return A sávszélesség labelje, vagy nullptr, ha nem található
     */
    const char *getCurrentBandWidthLabel() {
        const char *p;
        uint8_t currMod = getCurrentBand().currMod;
        if (currMod == AM) p = getCurrentBandWidthLabelByIndex(bandWidthAM, config.data.bwIdxAM);
        if (currMod == LSB or currMod == USB or currMod == CW) p = getCurrentBandWidthLabelByIndex(bandWidthSSB, config.data.bwIdxSSB);
        if (currMod == FM) p = getCurrentBandWidthLabelByIndex(bandWidthFM, config.data.bwIdxFM);

        return p;
    }

    /**
     * Sávszélesség tömb labeljeinek visszaadása
     * @param bandWidth A sávszélesség tömbje
     * @param count A tömb elemeinek száma
     * @return A label-ek tömbje
     */
    template <size_t N>
    const char **getBandWidthLabels(const BandWidth (&bandWidth)[N], size_t &count) {
        count = N;  // A tömb mérete
        static const char *labels[N];
        for (size_t i = 0; i < N; i++) {
            labels[i] = bandWidth[i].label;
        }
        return labels;
    }

    /**
     * A sávszélesség labeljének lekérdezése az index alapján
     * @param bandWidth A sávszélesség tömbje
     * @param index A keresett sávszélesség indexe
     * @return A sávszélesség labelje, vagy nullptr, ha nem található
     */
    template <size_t N>
    const char *getCurrentBandWidthLabelByIndex(const BandWidth (&bandWidth)[N], uint8_t index) {
        for (size_t i = 0; i < N; i++) {
            if (bandWidth[i].index == index) {
                return bandWidth[i].label;  // Megtaláltuk a labelt
            }
        }
        return nullptr;  // Ha nem található
    }

    /**
     * A sávszélesség indexének lekérdezése a label alapján
     * @param bandWidth A sávszélesség tömbje
     * @param label A keresett sávszélesség labelje
     * @return A sávszélesség indexe, vagy -1, ha nem található
     */
    template <size_t N>
    int8_t getBandWidthIndexByLabel(const BandWidth (&bandWidth)[N], const char *label) {
        for (size_t i = 0; i < N; i++) {
            if (strcmp(label, bandWidth[i].label) == 0) {
                return bandWidth[i].index;  // Megtaláltuk az indexet
            }
        }
        return -1;  // Ha nem található
    }

    /**
     * Lépésköz tömb labeljeinek visszaadása
     * @param bandWidth A lépésköz tömbje
     * @param count A tömb elemeinek száma
     * @return A label-ek tömbje
     */
    template <size_t N>
    const char **getStepSizeLabels(const FrequencyStep (&stepSizeTable)[N], size_t &count) {
        count = N;  // A tömb mérete
        static const char *labels[N];
        for (size_t i = 0; i < N; i++) {
            labels[i] = stepSizeTable[i].label;
        }
        return labels;
    }

    /**
     * A lépésköz labeljének lekérdezése az index alapján
     * @param bandWidth A lépésköz tömbje
     * @param index A keresett lépésköz indexe
     * @return A lépésköz labelje, vagy nullptr, ha nem található
     */
    template <size_t N>
    const char *getStepSizeLabelByIndex(const FrequencyStep (&stepSizeTable)[N], uint8_t index) {
        // Ellenőrizzük, hogy az index érvényes-e a tömbhöz
        if (index < N) {
            return stepSizeTable[index].label;  // Közvetlenül visszaadjuk a labelt az index alapján
        }
        return nullptr;  // Ha az index érvénytelen
    }

    /**
     * Aktuális frekvencia lépésköz felirat megszerzése
     */
    const char *currentStepSizeStr() {

        static const char *currentStepStr = nullptr;

        uint8_t currentBandType = getCurrentBandType();
        if (currentBandType == FM_BAND_TYPE) {
            currentStepStr = getStepSizeLabelByIndex(Band::stepSizeFM, config.data.ssIdxFM);
        } else {
            uint8_t index = (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) ? config.data.ssIdxMW : config.data.ssIdxAM;
            currentStepStr = getStepSizeLabelByIndex(Band::stepSizeAM, index);
        }

        return currentStepStr;
    }

    inline const char *getCurrentBandName() { return (const char *)pgm_read_ptr(&getCurrentBand().bandName); }

    inline const uint8_t getCurrentBandType() { return getCurrentBand().bandType; }

    inline const uint16_t getCurrentBandMinimumFreq() { return getCurrentBand().minimumFreq; }

    inline const uint16_t getCurrentBandMaximumFreq() { return getCurrentBand().maximumFreq; }

    inline const uint16_t getCurrentBandDefaultFreq() { return getCurrentBand().defFreq; }

    inline const uint8_t getCurrentBandDefaultStep() { return getCurrentBand().defStep; }

    inline const bool getCurrentBandIsHam() { return getCurrentBand().isHam; }

    /**
     *
     */
    const char **getBandNames(uint8_t &count, bool isHamFilter);
};

#endif  // __BAND_H