#ifndef CORE_COMMUNICATION_H
#define CORE_COMMUNICATION_H

#include <stdint.h>

// Parancsok Core0-ról Core1-nek
// Egyszerű uint32_t értékeket használunk az első lépésben.
// Egy összetettebb structot később is bevezethetünk, ha szükséges.
enum Core1Command : uint32_t {
    CORE1_CMD_NONE = 0,
    CORE1_CMD_SET_MODE_OFF = 0x10,
    CORE1_CMD_SET_MODE_RTTY = 0x11,
    CORE1_CMD_SET_MODE_CW = 0x12,
    CORE1_CMD_GET_RTTY_CHAR = 0x21,
    CORE1_CMD_GET_CW_CHAR = 0x22
    // Később bővíthető pl. MUTE paranccsal, stb.
};

// Válaszok Core1-ről Core0-nak (egyelőre csak egy karakter)
// Ha nincs dekódolt karakter, '\0'-t küldünk.
// Ezt is uint32_t-ként küldjük át a FIFO-n.

#endif  // CORE_COMMUNICATION_H
