/**
 * @file TuningAidUtils.cpp
 * @brief TuningAid segédeszközök implementációja
 *
 * Ez a fájl tartalmazza a fejlett CW és RTTY jelelemzési funkciókat,
 * amelyek kihasználják a javított FFT felbontást és interpolációs
 * technikákat a precíz frekvencia meghatározáshoz.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#include "TuningAidUtils.h"

#include <algorithm>

#include "FftInterpolation.h"

/**
 * @brief Maximális magnitúdójú bin keresése egy tartományban
 *
 * Lineáris keresést végez a megadott bin tartományon, hogy megtalálja
 * a legnagyobb magnitúdó értékkel rendelkező bin indexét.
 *
 * @param magnitudeData FFT magnitúdó adatok tömbje
 * @param minBin Keresési tartomány alsó határa (inclusive)
 * @param maxBin Keresési tartomány felső határa (inclusive)
 * @return A legnagyobb magnitúdójú bin indexe
 */
int TuningAidUtils::findPeakBin(const double* magnitudeData, int minBin, int maxBin) {
    // Hibás paraméterek ellenőrzése
    if (minBin >= maxBin || !magnitudeData) {
        return minBin;  // Hibás esetben az alsó határt adjuk vissza
    }

    int peakBin = minBin;                         // Kezdetben az első bin a csúcs
    double maxMagnitude = magnitudeData[minBin];  // Kezdeti maximum érték

    // Lineáris keresés a tartományon
    for (int i = minBin + 1; i <= maxBin; i++) {
        if (magnitudeData[i] > maxMagnitude) {
            maxMagnitude = magnitudeData[i];
            peakBin = i;
        }
    }
    return peakBin;
}

/**
 * @brief Precíz csúcs frekvencia keresése interpolációval
 *
 * Két lépésben működik:
 * 1. Megkeresi a legnagyobb magnitúdójú bin-t a tartományban
 * 2. Parabolikus interpolációt alkalmaz sub-bin pontosságért
 *
 * @param magnitudeData FFT magnitúdó adatok tömbje
 * @param minBin Keresési tartomány alsó határa
 * @param maxBin Keresési tartomány felső határa
 * @param binWidthHz FFT bin szélessége Hz-ben
 * @param fftSamples FFT minták száma az interpolációs határok ellenőrzéséhez
 * @return Precíz csúcs frekvencia Hz-ben
 */
float TuningAidUtils::findPrecisePeakFrequency(const double* magnitudeData, int minBin, int maxBin, float binWidthHz, int fftSamples) {
    // 1. lépés: Legnagyobb magnitúdójú bin megkeresése
    int peakBin = findPeakBin(magnitudeData, minBin, maxBin);

    // 2. lépés: Parabolikus interpoláció alkalmazása sub-bin pontosságért
    // Az interpoláció során a bin határok ellenőrzése az interpolateFrequencyPeak függvényben történik
    return interpolateFrequencyPeak(magnitudeData, peakBin, binWidthHz, fftSamples);
}

/**
 * @brief CW jel fejlett elemzése javított precizitással
 *
 * Komplex elemzési folyamat CW jelek számára:
 * 1. Frekvencia tartomány kiszámítása (centerFreq ± spanHz/2)
 * 2. Bin index határok meghatározása
 * 3. Precíz csúcs frekvencia keresése interpolációval
 * 4. Jelerősség mérése a csúcsnál
 * 5. Zaj szint becslése (csúcs körüli területet kizárva)
 * 6. Jel/zaj arány számítása
 * 7. Jel érvényességének validálása
 *
 * @param magnitudeData FFT magnitúdó adatok tömbje
 * @param fftSamples FFT minták száma
 * @param binWidthHz FFT bin szélessége Hz-ben
 * @param centerFreq Várható központi frekvencia (pl. 600 Hz CW-hez)
 * @param spanHz Elemzendő frekvencia tartomány szélessége (pl. 600 Hz)
 * @return SignalInfo struktúra a részletes elemzési eredményekkel
 */
