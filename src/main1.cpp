#include <Arduino.h>         // Serial, millis stb. (debugoláshoz)
#include <pico/multicore.h>  // FIFO kommunikációhoz

#include "CwDecoder.h"           // CW dekóder osztály
#include "RttyDecoder.h"         // RTTY dekóder osztály
#include "core_communication.h"  // Parancsok definíciója
#include "defines.h"             // DEBUG makróhoz
#include "utils.h"

// A Core1 belső állapota a dekódolási módhoz
enum class Core1ActiveMode { MODE_OFF, MODE_RTTY, MODE_CW };
static Core1ActiveMode core1_current_mode = Core1ActiveMode::MODE_OFF;

// A Core1-specifikus dekóder példányok
static CwDecoder* core1_cw_decoder = nullptr;
static RttyDecoder* core1_rtty_decoder = nullptr;

/**
 * @brief Törli a Core1 dekódereit és erőforrásait.
 */
void deleteDecoders() {
    if (core1_cw_decoder) {
        delete core1_cw_decoder;
        core1_cw_decoder = nullptr;
    }
    if (core1_rtty_decoder) {
        delete core1_rtty_decoder;
        core1_rtty_decoder = nullptr;
    }
}

/**
 * Core1 belépési pontja
 */
void setup1() {
    // Core1 belépési pontja, itt indítjuk el a Core1 logikát
    DEBUG("Core1: Setup started.\n");

    // // Végtelen loop a Core1-en
    // FONTOS!!!: Ezt azért kell itt, mert felébredés után a Core1 nem indul újra automatikusan, csak ha itt van egy loop
    while (true) {
        loop1();
    }
}

/**
 * @brief Core1 logika futtatása a loop-ban.
 */
void loop1() {
    // Core1 logika indítása
    // Várakozás parancsra Core0-tól
    if (rp2040.fifo.available() > 0) {
        uint32_t raw_command;
        rp2040.fifo.pop_nb(&raw_command);
        Core1Command command = static_cast<Core1Command>(raw_command);
        // DEBUG("Core1: Command received: 0x%lX\n", raw_command);

        switch (command) {
            case CORE1_CMD_SET_MODE_OFF:
                DEBUG("Core1: Mode set to OFF by command\n");
                deleteDecoders();
                core1_current_mode = Core1ActiveMode::MODE_OFF;
                break;
            case CORE1_CMD_SET_MODE_RTTY:
                DEBUG("Core1: Mode set to RTTY by command\n");
                deleteDecoders();
                core1_current_mode = Core1ActiveMode::MODE_RTTY;

                // RTTY dekóder példányosítása a Core1-en
                core1_rtty_decoder = new RttyDecoder(AUDIO_INPUT_PIN);
                if (core1_rtty_decoder) {
                    core1_rtty_decoder->resetDecoderState();
                } else {
                    DEBUG("Core1: FATAL - Failed to create RttyDecoder instance!\n");
                }
                break;

            case CORE1_CMD_SET_MODE_CW:
                // DEBUG("Core1: Mode set to CW by command\n");
                deleteDecoders();
                core1_current_mode = Core1ActiveMode::MODE_CW;

                // CW dekóder példányosítása a Core1-en
                core1_cw_decoder = new CwDecoder(AUDIO_INPUT_PIN);
                if (core1_cw_decoder) {
                    core1_cw_decoder->resetDecoderState();
                } else {
                    DEBUG("Core1: FATAL - Failed to create CwDecoder instance!\n");
                }
                break;
            case CORE1_CMD_GET_RTTY_CHAR:
                if (core1_current_mode == Core1ActiveMode::MODE_RTTY && core1_rtty_decoder) {
                    char char_to_send_back = core1_rtty_decoder->getCharacterFromBuffer();
                    if (char_to_send_back != '\0') {
                        if (!rp2040.fifo.push_nb(static_cast<uint32_t>(char_to_send_back))) {
                            Utils::beepError();
                            DEBUG("Core1: RTTY command NOT sent to Core0, FIFO full\n");
                        }
                    }
                }
                break;

            case CORE1_CMD_GET_CW_CHAR:
                // Ez a parancs kéri le a karaktert a CwDecoder belső pufferéből
                if (core1_current_mode == Core1ActiveMode::MODE_CW and core1_cw_decoder) {
                    char char_to_send_back = core1_cw_decoder->getCharacterFromBuffer();
                    if (char_to_send_back != '\0') {
                        if (!rp2040.fifo.push_nb(static_cast<uint32_t>(char_to_send_back))) {
                            Utils::beepError();
                            DEBUG("Core1: CW command NOT sent to Core0, FIFO full\n");
                        }
                    }
                }
                break;

            default:
                DEBUG("Core1: Unknown command received: 0x%lX\n", raw_command);
                break;
        }
    } else {
        // Ha nincs parancs a Core0-tól, futtatjuk a megfelelő dekódert
        if (core1_current_mode == Core1ActiveMode::MODE_CW && core1_cw_decoder) {
            core1_cw_decoder->updateDecoder();  // Folyamatosan feldolgozza az audiót és tölti a belső puffert
        } else if (core1_current_mode == Core1ActiveMode::MODE_RTTY && core1_rtty_decoder) {
            core1_rtty_decoder->updateDecoder();  // Folyamatosan feldolgozza az RTTY audiót és tölti a belső puffert
        }

        // Rövid várakozás, hogy ne pörgesse túl a CPU-t, ha nincs más teendő
        // Ezt az értéket finomhangolni kellhet a dekódolási sebesség és a rendszer válaszkészsége alapján.
        // Ha túl nagy, lassú lehet a dekódolás. Ha túl kicsi, feleslegesen terheli a CPU-t.
        // A CwDecoder belső időzítései (pl. sampleWithNoiseBlanking) határozzák meg a tényleges audio feldolgozási sebességet.
        // Ez a delay csak akkor releváns, ha a dekóder gyorsabban végezne, mint ahogy új adat érkezik,
        // vagy ha más feladatok is futnának a Core1-en.
        delayMicroseconds(1000);  // 1 ms várakozás, ha nincs FIFO parancs
    }
}