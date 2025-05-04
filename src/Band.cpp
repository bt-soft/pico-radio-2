#include "Band.h"

#include <patch_full.h>  // SSB patch for whole SSBRX full download

#include "rtVars.h"

// PROGMEM - ben tárolt állandó tábla
const BandTableConst bandTableConst[] = {
    {"FM", FM_BAND_TYPE, FM, 6400, 10800, 9390, 10, false},    //  FM          0   // 93.9MHz Petőfi
    {"LW", LW_BAND_TYPE, AM, 100, 514, 198, 9, false},         //  LW          1
    {"MW", MW_BAND_TYPE, AM, 514, 1800, 540, 9, false},        //  MW          2   // 540kHz Kossuth
    {"800m", SW_BAND_TYPE, AM, 280, 470, 284, 1, true},        // Ham  800M    3
    {"630m", SW_BAND_TYPE, LSB, 470, 480, 475, 1, true},       // Ham  630M    4
    {"160m", SW_BAND_TYPE, LSB, 1800, 2000, 1850, 1, true},    // Ham  160M    5
    {"120m", SW_BAND_TYPE, AM, 2000, 3200, 2400, 5, false},    //      120M    6
    {"90m", SW_BAND_TYPE, AM, 3200, 3500, 3300, 5, false},     //       90M    7
    {"80m", SW_BAND_TYPE, LSB, 3500, 3900, 3630, 1, true},     // Ham   80M    8
    {"75m", SW_BAND_TYPE, AM, 3900, 5300, 3950, 5, false},     //       75M    9
    {"60m", SW_BAND_TYPE, USB, 5300, 5900, 5375, 1, true},     // Ham   60M   10
    {"49m", SW_BAND_TYPE, AM, 5900, 7000, 6000, 5, false},     //       49M   11
    {"40m", SW_BAND_TYPE, LSB, 7000, 7500, 7074, 1, true},     // Ham   40M   12
    {"41m", SW_BAND_TYPE, AM, 7200, 9000, 7210, 5, false},     //       41M   13
    {"31m", SW_BAND_TYPE, AM, 9000, 10000, 9600, 5, false},    //       31M   14
    {"30m", SW_BAND_TYPE, USB, 10000, 10100, 10100, 1, true},  // Ham   30M   15
    {"25m", SW_BAND_TYPE, AM, 10200, 13500, 11700, 5, false},  //       25M   16
    {"22m", SW_BAND_TYPE, AM, 13500, 14000, 13700, 5, false},  //       22M   17
    {"20m", SW_BAND_TYPE, USB, 14000, 14500, 14074, 1, true},  // Ham   20M   18
    {"19m", SW_BAND_TYPE, AM, 14500, 17500, 15700, 5, false},  //       19M   19
    {"17m", SW_BAND_TYPE, AM, 17500, 18000, 17600, 5, false},  //       17M   20
    {"16m", SW_BAND_TYPE, USB, 18000, 18500, 18100, 1, true},  // Ham   16M   21
    {"15m", SW_BAND_TYPE, AM, 18500, 21000, 18950, 5, false},  //       15M   22
    {"14m", SW_BAND_TYPE, USB, 21000, 21500, 21074, 1, true},  // Ham   14M   23
    {"13m", SW_BAND_TYPE, AM, 21500, 24000, 21500, 5, false},  //       13M   24
    {"12m", SW_BAND_TYPE, USB, 24000, 25500, 24940, 1, true},  // Ham   12M   25
    {"11m", SW_BAND_TYPE, AM, 25500, 26100, 25800, 5, false},  //       11M   26
    {"CB", SW_BAND_TYPE, AM, 26100, 28000, 27200, 1, false},   // CB band     27
    {"10m", SW_BAND_TYPE, USB, 28000, 30000, 28500, 1, true},  // Ham   10M   28
    {"SW", SW_BAND_TYPE, AM, 100, 30000, 15500, 5, false}      // Whole SW    29
};

/// Itt határozzuk meg a BAND_COUNT értékét!
const size_t BANDTABLE_COUNT = ARRAY_ITEM_COUNT(bandTableConst);

// A Kombinált tábla RAM-ban
BandTable bandTable[BANDTABLE_COUNT];

// BandMode description
const char* Band::bandModeDesc[5] = {"FM", "LSB", "USB", "AM", "CW"};

