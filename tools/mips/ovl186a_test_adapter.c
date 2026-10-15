/* Expose one original thread-loop iteration to the differential harness. */
#include "../../src/modes/ae_ovl186a.c"

int32_t ovl186a_test_card_state_step(AeRam *ram,
                                     const AeOverlayHooks *hooks) {
    ae_ovl186a_card_state_thread_step(ram, hooks);
    return 0;
}

int32_t ovl186a_test_card_result_step(AeRam *ram,
                                      const AeOverlayHooks *hooks) {
    ae_ovl186a_card_result_thread_step(ram, hooks);
    return 0;
}