TuningAidUtils::SignalInfo TuningAidUtils::analyzeCwSignal(const double* magnitudeData, int fftSamples, float binWidthHz, float centerFreq, float spanHz) {
    SignalInfo info;
    // Alapértelmezett értékek inicializálása
    info.isValid = false;
    info.peakFrequency = 0.0f;
    info.signalStrength = 0.0f;
    info.signalToNoiseRatio = 0.0f;

    // Frekvencia tartomány számítása (középpont ± fél span)
    float minFreq = centerFreq - spanHz / 2.0f;
    float maxFreq = centerFreq + spanHz / 2.0f;

    // Bin index határok meghatározása
    int minBin = std::max(1, static_cast<int>(minFreq / binWidthHz));
    int maxBin = std::min(fftSamples / 2 - 1, static_cast<int>(maxFreq / binWidthHz));

    // Érvényes tartomány ellenőrzése
    if (minBin >= maxBin) return info;  // Precíz csúcs frekvencia keresése interpolációval
    info.peakFrequency = findPrecisePeakFrequency(magnitudeData, minBin, maxBin, binWidthHz, fftSamples);

    // Jelerősség számítása a csúcs bin-nél
    int peakBin = findPeakBin(magnitudeData, minBin, maxBin);
    info.signalStrength = static_cast<float>(magnitudeData[peakBin]);

    // Zaj szint becslése (csúcs körüli területet kizárva a számításból)
    double noiseSum = 0.0;
    int noiseCount = 0;
    int peakExclusionRange = 3;  // ±3 bin kizárása a csúcs körül

    for (int i = minBin; i <= maxBin; i++) {
        // Csak a csúcstól távolabb lévő bin-eket számoljuk a zajba
        if (abs(i - peakBin) > peakExclusionRange) {
            noiseSum += magnitudeData[i];
            noiseCount++;
        }
    }

    // Jel/zaj arány számítása és validálás
    if (noiseCount > 0) {
        float noiseFloor = static_cast<float>(noiseSum / noiseCount);
        info.signalToNoiseRatio = (noiseFloor > 0) ? (info.signalStrength / noiseFloor) : 0.0f;

        // Jel érvényességének kritériumai:
        // - SNR > 3 (körülbelül 10dB)
        // - Jelerősség > 10 (minimális jel szint)
        info.isValid = (info.signalToNoiseRatio > 3.0f) && (info.signalStrength > 10.0f);
    }

    return info;
}

/**
 * @brief RTTY jel elemzése Mark/Space detektálással
 *
 * Speciálisan RTTY jelekre optimalizált elemzést végez:
 * 1. Mark frekvencia keresési tartomány meghatározása (±50 Hz tolerancia)
 * 2. Space frekvencia keresési tartomány meghatározása (±50 Hz tolerancia)
 * 3. Mark frekvencia precíz detektálása interpolációval
 * 4. Space frekvencia precíz detektálása interpolációval
 * 5. Frekvencia eltolás (shift) számítása
 * 6. Mindkét hangszín erősségének mérése
 * 7. RTTY jel érvényességének validálása
 *
 * @param magnitudeData FFT magnitúdó adatok tömbje
 * @param fftSamples FFT minták száma
 * @param binWidthHz FFT bin szélessége Hz-ben
 * @param expectedMarkFreq Várható Mark frekvencia (pl. 2295 Hz)
 * @param expectedSpaceFreq Várható Space frekvencia (pl. 2125 Hz)
 * @return RttyInfo struktúra a részletes RTTY elemzési eredményekkel
 */