// Sávszélesség struktúrák tömbjei - ez indexre állítódik az si4735-ben!
const BandWidth Band::bandWidthFM[] = {{"AUTO", 0}, {"110", 1}, {"84", 2}, {"60", 3}, {"40", 4}};
const BandWidth Band::bandWidthAM[] = {{"1.0", 4}, {"1.8", 5}, {"2.0", 3}, {"2.5", 6}, {"3.0", 2}, {"4.0", 1}, {"6.0", 0}};
const BandWidth Band::bandWidthSSB[] = {{"0.5", 4}, {"1.0", 5}, {"1.2", 0}, {"2.2", 1}, {"3.0", 2}, {"4.0", 3}};

// AM Lépésköz
const FrequencyStep Band::stepSizeAM[] = {
    {"1kHz", 1},   // "1kHz" -> 1
    {"5kHz", 5},   // "5kHz" -> 5
    {"9kHz", 9},   // "9kHz" -> 9
    {"10kHz", 10}  // "10kHz" -> 10
};
// FM Lépésköz
const FrequencyStep Band::stepSizeFM[] = {
    {"50kHz", 5},    // "50kHz" -> 5
    {"100kHz", 10},  // "100kHz" -> 10
    {"1MHz", 100}    // "1MHz" -> 100
};

// BFO Lépésköz
const FrequencyStep Band::stepSizeBFO[] = {
    {"1Hz", 1},
    {"5Hz", 5},
    {"10Hz", 10},
    {"25Hz", 25},
};

/**
 * Konstruktor
 */
Band::Band(SI4735& si4735, Config& configRef) : si4735(si4735), configRef(configRef) {

    // A BandTable inicializálása
    for (uint8_t i = 0; i < BANDTABLE_COUNT; i++) {

        // Flash adatok pointerének beállítása
        bandTable[i].pConstData = &bandTableConst[i];  // PROGMEM mutató beállítása, hátha kell

        // RAM Változó adatok inicializálása
        //  Ha még nem volt az EEPROM mentésből visszaállítás, akkor most bemásoljuk a default értékeket a változó értékekbe
        if (bandTable[i].varData.currFreq == 0) {
            bandTable[i].varData.currFreq = bandTableConst[i].defFreq;  // Frekvencia
            bandTable[i].varData.currStep = bandTableConst[i].defStep;  // Lépés
            bandTable[i].varData.currMod = bandTableConst[i].prefMod;   // Moduláció

            // Antenna tunning capacitor
            if (bandTableConst[i].bandType != FM_BAND_TYPE or bandTableConst[i].bandType != MW_BAND_TYPE or bandTableConst[i].bandType != LW_BAND_TYPE) {
                bandTable[i].varData.antCap = 0;  // Antenna tunning capacitor
            } else {
                bandTable[i].varData.antCap = 1;
            }

            bandTable[i].varData.lastBFO = 0;
            bandTable[i].varData.lastmanuBFO = 0;
        }
    }
}

/**
 * A Band egy rekordjának elkérése az index alapján
 *
 * @param bandIdx A keresett sáv indexe
 * @return A BandTable rekord referenciája, vagy egy üres rekord, ha nem található
 */
BandTable& Band::getBandByIdx(uint8_t bandIdx) {

    static BandTable emptyBand = {nullptr, {0, 0, 0, 0}};  // Üres rekord
    if (bandIdx >= BANDTABLE_COUNT) return emptyBand;      // Érvénytelen index esetén üres rekordot adunk vissza

    // return bandTableVar[index];
    return bandTable[bandIdx];  // Egyébként visszaadjuk a megfelelő rekordot
}

/**
 * A Band indexének elkérése a bandName alapján
 *
 * @param bandName A keresett sáv neve
 * @return A BandTable rekord indexe, vagy -1, ha nem található
 */
int8_t Band::getBandIdxByBandName(const char* bandName) {

    for (size_t i = 0; i < BANDTABLE_COUNT; i++) {
        if (strcmp(bandName, bandTableConst[i].bandName) == 0) {
            return i;  // Megtaláltuk az indexet
        }
    }
    return -1;  // Ha nem található
}

/**
 * Sávok neveinek visszaadása tömbként
 *
 * @param count talált elemek száma
 * @param isHamFilter HAM szűrő
 */
