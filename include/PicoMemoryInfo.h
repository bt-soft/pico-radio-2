#ifndef __PICOMEMORYINFO_H
#define __PICOMEMORYINFO_H

#include <RP2040Support.h>

#include "utils.h"

#define FULL_FLASH_SIZE 2093056      // Teljes flash memória méret (2MB)
#define FULL_MEMORY_SIZE 262144      // Teljes RAM méret (heap) byte-ban
extern "C" char __flash_binary_end;  // A program végét jelző cím

// Struktúra a memóriaállapot tárolására
struct MemoryStatus_t {
    // Flash memória (programtár)
    uint32_t programSize;
    float programPercent;
    uint32_t freeFlash;
    float freeFlashPercent;

    // Heap memória (RAM)
    uint32_t heapSize;
    uint32_t usedHeap;
    float usedHeapPercent;
    uint32_t freeHeap;
    float freeHeapPercent;
};

#ifdef __DEBUG
#define MEASUREMENTS_COUNT 20  // A memóriahasználat mérésének száma

/**
 * A memóriahasználat mérésének tárolása
 */
struct UsedHeapMemoryMonitor {
    uint32_t measurements[MEASUREMENTS_COUNT];  // A memóriahasználatot tároló tömb
    int8_t index = -1;                          // Az aktuális mérési index -1-re állítjuk be a kezdeti értéket!!
    bool valid = false;

    UsedHeapMemoryMonitor() {
        // Kezdeti értékek beállítása 0-ra
        for (uint8_t i = 0; i < MEASUREMENTS_COUNT; i++) {
            measurements[i] = 0;
        }
    }

    // Adott heap használat mérésének hozzáadása
    void addMeasurement(uint32_t usedHeap) {

        // Ha az index elérte a maximális értéket, akkor elölről kezdjük
        index = (index + 1) % MEASUREMENTS_COUNT;  // Körkörös indexelés

        // Eltároljuk a mért Heap értéket
        measurements[index] = usedHeap;

        // Ha már van 2 adatunk, akkor rendben vagyunk
        if (!valid and index > 0) {
            valid = true;
        }
    }

    // Kiszámítja az átlagos memóriahasználatot
    float getAverageUsedHeap() {

        // Legalább 2 mérés szükséges
        if (!valid) {
            return 0.0f;
        }

        uint32_t total = 0;
        uint8_t count = valid ? MEASUREMENTS_COUNT : index;

        for (uint8_t i = 0; i < count; i++) {
            uint8_t currIndex = (index + i) % MEASUREMENTS_COUNT;
            total += measurements[currIndex];
        }

        return static_cast<float>(total) / count;
    }

    // Kiszámítja az előző méréshez képest a memóriahasználat változását
    // Az előző és a legutóbbi mérés közötti különbség, legalább 2 mérés szükséges
    // Lehet negatív is
    int32_t getChangeFromPreviousMeasurement() {

        if (!valid) {
            return 0;
        }

        // Az előző mérés indexének helyes számítása körkörös puffer esetén
        uint8_t prevIndex = (index == 0) ? MEASUREMENTS_COUNT - 1 : index - 1;
        // DEBUG("value(%d) %.2f kB, prevValue(%d): %.2f kB -> canged: %.2f kB\n",
        //       index, measurements[index] / 1024.0,
        //       prevIndex, measurements[prevIndex] / 1024.0,
        //       (static_cast<int32_t>(measurements[index]) - static_cast<int32_t>(measurements[prevIndex])) / 1024.0);

        return measurements[index] - measurements[prevIndex];
    }
};

// Globális memóriafigyelő objektum, csak DEBUG módban
UsedHeapMemoryMonitor usedHeapMemoryMonitor;

#endif

/**
 * Memóriaállapot lekérdezése
 */
MemoryStatus_t getMemoryStatus() {
    MemoryStatus_t status;

    // Flash memória méretének meghatározása
    status.programSize = (uint32_t)&__flash_binary_end - 0x10000000;
    status.programPercent = (status.programSize * 100.0) / FULL_FLASH_SIZE;
    status.freeFlash = FULL_FLASH_SIZE - status.programSize;
    status.freeFlashPercent = 100.0 - status.programPercent;

    // Heap memória (RAM)
    RP2040 rp2040;
    status.heapSize = rp2040.getTotalHeap();
    status.usedHeap = rp2040.getUsedHeap();
    status.freeHeap = rp2040.getFreeHeap();

    // Százalékszámítás a heap teljes méretére vonatkozóan
    status.usedHeapPercent = (status.usedHeap * 100.0) / status.heapSize;
    status.freeHeapPercent = (status.freeHeap * 100.0) / status.heapSize;

#ifdef __DEBUG
    // Used Heap mért érték hozzáadása
    usedHeapMemoryMonitor.addMeasurement(status.usedHeap);
#endif

    return status;
}

/**
 * Debug módban az adatok kiírása
 */
#ifdef __DEBUG
void debugMemoryInfo() {

    MemoryStatus_t status = getMemoryStatus();  // Adatok lekérése

    Serial.flush();

    DEBUG("===== Memory info =====\n");

    // Program memória (flash)
    DEBUG("Flash\t\t\t\t\t\tHeap\n");
    DEBUG("Total: %d B (%.2f kB)\t\t\t%d B (%.2f kB)\n", FULL_FLASH_SIZE, FULL_FLASH_SIZE / 1024.0,  // Flash
          status.heapSize, status.heapSize / 1024.0                                                  // Heap
    );
    DEBUG("Used: %d B (%.2f kB) - %.2f%%\t\t%d B (%.2f kB) - %.2f%%\n", status.programSize, status.programSize / 1024.0, status.programPercent,  // Flash
          status.usedHeap, status.usedHeap / 1024.0, status.usedHeapPercent                                                                      // Heap
    );
    DEBUG("Free: %d B (%.2f kB) - %.2f%%\t\t%d B (%.2f kB) - %.2f%%\n", status.freeFlash, status.freeFlash / 1024.0, status.freeFlashPercent,  // Flash
          status.freeHeap, status.freeHeap / 1024.0, status.freeHeapPercent                                                                    // Heap
    );

    DEBUG("Heap usage:\n changed(from prev): %.2f kB, ave: %.2f kB - (%d/%d)\n",
          usedHeapMemoryMonitor.getChangeFromPreviousMeasurement() / 1024.0,  // Az előző méréshez képesti változás
          usedHeapMemoryMonitor.getAverageUsedHeap() / 1024.0,                // average
          usedHeapMemoryMonitor.index, MEASUREMENTS_COUNT                     // max grow
    );

    DEBUG("---\n");
    DEBUG("\n");
    Serial.flush();
}
#endif

#endif  //__PICOMEMORYINFO_H