TuningAidUtils::RttyInfo TuningAidUtils::analyzeRttySignal(const double* magnitudeData, int fftSamples, float binWidthHz, float expectedMarkFreq, float expectedSpaceFreq) {
    RttyInfo info;
    // Alapértelmezett értékek inicializálása
    info.isValid = false;
    info.markFrequency = 0.0f;
    info.spaceFrequency = 0.0f;
    info.shift = 0.0f;
    info.markStrength = 0.0f;
    info.spaceStrength = 0.0f;

    // Keresési tartományok meghatározása a várható frekvenciák körül (±50 Hz tolerancia)
    float tolerance = 50.0f;

    // Mark frekvencia elemzése
    float markMinFreq = expectedMarkFreq - tolerance;
    float markMaxFreq = expectedMarkFreq + tolerance;
    int markMinBin = std::max(1, static_cast<int>(markMinFreq / binWidthHz));
    int markMaxBin = std::min(fftSamples / 2 - 1, static_cast<int>(markMaxFreq / binWidthHz));
    if (markMinBin < markMaxBin) {
        info.markFrequency = findPrecisePeakFrequency(magnitudeData, markMinBin, markMaxBin, binWidthHz, fftSamples);
        int markPeakBin = findPeakBin(magnitudeData, markMinBin, markMaxBin);
        info.markStrength = static_cast<float>(magnitudeData[markPeakBin]);
    }

    // Space frekvencia elemzése
    float spaceMinFreq = expectedSpaceFreq - tolerance;
    float spaceMaxFreq = expectedSpaceFreq + tolerance;
    int spaceMinBin = std::max(1, static_cast<int>(spaceMinFreq / binWidthHz));
    int spaceMaxBin = std::min(fftSamples / 2 - 1, static_cast<int>(spaceMaxFreq / binWidthHz));
    if (spaceMinBin < spaceMaxBin) {
        info.spaceFrequency = findPrecisePeakFrequency(magnitudeData, spaceMinBin, spaceMaxBin, binWidthHz, fftSamples);
        int spacePeakBin = findPeakBin(magnitudeData, spaceMinBin, spaceMaxBin);
        info.spaceStrength = static_cast<float>(magnitudeData[spacePeakBin]);
    }

    // Frekvencia eltolás számítása és érvényességi ellenőrzés
    if (info.markFrequency > 0 && info.spaceFrequency > 0) {
        info.shift = abs(info.markFrequency - info.spaceFrequency);

        // RTTY jel érvényességének kritériumai:
        // 1. Mindkét frekvencia detektálva
        // 2. Shift ésszerű tartományban van (100-300 Hz)
        // 3. Mindkét jel megfelelő erősséggel rendelkezik
        info.isValid = (info.shift >= 100.0f && info.shift <= 300.0f) && (info.markStrength > 10.0f && info.spaceStrength > 10.0f);
    }

    return info;
}

/**
 * @brief Frekvencia hiba számítása hangolási útmutatáshoz
 *
 * Egyszerű frekvencia eltérés számítása, amely megmutatja,
 * hogy mennyivel tér el a detektált frekvencia a céltól.
 * Pozitív értékek azt jelentik, hogy le kell hangolni,
 * negatív értékek azt, hogy fel kell hangolni.
 *
 * @param actualFreq Detektált (aktuális) frekvencia Hz-ben
 * @param targetFreq Cél frekvencia Hz-ben
 * @return Frekvencia hiba Hz-ben (pozitív = hangolj le, negatív = hangolj fel)
 */
float TuningAidUtils::calculateFrequencyError(float actualFreq, float targetFreq) { return actualFreq - targetFreq; }

/**
 * @brief Hangolási javaslat meghatározása frekvencia hiba alapján
 *
 * A frekvencia hiba nagysága és iránya alapján meghatározza
 * a megfelelő hangolási javaslatot a felhasználó számára.
 * A javaslat kategóriákra osztja a hangolási mértéket.
 *
 * @param frequencyError Frekvencia hiba Hz-ben
 * @param tolerance Elfogadható tolerancia Hz-ben
 * @return Hangolási javaslat (TuningRecommendation enum)
 *
 * @note Logika:
 *       - Ha |hiba| <= tolerancia: OnTarget
 *       - Ha hiba > tolerancia: TuneDown (Small ha < 3×tolerancia, Large ha >= 3×tolerancia)
 *       - Ha hiba < -tolerancia: TuneUp (Small ha < 3×tolerancia, Large ha >= 3×tolerancia)
 */
TuningAidUtils::TuningRecommendation TuningAidUtils::getTuningRecommendation(float frequencyError, float tolerance) {
    float absError = abs(frequencyError);  // Hiba abszolút értéke

    if (absError <= tolerance) {
        // Hiba a tolerancián belül - jó hangolás
        return TuningRecommendation::OnTarget;
    } else if (frequencyError > tolerance) {
        // Pozitív hiba - le kell hangolni
        return (absError > tolerance * 3) ? TuningRecommendation::TuneDownLarge : TuningRecommendation::TuneDownSmall;
    } else {
        // Negatív hiba - fel kell hangolni
        return (absError > tolerance * 3) ? TuningRecommendation::TuneUpLarge : TuningRecommendation::TuneUpSmall;
    }
}