const char** Band::getBandNames(uint8_t& count, bool isHamFilter) {

    static const char* filteredNames[BANDTABLE_COUNT];  // Tároló a kiválasztott nevekre
    count = 0;                                          // Kezdőérték

    for (size_t i = 0; i < BANDTABLE_COUNT; i++) {
        if (bandTableConst[i].isHam == isHamFilter) {             // HAM sáv szűrés
            filteredNames[count++] = bandTableConst[i].bandName;  // Közvetlen pointer hozzáadás
        }
    }

    return filteredNames;  // A pointert visszaadjuk
}

/**
 * SSB patch betöltése
 */
void Band::loadSSB() {

    DEBUG("Band::loadSSB()\n");

    // Ha már be van töltve, akkor nem megyünk tovább
    if (ssbLoaded) {
        DEBUG("Band::loadSSB() -> SSB már be van töltve\n");
        return;
    }

    si4735.reset();
    si4735.queryLibraryId();  // Is it really necessary here? I will check it.
    si4735.patchPowerUp();
    delay(50);

    si4735.setI2CFastMode();  // Recommended
    si4735.downloadPatch(ssb_patch_content, sizeof(ssb_patch_content));
    si4735.setI2CStandardMode();  // goes back to default (100KHz)
    delay(50);

    // Parameters
    // AUDIOBW - SSB Audio bandwidth; 0 = 1.2KHz (default); 1=2.2KHz; 2=3KHz; 3=4KHz; 4=500Hz; 5=1KHz;
    // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
    // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
    // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
    // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
    // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
    si4735.setSSBConfig(config.data.bwIdxSSB, 1, 0, 1, 0, 1);
    delay(25);
    ssbLoaded = true;
}

/**
 * Band beállítása
 */
