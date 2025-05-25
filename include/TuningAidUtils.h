/**
 * @file TuningAidUtils.h
 * @brief Fejlett TuningAid elemzési segédeszközök
 *
 * Ez a modul speciális függvényeket biztosít a CW és RTTY jelek
 * részletes elemzéséhez, kihasználva a javított FFT felbontást
 * és a sub-bin interpolációt a precíz frekvencia meghatározáshoz.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#ifndef TUNING_AID_UTILS_H
#define TUNING_AID_UTILS_H

/**
 * @brief Segédeszközök osztály a fejlett TuningAid elemzéshez
 *
 * Ez az osztály fejlett jelelemzési funkciókat biztosít, amelyek kihasználják
 * a javított FFT felbontást és a sub-bin interpolációt a precíz frekvencia
 * meghatározáshoz CW és RTTY jeleknél. Az osztály statikus metódusokat tartalmaz,
 * így példányosítás nélkül használható.
 */
class TuningAidUtils {
   public:
    /**
     * @brief CW jel információk struktúrája
     *
     * Tartalmazza a detektált CW jelről összegyűjtött információkat,
     * beleértve a precíz frekvenciát, jelerősséget és jel/zaj arányt.
     */
    struct SignalInfo {
        bool isValid;             /**< @brief Igaz, ha érvényes jel detektálva */
        float peakFrequency;      /**< @brief Precíz csúcs frekvencia Hz-ben (interpolációval) */
        float signalStrength;     /**< @brief Jel magnitúdója a csúcsnál */
        float signalToNoiseRatio; /**< @brief Jel/zaj arány becslése */
    };

    /**
     * @brief RTTY jel információk struktúrája
     *
     * Tartalmazza a detektált RTTY jelről összegyűjtött információkat,
     * beleértve a Mark és Space frekvenciákat, valamint azok erősségét.
     */
    struct RttyInfo {
        bool isValid;         /**< @brief Igaz, ha érvényes RTTY jel detektálva */
        float markFrequency;  /**< @brief Mark hangszín frekvenciája (Hz) */
        float spaceFrequency; /**< @brief Space hangszín frekvenciája (Hz) */
        float shift;          /**< @brief Frekvencia eltolás (Hz) */
        float markStrength;   /**< @brief Mark jel erőssége */
        float spaceStrength;  /**< @brief Space jel erőssége */
    };

    /**
     * @brief Hangolási javaslatok felsorolása
     *
     * Meghatározza a felhasználó számára javasolt hangolási irányt
     * és mértéket a frekvencia hiba alapján.
     */
    enum class TuningRecommendation {
        OnTarget,      /**< @brief Frekvencia a tolerancián belül van */
        TuneUpSmall,   /**< @brief Kismértékben hangolj fel */
        TuneUpLarge,   /**< @brief Nagymértékben hangolj fel */
        TuneDownSmall, /**< @brief Kismértékben hangolj le */
        TuneDownLarge  /**< @brief Nagymértékben hangolj le */
    };

    /**
     * @brief Maximális magnitúdójú bin keresése egy tartományban
     *
     * Végigmegy a megadott bin tartományon és megkeresi azt a bin-t,
     * amely a legnagyobb magnitúdó értékkel rendelkezik.
     *
     * @param magnitudeData FFT magnitúdó adatok tömbje
     * @param minBin Keresési tartomány alsó határa (bin index)
     * @param maxBin Keresési tartomány felső határa (bin index)
     * @return A legnagyobb magnitúdójú bin indexe
     *
     * @note Ha minBin >= maxBin vagy az adatok érvénytelenek, minBin-t adja vissza
     */
    static int findPeakBin(const double* magnitudeData, int minBin, int maxBin);

    /**
     * @brief Precíz csúcs frekvencia keresése interpolációval
     *
     * Megkeresi a legnagyobb magnitúdójú bin-t a megadott tartományban,
     * majd parabolikus interpolációt alkalmaz a pontos frekvencia meghatározásához.
     * Ez sub-bin pontosságot biztosít.
     *
     * @param magnitudeData FFT magnitúdó adatok tömbje
     * @param minBin Keresési tartomány alsó határa (bin index)
     * @param maxBin Keresési tartomány felső határa (bin index)
     * @param binWidthHz FFT bin szélessége Hz-ben
     * @return Precíz csúcs frekvencia Hz-ben (sub-bin pontossággal)
     */
    static float findPrecisePeakFrequency(const double* magnitudeData, int minBin, int maxBin, float binWidthHz);

