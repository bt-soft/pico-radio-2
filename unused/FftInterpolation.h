/**
 * @file FftInterpolation.h
 * @brief FFT bin interpolációs függvények a TuningAid precíziójának javításához
 *
 * Ez a modul parabolikus interpolációt valósít meg az FFT bin-ek között,
 * hogy pontosabb frekvencia meghatározást tegyen lehetővé. Különösen hasznos
 * CW és RTTY jelek esetében, ahol a frekvencia pontosság kritikus.
 *
 * @author Pico Radio Projekt
 * @date 2025
 */

#ifndef FFT_INTERPOLATION_H
#define FFT_INTERPOLATION_H

/**
 * @brief FFT bin interpoláció a TuningAid pontosabb frekvencia meghatározásához
 *
 * Parabolikus illesztést használ három szomszédos FFT bin magnitúdó értéke alapján
 * a valós csúcs frekvencia pontosabb meghatározásához. Az algoritmus egy parabola
 * egyenletet illeszt a három pontra, majd megkeresi a parabola maximumát.
 *
 * Ez különösen hasznos CW és RTTY jelek esetén, ahol nagy frekvencia pontosság
 * szükséges a helyes hangoláshoz. A módszer sub-bin pontosságot biztosít, amely
 * jelentősen javítja a frekvencia diszkriminációt.
 * * @param magnitudeData FFT magnitúdó adatok tömbje (double típusú)
 * @param binIndex Központi bin index (a csúcs bin), ahol az interpolációt végezzük
 * @param binWidthHz Egy FFT bin szélessége Hz-ben
 * @param fftSamples FFT minták száma (használt az határellenőrzéshez)
 * @return Interpolált frekvencia Hz-ben (sub-bin pontossággal)
 *
 * @note A függvény ellenőrzi a bin határokat és szélső esetekben
 *       az eredeti bin frekvenciát adja vissza interpoláció nélkül
 */
float interpolateFrequencyPeak(const double* magnitudeData, int binIndex, float binWidthHz, int fftSamples);

#endif  // FFT_INTERPOLATION_H