void Band::useBand() {

    // Kikeressük az aktuális Band rekordot
    BandTable& currentBand = getCurrentBand();
    uint8_t currentBandType = getCurrentBandType();

    DEBUG("Band::useBand() -> bandName: %s currStep: %d, currentMode: %s\n", getCurrentBandName(), currentBand.varData.currStep, getCurrentBandModeDesc());

    //---- CurrentStep beállítása a band rekordban

    // Index ellenőrzés (biztonsági okokból)
    uint8_t stepIndex;

    // AM esetén 1...1000Khz között bármi lehet - {"1kHz", "5kHz", "9kHz", "10kHz"};
    if (currentBandType == MW_BAND_TYPE or currentBandType == LW_BAND_TYPE) {
        // currentBand.currentStep = static_cast<uint8_t>(atoi(Band::stepSizeAM[config.data.ssIdxMW]));
        stepIndex = config.data.ssIdxMW;
        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeAM)) {
            DEBUG("Hiba: Érvénytelen ssIdxMW index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0;                    // Visszaállás alapértelmezettre (pl. 1kHz)
            config.data.ssIdxMW = stepIndex;  // Opcionális: Konfig frissítése
        }
        currentBand.varData.currStep = stepSizeAM[stepIndex].value;

    } else if (currentBandType == SW_BAND_TYPE) {
        // currentBand.currentStep = static_cast<uint8_t>(atoi(Band::stepSizeAM[config.data.ssIdxAM]));
        // AM/SSB/CW Shortwave esetén
        stepIndex = config.data.ssIdxAM;
        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeAM)) {
            DEBUG("Hiba: Érvénytelen ssIdxAM index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0;                    // Visszaállás alapértelmezettre
            config.data.ssIdxAM = stepIndex;  // Opcionális: Konfig frissítése
        }
        currentBand.varData.currStep = stepSizeAM[stepIndex].value;

    } else {
        // FM esetén csak 3 érték lehet - {"50Khz", "100KHz", "1MHz"};
        // static_cast<uint8_t>(atoi(Band::stepSizeFM[config.data.ssIdxFM]));
        stepIndex = config.data.ssIdxFM;
        // Határellenőrzés
        if (stepIndex >= ARRAY_ITEM_COUNT(stepSizeFM)) {
            DEBUG("Hiba: Érvénytelen ssIdxFM index: %d. Alapértelmezett használata.\n", stepIndex);
            stepIndex = 0;                    // Visszaállás alapértelmezettre
            config.data.ssIdxFM = stepIndex;  // Opcionális: Konfig frissítése
        }
        currentBand.varData.currStep = stepSizeFM[stepIndex].value;
    }

    if (currentBandType == FM_BAND_TYPE) {
        ssbLoaded = false;
        rtv::bfoOn = false;

        // Antenna tuning capacitor beállítása (FM esetén antenna tuning capacitor nem kell)
        currentBand.varData.antCap = getDefaultAntCapValue();
        si4735.setTuneFrequencyAntennaCapacitor(currentBand.varData.antCap);
        // delay(100);

        si4735.setFM(currentBand.pConstData->minimumFreq, currentBand.pConstData->maximumFreq, currentBand.varData.currFreq, currentBand.varData.currStep);
        si4735.setFMDeEmphasis(1);  // 1 = 50 μs. Usedin Europe, Australia, Japan;  2 = 75 μs. Used in USA (default)
        si4735.RdsInit();
#define RDS_ENABLE 1
#define RDS_BLOCK_ERROR_TRESHOLD 2
        si4735.setRdsConfig(RDS_ENABLE, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD, RDS_BLOCK_ERROR_TRESHOLD);

    } else {

        // AM-ben vagyunk
        currentBand.varData.antCap = getDefaultAntCapValue();  // Sima AM esetén antenna tuning capacitor nem kell
        si4735.setTuneFrequencyAntennaCapacitor(currentBand.varData.antCap);

        if (ssbLoaded) {
            // SSB vagy CW mód

            bool isCWMode = (currentBand.varData.currMod == CW);

            // Mód beállítása (LSB-t használunk alapnak CW-hez)
            uint8_t modeForChip = isCWMode ? LSB : currentBand.varData.currMod;
            si4735.setSSB(currentBand.pConstData->minimumFreq, currentBand.pConstData->maximumFreq, currentBand.varData.currFreq,
                          1,  // SSB/CW esetén a step mindig 1kHz a chipen belül
                          modeForChip);

            // BFO beállítása
            // CW mód: Fix BFO offset (pl. 700 Hz) + manuális finomhangolás
            const int16_t cwBaseOffset = isCWMode ? CW_SHIFT_FREQUENCY : 0;  // Alap CW eltolás
            si4735.setSSBBfo(cwBaseOffset + config.data.currentBFO + config.data.currentBFOmanu);
            rtv::CWShift = isCWMode;  // Jelezzük a kijelzőnek

            // SSB/CW esetén a lépésköz a chipen mindig 1kHz, de a finomhangolás BFO-val történik
            currentBand.varData.currStep = 1;
            si4735.setFrequencyStep(currentBand.varData.currStep);

        } else {
            // Sima AM mód
            si4735.setAM(currentBand.pConstData->minimumFreq, currentBand.pConstData->maximumFreq, currentBand.varData.currFreq, currentBand.varData.currStep);
            // si4735.setAutomaticGainControl(1, 0);
            // si4735.setAmSoftMuteMaxAttenuation(0); // // Disable Soft Mute for AM
            rtv::bfoOn = false;
            rtv::CWShift = false;  // AM módban biztosan nincs CW shift
        }
    }
}

/**
 * Sávszélesség beállítása
 */
