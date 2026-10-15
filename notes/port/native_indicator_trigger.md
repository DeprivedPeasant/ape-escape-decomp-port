# Native indicator proximity trigger

`native_indicator_trigger.c/.h` reconstructs Gimmick_ProxTrigger_Update (0x80091178) as a portable state adapter. The caller supplies current collider center/touchedBy, original VecLength3 result, area and stage-exit code. It returns ordered activation and effect0x112 notifications rather than invoking unimplemented engine globals.

The 24-byte trigger record is the original Gimmick layout. Only state+2 changes. State0 activates inside the signed radius and changes to1, or3 in areas5C..62. State1 additionally needs nonnegative touchedBy and param10==0. State3 resets when strictly outside radius; while inside or on the boundary it needs touchedBy. States2/other values stay inactive. Stage exit codes17/07/15 return before the original distance query. The pure state helper receives an already computed distance; the host should retain that early-query ordering.

The distance helper reproduces signed16 GTE IR truncation, wrapped unsigned sum of squares, and the independently verified original IntSqrt. It does not substitute floating Euclidean length. Effect position uses centerY-0xBE wrapped16. Notifications contain signed indicator index and hint sector.

Validation:4000 complete original VecLength3 calls;6000 complete original Gimmick_ProxTrigger_Update calls with controlled query/call endpoints, complete record bytes and580 ordered engine notifications agree. Generated results are in validation/indicator_trigger_validation.md. This is not a continuous gameplay capture.

Pending: runtime touchedBy updates and collider displacement, game-progress initialization, actual Indicator_Activate globals, hint player mode0xB, stage requests, pickup/effect execution and viewer wiring. The native player module remains untouched.
