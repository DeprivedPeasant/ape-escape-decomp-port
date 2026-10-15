/* Test-only access to portions of the unfinished enclosing PlayerUpdate port. */
#include "../../src/player/ae_playerupdate_internal.h"
void test_playerupdate_copy_input(AeRam *r) { playerupdate_copy_input(r); }
void test_playerupdate_select_tables(AeRam *r) { playerupdate_select_tables(r); }
void test_playerupdate_prepare_vertical_move(AeRam *r) { playerupdate_prepare_vertical_move(r); }