void Band::setBandWidth() {

    DEBUG("Band::setBandWidth()\n");

    BandTable& currentBand = getCurrentBand();
    uint8_t currMod = currentBand.varData.currMod;

    if (currMod == LSB or currMod == USB or currMod == CW) {
        /**
         * @ingroup group17 Patch and SSB support
         *
         * @brief SSB Audio Bandwidth for SSB mode
         *
         * @details 0 = 1.2 kHz low-pass filter  (default).
         * @details 1 = 2.2 kHz low-pass filter.
         * @details 2 = 3.0 kHz low-pass filter.
         * @details 3 = 4.0 kHz low-pass filter.
         * @details 4 = 500 Hz band-pass filter for receiving CW signal, i.e. [250 Hz, 750 Hz] with center
         * frequency at 500 Hz when USB is selected or [-250 Hz, -750 1Hz] with center frequency at -500Hz
         * when LSB is selected* .
         * @details 5 = 1 kHz band-pass filter for receiving CW signal, i.e. [500 Hz, 1500 Hz] with center
         * frequency at 1 kHz when USB is selected or [-500 Hz, -1500 1 Hz] with center frequency
         *     at -1kHz when LSB is selected.
         * @details Other values = reserved.
         *
         * @details If audio bandwidth selected is about 2 kHz or below, it is recommended to set SBCUTFLT[3:0] to 0
         * to enable the band pass filter for better high- cut performance on the wanted side band. Otherwise, set it to 1.
         *
         * @see AN332 REV 0.8 UNIVERSAL PROGRAMMING GUIDE; page 24
         *
         * @param AUDIOBW the valid values are 0, 1, 2, 3, 4 or 5; see description above
         */
        si4735.setSSBAudioBandwidth(config.data.bwIdxSSB);

        // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
        if (config.data.bwIdxSSB == 0 or config.data.bwIdxSSB == 4 or config.data.bwIdxSSB == 5) {
            // Band pass filter to cutoff both the unwanted side band and high frequency components > 2.0 kHz of the wanted side band. (default)
            si4735.setSSBSidebandCutoffFilter(0);
        } else {
            // Low pass filter to cutoff the unwanted side band.
            si4735.setSSBSidebandCutoffFilter(1);
        }

    } else if (currMod == AM) {
        /**
         * @ingroup group08 Set bandwidth
         * @brief Selects the bandwidth of the channel filter for AM reception.
         * @details The choices are 6, 4, 3, 2, 2.5, 1.8, or 1 (kHz). The default bandwidth is 2 kHz. It works only in AM / SSB (LW/MW/SW)
         * @see Si47XX PROGRAMMING GUIDE; AN332 (REV 1.0); pages 125, 151, 277, 181.
         * @param AMCHFLT the choices are:   0 = 6 kHz Bandwidth
         *                                   1 = 4 kHz Bandwidth
         *                                   2 = 3 kHz Bandwidth
         *                                   3 = 2 kHz Bandwidth
         *                                   4 = 1 kHz Bandwidth
         *                                   5 = 1.8 kHz Bandwidth
         *                                   6 = 2.5 kHz Bandwidth, gradual roll off
         *                                   7–15 = Reserved (Do not use).
         * @param AMPLFLT Enables the AM Power Line Noise Rejection Filter.
         */
        si4735.setBandwidth(config.data.bwIdxAM, 0);

    } else if (currMod == FM) {
        /**
         * @brief Sets the Bandwith on FM mode
         * @details Selects bandwidth of channel filter applied at the demodulation stage. Default is automatic which means the device automatically selects proper channel filter.
         * <BR>
         * @details | Filter  | Description |
         * @details | ------- | -------------|
         * @details |    0    | Automatically select proper channel filter (Default) |
         * @details |    1    | Force wide (110 kHz) channel filter |
         * @details |    2    | Force narrow (84 kHz) channel filter |
         * @details |    3    | Force narrower (60 kHz) channel filter |
         * @details |    4    | Force narrowest (40 kHz) channel filter |
         *
         * @param filter_value
         */
        si4735.setFmBandwidth(config.data.bwIdxFM);
    }
}

/**
 * Band inicializálása konfig szerint
 */
void Band::bandInit(bool sysStart) {

    DEBUG("Band::BandInit() ->bandIdx: %d\n", config.data.bandIdx);
    BandTable& curretBand = getCurrentBand();

    if (getCurrentBandType() == FM_BAND_TYPE) {
        si4735.setup(PIN_SI4735_RESET, FM_BAND_TYPE);
        si4735.setFM();

        si4735.setSeekFmSpacing(10);
        si4735.setSeekFmLimits(curretBand.pConstData->minimumFreq, curretBand.pConstData->maximumFreq);
        si4735.setSeekAmRssiThreshold(50);
        si4735.setSeekAmSrnThreshold(20);
        si4735.setSeekFmRssiThreshold(5);
        si4735.setSeekFmSrnThreshold(5);

    } else {
        si4735.setup(PIN_SI4735_RESET, MW_BAND_TYPE);
        si4735.setAM();
    }

    // Rendszer indítás van?
    if (sysStart) {

        // rtv::freqstep = 1000;  // 1kHz
        rtv::freqDec = config.data.currentBFO;
        curretBand.varData.lastBFO = config.data.currentBFO;
        // curretBand.prefMod = config.data.currentMode;
        // curretBand.varData.currFreq = config.data.currentFreq;

        si4735.setVolume(config.data.currVolume);  // Hangerő
    }
}