    /**
     * @brief CW jel fejlett elemzése javított precizitással
     *
     * Komplex elemzést végez a CW jeleken, beleértve:
     * - Precíz frekvencia meghatározást interpolációval
     * - Jelerősség mérését
     * - Zaj szint becslését és jel/zaj arány számítást
     * - Jel érvényességének validálását
     *
     * @param magnitudeData FFT magnitúdó adatok tömbje
     * @param fftSamples FFT minták száma
     * @param binWidthHz FFT bin szélessége Hz-ben
     * @param centerFreq Várható központi frekvencia
     * @param spanHz Elemzendő frekvencia tartomány szélessége
     * @return SignalInfo struktúra az elemzés eredményeivel
     *
     * @note A jelet érvényesnek tekinti, ha SNR > 3 (kb. 10dB) és jelerősség > 10
     */
    static SignalInfo analyzeCwSignal(const double* magnitudeData, int fftSamples, float binWidthHz, float centerFreq, float spanHz);

    /**
     * @brief RTTY jel elemzése Mark/Space detektálással
     *
     * Speciálisan RTTY jelekre optimalizált elemzést végez:
     * - Mark frekvencia precíz detektálása
     * - Space frekvencia precíz detektálása
     * - Frekvencia eltolás (shift) számítása
     * - Mindkét hangszín erősségének mérése
     * - RTTY jel érvényességének validálása
     *
     * @param magnitudeData FFT magnitúdó adatok tömbje
     * @param fftSamples FFT minták száma
     * @param binWidthHz FFT bin szélessége Hz-ben
     * @param expectedMarkFreq Várható Mark frekvencia
     * @param expectedSpaceFreq Várható Space frekvencia
     * @return RttyInfo struktúra az elemzés eredményeivel
     *
     * @note Az RTTY jelet érvényesnek tekinti, ha:
     *       - Mindkét frekvencia detektálva
     *       - Shift 100-300 Hz tartományban van
     *       - Mindkét jel erőssége > 10
     */
    static RttyInfo analyzeRttySignal(const double* magnitudeData, int fftSamples, float binWidthHz, float expectedMarkFreq, float expectedSpaceFreq);

    /**
     * @brief Frekvencia hiba számítása hangolási útmutatáshoz
     *
     * Egyszerű frekvencia eltérés számítása, amely megmutatja,
     * hogy mennyivel tér el a detektált frekvencia a céltól.
     *
     * @param actualFreq Detektált frekvencia
     * @param targetFreq Cél frekvencia
     * @return Frekvencia hiba Hz-ben (pozitív = hangolj le, negatív = hangolj fel)
     */
    static float calculateFrequencyError(float actualFreq, float targetFreq); /**
                                                                               * @brief Hangolási javaslat meghatározása frekvencia hiba alapján
                                                                               *
                                                                               * Elemzi a frekvencia hibát és meghatározza a megfelelő hangolási
                                                                               * javaslatot a felhasználó számára. A javaslat függ a hiba nagyságától
                                                                               * és irányától.
                                                                               *
                                                                               * @param frequencyError Frekvencia hiba Hz-ben
                                                                               * @param tolerance Elfogadható tolerancia Hz-ben
                                                                               * @return Hangolási javaslat (TuningRecommendation enum)
                                                                               *
                                                                               * @note - Ha |hiba| <= tolerancia: OnTarget
                                                                               *       - Ha hiba > tolerancia: TuneDown (Small/Large)
                                                                               *       - Ha hiba < -tolerancia: TuneUp (Small/Large)
                                                                               *       - Large javaslat, ha |hiba| > 3 * tolerancia
                                                                               */
    static TuningRecommendation getTuningRecommendation(float frequencyError, float tolerance);
};

#endif  // TUNING_AID_UTILS_H
