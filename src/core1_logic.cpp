#include "core1_logic.h"

#include <Arduino.h>         // Serial, millis stb. (debugoláshoz)
#include <pico/multicore.h>  // FIFO kommunikációhoz

#include "CwDecoder.h"           // CW dekóder osztály
#include "core_communication.h"  // Parancsok definíciója
#include "defines.h"             // DEBUG makróhoz

// A Core1 belső állapota a dekódolási módhoz
enum class Core1ActiveMode { MODE_OFF, MODE_RTTY, MODE_CW };
static Core1ActiveMode core1_current_mode = Core1ActiveMode::MODE_OFF;

// A Core1-specifikus AudioProcessor és dekóder példányok
// static AudioProcessor* core1_audio_processor = nullptr;  // Későbbi használatra
// static RttyDecoder* core1_rtty_decoder = nullptr;        // Későbbi használatra
static CwDecoder* core1_cw_decoder = nullptr;

void core1_main() {
    multicore_fifo_drain();  // Drain any stale data in our RX FIFO at startup
    DEBUG("Core1: Main loop started.\n");

    // CW dekóder példányosítása a Core1-en
    core1_cw_decoder = new CwDecoder(AUDIO_INPUT_PIN);
    if (!core1_cw_decoder) {
        DEBUG("Core1: FATAL - Failed to create CwDecoder instance!\n");
        // Itt valamilyen hibajelzés/leállás kellene, ha a memória elfogyott
    }

    // Ide fognak kerülni később a Core1-specifikus AudioProcessor és dekóder példányok
    // (a komment áthelyezve a jobb olvashatóságért)

    while (true) {
        // Várakozás parancsra Core0-tól
        if (multicore_fifo_rvalid()) {
            uint32_t raw_command = multicore_fifo_pop_blocking();
            // DEBUG("Core1: Popped command: 0x%lX\n", raw_command);  // Log the popped command

            Core1Command command = static_cast<Core1Command>(raw_command);
            char char_to_send_back = '\0';
            bool send_char = false;

            switch (command) {
                case CORE1_CMD_SET_MODE_OFF:
                    core1_current_mode = Core1ActiveMode::MODE_OFF;
                    DEBUG("Core1: Mode set to OFF by command\n");
                    // Később: Core1 dekóderek resetelése/leállítása
                    break;

                case CORE1_CMD_SET_MODE_RTTY:
                    core1_current_mode = Core1ActiveMode::MODE_RTTY;
                    DEBUG("Core1: Mode set to RTTY by command\n");
                    // Később: Core1 RTTY dekóder inicializálása/resetelése
                    break;

                case CORE1_CMD_SET_MODE_CW:
                    core1_current_mode = Core1ActiveMode::MODE_CW;
                    DEBUG("Core1: Mode set to CW by command\n");
                    // Core1 CW dekóder resetelése
                    if (core1_cw_decoder)
                        core1_cw_decoder->resetDecoderState();
                    else
                        DEBUG("Core1: CW Decoder not initialized for reset!\n");
                    // Később: Core1 CW dekóder inicializálása/resetelése
                    break;

                case CORE1_CMD_PROCESS_AUDIO_RTTY:
                    if (core1_current_mode == Core1ActiveMode::MODE_RTTY) {
                        // DEBUG("Core1: Processing RTTY audio (simulated)\n"); // Ezt kikommentálva hagyjuk, hogy ne árassza el a logot
                        // KÉSŐBB: Itt hívnánk a Core1-en futó RTTY dekódert
                        // char_to_send_back = core1_rtty_decoder->decodeNextCharacter();
                        char_to_send_back = 'R';  // Szimulált válasz
                        send_char = true;
                    }
                    break;

                case CORE1_CMD_PROCESS_AUDIO_CW:
                    if (core1_current_mode == Core1ActiveMode::MODE_CW) {
                        if (core1_cw_decoder) {
                            char_to_send_back = core1_cw_decoder->decodeNextCharacter();
                        } else {
                            DEBUG("Core1: CW Decoder not initialized for processing!\n");
                            char_to_send_back = '?';  // Hiba karakter
                        }
                        send_char = true;
                    }
                    break;
                default:
                    DEBUG("Core1: Unknown command received: 0x%lX\n", raw_command);
                    break;
            }

            if (send_char) {
                // Válasz küldése Core0-nak (a dekódolt karakter)
                // Biztosítjuk, hogy a FIFO írható legyen, mielőtt küldenénk.
                // Ez egy egyszerűsített példa, valós alkalmazásban timeout-ot vagy nem-blokkoló írást használhatnánk.
                multicore_fifo_push_blocking(static_cast<uint32_t>(char_to_send_back));
            }
        } else {
            // Ha nincs parancs, a Core1 végezhet más feladatokat, vagy aludhat egy kicsit.
            // A `tight_loop_contents()` egy RP2040 specifikus no-op, ami jelzi, hogy ez egy szándékos üres ciklus.
            // Vagy használhatunk egy rövid `delayMicroseconds`-t vagy `yield()`-et, hogy ne pörgesse feleslegesen a CPU-t.
            // delayMicroseconds(100); // Példa
            tight_loop_contents();
        }
    }
}