/**
 * Band beállítása
 * @param useDefaults prefereált demodulációt betöltsük?
 */
void Band::bandSet(bool useDefaults) {

    DEBUG("Band::bandSet(useDefaults: %s)\n", useDefaults ? "true" : "false");

    // Kikeressük az aktuális Band rekordot
    BandTable& currentBand = getCurrentBand();

    // Demoduláció beállítása
    uint8_t currMod = currentBand.varData.currMod;

    // A sávhoz preferált demodulációs módot állítunk be?
    if (useDefaults) {
        // Átmásoljuk a preferált modulációs módot
        currMod = currentBand.varData.currMod = currentBand.pConstData->prefMod;
        ssbLoaded = false;  // SSB patch betöltése szükséges
    }

    if (currMod == AM or currMod == FM) {

        ssbLoaded = false;  // FIXME: Ez kell? Band váltás után megint be kell tölteni az SSB-t?

    } else if (currMod == LSB or currMod == USB or currMod == CW) {
        if (ssbLoaded == false) {
            this->loadSSB();
        }
    }

    useBand();
    setBandWidth();

    // Antenna Tunning Capacitor beállítása
    si4735.setTuneFrequencyAntennaCapacitor(currentBand.varData.antCap);
}

/**
 *
 */
void Band::tuneMemoryStation(uint16_t frequency, int16_t bfoOffset, uint8_t bandIndex, uint8_t demodModIndex, uint8_t bandwidthIndex) {

    // 1. Elkérjük a Band táblát
    config.data.bandIdx = bandIndex;  // Band index beállítása
    BandTable& currentBand = this->getCurrentBand();

    // 2. Demodulátor beállítása a chipen.  Ha CW módra váltunk, akkor nullázzuk a finomhangolási BFO-t
    uint8_t savedMod = demodModIndex;  // A demodulációs mód kiemelése
    if (savedMod != CW and rtv::CWShift == true) {
        currentBand.varData.lastBFO = 0;
        config.data.currentBFO = currentBand.varData.lastBFO;
        rtv::CWShift = false;
    }
    currentBand.varData.currMod = demodModIndex;  // Átállítjuk a demodulációs módot

    // 3. Sávszélesség index beállítása a configban a MENTETT érték alapján ---
    uint8_t savedBwIndex = bandwidthIndex;
    if (savedMod == FM) {
        config.data.bwIdxFM = savedBwIndex;
    } else if (savedMod == AM) {
        config.data.bwIdxAM = savedBwIndex;
    } else {  // LSB, USB, CW
        config.data.bwIdxSSB = savedBwIndex;
    }

    // 4. Újra beállítjuk a sávot az új móddal (false -> ne a preferált adatokat töltse be)
    this->bandSet(false);

    // 5. Explicit módon állítsd be a frekvenciát és a módot a chipen
    currentBand.varData.currFreq = frequency;
    si4735.setFrequency(currentBand.varData.currFreq);

    // BFO eltolás visszaállítása SSB/CW esetén ---
    if (demodModIndex == LSB || demodModIndex == USB || demodModIndex == CW) {
        currentBand.varData.lastBFO = bfoOffset;  // Mentett BFO visszaállítása a sáv változóba
        config.data.currentBFO = bfoOffset;       // Mentett BFO visszaállítása az aktuális hangolási változóba
        rtv::freqDec = bfoOffset;                 // Rotary változó szinkronizálása

        const int16_t cwBaseOffset = (demodModIndex == CW) ? CW_SHIFT_FREQUENCY : 0;
        // A visszaállított BFO (+ az AKTUÁLIS manuális finomítás) használata
        int16_t bfoToSet = cwBaseOffset + config.data.currentBFO + config.data.currentBFOmanu;
        si4735.setSSBBfo(bfoToSet);
        rtv::CWShift = (demodModIndex == CW);  // CW shift állapot frissítése

    } else {
        // AM/FM esetén biztosítjuk, hogy a BFO nullázva legyen
        currentBand.varData.lastBFO = 0;
        config.data.currentBFO = 0;
        rtv::freqDec = 0;
        rtv::CWShift = false;
    }

    // 6. Hangerő visszaállítása
    si4735.setVolume(config.data.currVolume);
}
