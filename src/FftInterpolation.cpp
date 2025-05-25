/**
 * @file FftInterpolation.cpp
 * @brief FFT bin interpolációs függvények implementációja
 *
 * Ez a fájl tartalmazza a parabolikus interpolációs algoritmus implementációját,
 * amely az FFT bin-ek közötti pontos frekvencia meghatározást teszi lehetővé.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#include "FftInterpolation.h"

#include <Arduino.h>  // constrain függvény

#include <cmath>  // abs függvény

#include "AudioProcessor.h"  // AudioProcessorConstants

/**
 * @brief FFT bin interpoláció a TuningAid pontosabb frekvencia meghatározásához
 *
 * Parabolikus illesztést végez három szomszédos FFT bin magnitúdó értéke alapján.
 * Az algoritmus a következő lépéseket hajtja végre:
 * 1. Ellenőrzi a bin index határokat
 * 2. Kinyeri a három szomszédos bin értéket (y1, y2, y3)
 * 3. Parabolikus egyenlet illesztése: y = ax² + bx + c
 * 4. A parabola csúcsának megkeresése: x_peak = -b/(2a)
 * 5. Interpolált bin index számítása és korlátozása
 * 6. Frekvencia konvertálás
 *
 * @param magnitudeData FFT magnitúdó adatok tömbje
 * @param binIndex Központi bin index (ahol a csúcs van)
 * @param binWidthHz Egy bin szélessége Hz-ben
 * @return Interpolált frekvencia Hz-ben
 *
 * @note Ha a bin a szélen van vagy a parabola illesztés sikertelen,
 *       akkor az eredeti bin frekvenciát adja vissza
 */
float interpolateFrequencyPeak(const double* magnitudeData, int binIndex, float binWidthHz, int fftSamples) {
    // Határellenőrzés: legalább 1 bin távolság kell a szélek!
    if (binIndex <= 0 || binIndex >= fftSamples / 2 - 1) {
        return binIndex * binWidthHz;  // Szélső esetben nincs interpoláció lehetséges
    }

    // Három szomszédos bin magnitúdó értékének kinyerése
    double y1 = magnitudeData[binIndex - 1];  // Bal szomszéd
    double y2 = magnitudeData[binIndex];      // Központi bin (csúcs)
    double y3 = magnitudeData[binIndex + 1];  // Jobb szomszéd

    // Parabolikus illesztés: y = ax² + bx + c
    // Három pont alapján: (-1,y1), (0,y2), (1,y3)
    // A csúcs x koordinátája: x_peak = -b/(2a)
    double a = (y1 + y3 - 2 * y2) / 2.0;  // Másodrendű tag együtthatója
    double b = (y3 - y1) / 2.0;           // Elsőrendű tag együtthatója

    // Ellenőrizzük, hogy van-e érvényes parabola (a != 0)
    if (abs(a) < 1e-10) {
        return binIndex * binWidthHz;  // Nincs parabola, nincs interpoláció lehetséges
    }

    // A parabola csúcsának x koordinátája
    double x_peak = -b / (2.0 * a);

    // Interpolált bin index (relatív eltolás a központi bin-től)
    double interpolated_bin = binIndex + x_peak;

    // Biztonsági korlátozás: maximum ±0.5 bin eltérés a központtól
    interpolated_bin = constrain(interpolated_bin, binIndex - 0.5, binIndex + 0.5);

    // Konvertálás frekvenciára
    return interpolated_bin * binWidthHz;
}
