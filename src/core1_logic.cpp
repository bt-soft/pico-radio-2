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

/**
 * @brief Törli a Core1 dekódereit és erőforrásait.
 */
void deleteDecoders() {
    if (core1_cw_decoder) {
        delete core1_cw_decoder;
        core1_cw_decoder = nullptr;
    }
    // Később: Itt törölhetjük a RTTY dekódert is, ha szükséges
    // if (core1_rtty_decoder) {
    //     delete core1_rtty_decoder;
    //     core1_rtty_decoder = nullptr;
    // }
}

void core1_main() {
    multicore_fifo_drain();  // Drain any stale data in our RX FIFO at startup
    DEBUG("Core1: Main loop started.\n");

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
                    deleteDecoders();
                    core1_current_mode = Core1ActiveMode::MODE_OFF;

                    DEBUG("Core1: Mode set to OFF by command\n");
                    break;

                case CORE1_CMD_SET_MODE_RTTY:
                    deleteDecoders();
                    core1_current_mode = Core1ActiveMode::MODE_RTTY;
                    DEBUG("Core1: Mode set to RTTY by command\n");

                    // Később: Core1 RTTY dekóder inicializálása/resetelése
                    break;

                case CORE1_CMD_SET_MODE_CW:
                    deleteDecoders();
                    core1_current_mode = Core1ActiveMode::MODE_CW;
                    DEBUG("Core1: Mode set to CW by command\n");

                    // CW dekóder példányosítása a Core1-en
                    core1_cw_decoder = new CwDecoder(AUDIO_INPUT_PIN);
                    if (core1_cw_decoder) {
                        core1_cw_decoder->resetDecoderState();
                    } else {
                        DEBUG("Core1: FATAL - Failed to create CwDecoder instance!\n");
                    }
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
                    // Ezt a parancsot már nem használjuk a karakterenkénti dekódoláshoz,
                    // de itt maradhat pl. egy explicit dekódolási kéréshez.
                    if (core1_current_mode == Core1ActiveMode::MODE_CW) {
                        if (core1_cw_decoder) {
                            char_to_send_back = core1_cw_decoder->decodeNextCharacter();
                        } else {
                            DEBUG("Core1: CW Decoder not initialized for explicit processing!\n");
                        }

                        if (char_to_send_back != '\0') {
                            send_char = true;
                        }
                    }
                    break;

                default:
                    DEBUG("Core1: Unknown command received: 0x%lX\n", raw_command);
                    break;
            }

            // Ha van dekódolt karakter, akkor azt küldjük vissza a Core0-nak
            if (send_char) {
                if (multicore_fifo_wready()) {
                    DEBUG("Core1: CW Decoder processed character: '%c'\n", char_to_send_back);
                    multicore_fifo_push_blocking(static_cast<uint32_t>(char_to_send_back));
                } else {
                    DEBUG("Core1: FIFO full when sending char '%c'\n", char_to_send_back);
                }
            }

        } else {
            // A dekódolás csak a CORE1_CMD_PROCESS_AUDIO_CW parancsra történik.
            // Ha más háttérfeladatok lennének itt, azok maradhatnak.
            delayMicroseconds(10000);  // Pl. 10 ms
        }
    }
}
