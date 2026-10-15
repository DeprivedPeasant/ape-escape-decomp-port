# Native camera mode selection

`native_camera_select.c/.h` reconstructs local control state and ordered requests
from CameraSelectMode (0x80046DE8). It reads original 0xB0-byte slots and changes
the exact 80-byte control window at 0x800E68D0. No original code bytes are embedded.

Priority from lowest to highest: follow slot (+18), region slot (+1B, kind4 or
kind5 when slot+8E==1), force-follow (+42), fixed (+38==1), aim
(+32==1 or +46==1, unless IsFlightArea), forced region (+41). If the chosen slot
equals the previous slot (+0E), nothing changes, including the existing kind.

Changing from/to aim yields blend style2. Fixed camera changes yield style3.
Kind5 with fixedActive (+33) also uses style3; otherwise the original normalized
XZ direction from the previous eye to the player's position must have a
nonnegative signed wrapped dot product with the new slot's scriptFocus X/Z
(+94/+98). A negative result preserves the previous selection.

Returning from kind4/5 to follow requests CameraSlotSnapshotEye first and uses
blend style1. Other changes use style0. Accepted changes store style at control
+2E, delay128 at +30 (zero for forced region kind4), both current padded eye/target
SVECTORs at +1C/+24, then chosen slot/kind at +0E/+0F, and request
CameraActivateSlot. Both service calls are returned rather than executed. In
particular, this function does not claim to initialize the follow slot during
the snapshot request or replace the original active-slot pointer globally.

Inputs include a controlled flight result and original runtime math tables for
the direction gate. Invalid pointers/indices or a normalization overflow/missing
table return0 with controls and requests unchanged. Valid no-change calls return1
with requests cleared. Slots are always read-only.

Generated checks: validation/camera_select_validation.md. Complete original instructions,
including VectorNormalSS, are compared against all control bytes and ordered
requests. Flight/snapshot/activation dependencies are intercepted explicitly.
Region lifetime, script execution, blend rendering and native viewer integration
remain separate. The service functions below are now implemented and validated.

## Snapshot and activation services

`ae_camera_snapshot_eye` implements complete CameraSlotSnapshotEye (0x80049514)
for the caller's follow slot. It sets fixedActive1, modeTimer0, stillFrames0,
dist/followDist0x460, copies all eight current eye bytes including padding, and
computes yaw from the wrapped signed16 XZ player-minus-eye delta through original
VectorNormalSS and Atan2Safe(-dx,-dz). It does not copy the target despite the
older function comment in the evidence saying eye/target. Transactional math
failure leaves all slot bytes unchanged.

`ae_camera_activate_slot` implements CameraActivateSlot (0x80046DAC) control
fields +04 index (halfword), +06 slot kind (unsigned byte widened to halfword),
and +08 address tag (base+index*0xB0). That tag is an opaque source address for
state comparisons; native hosts must resolve the slot by index and must never
dereference it. Null arguments, invalid indices and overflowing address tags
reject without writes.

The checker now also executes a second12,000-case pass with actual original
snapshot/activation services and SDK math. Portable selection followed by these
two services matches every control and all12slot bytes. Four additional service
failure checks preserve state. The first pass still independently checks the
ordered service requests with those calls intercepted.
