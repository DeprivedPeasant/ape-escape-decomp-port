/* Isolate the historical two-callback gadget slot ABI from AeGadgetHooks. */
#include "../player/native_gadget_slots.h"
#include <string.h>

void ae_game_legacy_gadget_init(AeRam *ram, const void *hooks) {
    AeGadgetHooks local;
    memcpy(&local, hooks, sizeof(local));
    ae_GadgetSlotsInitForExit(ram, &local);
}
