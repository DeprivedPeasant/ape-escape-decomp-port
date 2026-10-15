# Integration module findings (2026-10-14)


## Pass 7/8 merge: bounded mode guards (2026-10-15)

The former mode-35 missing-entry guard became invalid when save-menu part 1
was merged. It entered the now-ported synchronous Mode_SaveMenu loop after
the last native-leaf guard, with raw tracing bypassing the normal log limit.
The seeded native-leaf words and SeqPlayer_SaveRestoreVram callback were not
the cause. The guard now checks the merged ledger before probing an absent
entry. The sequence-6 fixture stops at the member-186 entry through the existing
host continuation service; the oracle stops before the same first instruction.
It does not claim save-menu execution coverage. Its owning
module checker and central-wrapper suite retain that coverage.

The test shim caps each bounded native invocation at 20,000 callbacks and
reports an AeRam error before unwinding. Raw tracing also caps the log, and
Python callback failures request an immediate unwind. Public mode-word test
calls now use that bounded scope. Existing MIPS instruction limits remain.

The full central-wrapper sweep also found a stale boot-unit boundary:
check_ovl181 forced a StickToButtons callback while the merged host called
its verified port. It now executes the original dependency instructions when
that port exists, retaining the fallback hook for a tree without the port.
The enclosing Mode_03 comparison still checks all RAM, scratchpad and calls.

## Pass 7: Stage Start (member 188)

The integrated sequence-0 fixture executes the opening with real member-188
archive assets and compares every non-stack RAM byte, scratchpad, frame hashes,
and native calls. Its test-only renderer executes original packet-building
instructions from the C semantic requests; no renderer/gameplay RAM mask was
added. Scanline timer/seed writes from that renderer fixture are restored to
the C-produced values before comparison, keeping the gameplay check independent.

| Original evidence | Minimal correction / independent old-source rejection |
| --- | --- |
| `80136168` calls SDK ApplyMatrixLV at `80030450` with the view matrix and its translation as both input/output vector. This is byte-identical to the member-187 helper repaired in pass 6. | Call the existing exact `ae_sdk_apply_matrix_lv` helper, rather than delegating the result to a renderer callback. Both checkers now execute the original SDK instructions. Two whole-RAM LookAt cases reject the discarded translation; the following ScaleMatrixL callback retains its actual two-word ABI. |
| `8013685C..8013687C`, `80136A4C..80136A6C`, `801375E4..80137604` copy the eight-byte template at `8013ADB0` to each local RECT. | Initialize all three local rectangles. Six bounded instruction comparisons vary the template and compare the actual ClearImage bytes; each former source is rejected separately. |
| `8013689C` writes zero to `8013AE60`, including modes other than 8. | Initialize the wipe mode on entry. The rectangle probe starts with `12345678` and compares all non-stack RAM; omitting this store fails. |
| `8013827C..80138290` form mask `FFFF1FFF` using LUI/ORI and AND the mesh header. | Retain the upper halfword. The former `00001FFF` mask destroys real asset header bits and fails integrated RAM comparison. |
| `80137318..80137334` multiply the timer by 192, multiply by `66666667`, then shift the high word right once: signed division by 5. The reverse wipe repeats this at `80137448..80137464`. | Divide by 5 rather than 10. Native packet fixture comparisons reject the former half-speed horizontal coordinates. |
| `8013975C` / `80139FE8` read the display-buffer index at `800EC35C`; `8013A034` / `8013A0B8` read texture-page selection at UI `+2C`. | Rendering requests use the frame global rather than RAM `0000000C`, and the page field rather than animation-frame `+28`. Six full-RAM renderer cases vary buffer index and keep animation/page values distinct; both former requests fail independently. |
| XaPlay `80012000` onward writes only two filter bytes and one mode byte; CdMgrQueueCommand copies full parameter words. | Supply the untouched high bytes explicitly through `AeOvl188Hooks.xa_padding` / `AeGameHost.xa_caller_padding`, defaulting to zero in a native session. The oracle observer reads the actual caller bytes and resumes the entire original function. Zeroing the observed filter padding fails queue RAM comparison. |

The new `check_game_modes` shared-input row for XaPlay padding needs owner
review under the ledger rules. It shares only unwritten caller bytes; it does
not mask outputs, seed game state from the original, lower case minima, or
skip any original gameplay instruction.

## Pass 9: MissB ABI and XA archive boundary (2026-10-15)

AeMissbCallFn declares `(user, ram, name, address, args[4], argc)`.
The generator recognized only pointer spellings and treated `args[4]` as an
o32 scalar, producing invalid C. It now recognizes both array and pointer
spellings and emits a typed array-before-count adapter. The adapter reuses
the central member/address and named native API dispatcher. `Renderer*`
requests keep their names and complete arrays through render_words; ported
entries still execute their C adapters. No native pointer is cast to a word.

The first integrated area-05 divergence was frame 47, when the player's death
path reached XaPlay. Other callers of CdMgrQueueCommand already used the
native asynchronous archive boundary, but XaPlay called the queue C port
directly. Original calls are `jal 80012360` at 80011FF0, 8001202C, 80012060,
80012084, 80012098, 800120AC, 800120C0 and 800120D4. Its sector conversion is
`jal 80025D20` at 80012070. Running one queue implementation and intercepting
the other left different queue records and call sequences. This is host
interface closure, not a queue arithmetic change: XaPlay now accepts the same
native queue callback as the frame host, with the existing queue C port as
its standalone fallback. The eight commands, order and arguments are retained.
The original queue implementation still runs in its standalone differential
cases, and all 48 existing XaPlay cases still use it.

Eight additional check_player1 cases select the native queue path and compare
all commands, meaningful parameter bytes (filter, mode and CD position),
result/done pointers and all non-stack RAM against original instructions.
The body-movement unit host uses that same asynchronous queue boundary on
both sides and compares its meaningful payload bytes; its former oracle-only
queue execution caused two stale-host failures during the wrapper sweep.
The ctypes owners retain the full extended hook layout. The central area
checker adds 144 named MissB port/renderer/missing and XA comparisons; the
module regression independently reconstructs the former queue bypass and
requires rejection. Wiring mutations also remove the adapter, lose its count,
misroute renderer names and restore the queue bypass. The oracle budget is
2,000,000 instructions per call.

SfxVoiceSetPitch's old body callback and missing interception are removed;
its new port now receives those calls. SoundBankSetVolume and the existing
SfxVoiceSetPanVol likewise use their ported adapters instead of stale SDK
classification. No comparison mask, exclusion row or ledger minimum changes.

The full wrapper sweep also exposed older boot/save unit hosts that had no
MissB sound table or SpuWaitIdle service. Their existing native SPU boundary
is now supplied, and their former whole-function SoundStopAllVoices and
SeqTrackSetPlayState doubles are replaced by the new ports. Boot's stale
StickToButtons interception is removed too. SaveUi_SetState's score==1000
branch now receives actual save-image score bytes, derived with a bounded
original SaveScore call, instead of relying on its retired constant double.
All 518 save and 108 boot/intro cases pass with their original coverage rules.
Member 189's named unit exports also redirect to the central public adapters;
its private EXE-dependency bridge remains the owning unit's interface.

## Pass 8: PlayerInit boot input and aliased model outputs (2026-10-15)

The failing area-load fixture did not load player member 008 at 80103000.
PlayerInit (8003272C) is now reachable and requires that boot input. The
viewer also loaded it only after StageRequest; both now supply it before
stage entry. With the actual member, all four loads and all 360 frames
match, followed by the original frame-11 net catch and saved activity 2.

The empty header also exposed a genuine PlayerInit port bug. At 80032978
the auxiliary count is stored as a halfword; 800329B8 reloads it after each
StaticModelInit. Its output instances start at 800EC060 with stride 16.
For a zero header the initial count is 65535, and those outputs eventually
alias the stored count and stage model pointers. The old C cached the count
and pointers and continued overwriting unrelated RAM. Preserve the count
reload and the table reloads at 80032998, 800329DC, 80032A28 and 80032A90;
the auxiliary header uses the loop's actual final index (800329E0).

check_missa's last PlayerInit case now supplies an empty header. It compares
all non-stack RAM, scratchpad and calls with the original instructions under
the existing 2,000,000-instruction budget. check_game_module_regressions also
restores the cached count independently and requires a RAM mismatch. The
ordinary 31 real-member cases remain. No exclusions or ledger minima change.

The load call-log fixture now records PlayerAuxModelInit's single meaningful
argument: 800329F4 calls it with the header in a0; a1-a3 are caller leftovers.
Area 44's 90-frame original trace reaches state 2D after falling and landing
and starts state 80, without reaching 81/83 in this interval. Its assertion
now requires that observed path while every frame remains differentially
compared.

The full wrapper sweep exposed additional test-host closure defects. Small
overlay scripts 183/184/185 expose a shared member runner rather than main();
the wrapper runner now retains their function-table guard and invokes that
runner. Save-menu 186a's dispatcher indices are independent of boot's: its
index 0 means SaveUi_DrawText, not Mode_Boot. The proxy now derives its typed
dispatcher from the owning shim and selects it for that checker. The old
worker was terminated after identifying its DLL/PID in this worktree; its
incorrect boot dispatch is a failed run, never a passing comparison.

Save-menu 186b referenced absent ignored teststubs/testwrap files. Its tracked
ovl186b_test_api.c dispatcher now links the verified dependency closure and is
also used by the proxy to exercise public adapters. The two 186a worker
entries loop forever; the existing checker exposes one iteration using its
explicit adapter. Those two iteration-only interfaces are now explicitly
delegated in wrapper coverage rather than misreported as complete entry
proofs. Their existing module cases and ledger requirements remain unchanged.
Member 188's unit-only ovl188_call dispatcher likewise bypassed the public
adapters. The proxy now reuses that declaration-only dispatcher, retaining
its normal automatic export policy, and records all 28 wrapper entries across
192 invocations. The original unit comparisons still run unchanged.

The 186a rerun then exposed the production pad adapter's discarded input:
800133E8..800133F4 combines raw packet bytes 2/3 and NORs them into a0;
80013404 calls StickToButtons when PadUpdate flag 2 is set. The historical
AePadHooks callback has no argument, so generation had supplied zero. Its
adapter now reconstructs the same word from AE_PAD_BUF1, preserving all 32
bits, before entering the port. The unit-host stub uses that real route too.
Sixteen enclosing PadUpdate comparisons vary held buttons and stick axes;
a wiring mutation restores the discarded word and must fail those cases.

Enabled-demo profiles now provide neutral records and a valid buffer/frame
cursor. The newly ported playback correctly treats a zero record as an end
request, which made the old empty fixture enter a stage transition. FadeFrame
also makes FadeSpiralDraw reachable in transition profiles. Integration reuses
check_stagefade's registered 8007A02C renderer block and its cursor-store
epilogue, and both the native viewer notification and checker renderer retain
the original 32 * 24-byte cursor advance. No packet or game-RAM mask is added.

The corrected catch fixture no longer loads an archive member through the
stage's ArcGetMemberPos hook. Retain its existing wrong-address mutation and
exercise that exact hook directly for four real member-table entries instead:
poison its output record, run original 80012C0C, and compare the offset write,
CdIntToPos request and returned output pointer. The unchanged net-catch proof
still compares all 109 encounter frames and the final saved activity.

## Pass 5: member 18C integration defects (2026-10-15)

The real area-02 run now executes the 26 overlay ports and their ported EXE
dependencies. It exposed the following omissions in ae_ovl18c.c. The fixes
change only the corresponding expressions, stores and branch gates.

| Original instructions | Finding and fix | Strengthened check_ovl18c case |
| --- | --- | --- |
| 8013DF98..8013DFAC loads -2 and ANDs the flags | Clear bit 0, preserving bit 1; the old port cleared bit 1. | StageSpecificEnemyUpdate 8 starts with flags 3. |
| 8013E020 stores zero to object+200 before ground/shadow/cooldown | Clear render mode at the start of state 1, including early cooldown exits. | StageSpecificEnemyUpdate 10 starts with mode 44 and nonzero cooldown. |
| 8013E068 branches directly to 8013E1C4 when the near query succeeds | Return to the shared hit-update tail; do not enter reaction, set render mode 14 or reenable the body. | StageSpecificEnemyUpdate 9 supplies near=1. |
| 8013E05C loads a full word at object+40 | Forward the full signed-Y word to the near query, rather than truncating to s16. | The same case uses Y=-90000 and compares hook arguments. |
| 8013E100..8013E160 gates on Phase6Update only, then checks the exact first+20 frame and wake count | Remove the invented animation-finished gate in state 4. | StageSpecificEnemyUpdate 11/12 use unfinished animation and wake count 3. |
| 8013EA00 calls VectorNormalSS at 800301E0 before the quarter-step | Normalize the eye-to-target vector before adding its quarter-step to the surface origin. | SurfaceDraw 8 uses unequal, non-axis-aligned eye/target values. |
| 8013E4B0..8013E4C4 adds 16 to the texture byte at frame 34 | Apply the texture step at both frame 10 and frame 34. | EffectDraw 8 uses frame 34 without a Y clamp. |
| 8013E504..8013E514 adds and stores Y before testing the stop height | Always publish the moving Y, including non-clamping updates. | EffectDraw 9 uses frame 35 below the stop height. |

The standalone overlay checker now runs 216 cases (13 stage-specific enemy,
10 effect-draw and 9 surface-draw cases). Its builder uses the verified
dependency closure, repairing the obsolete link list that omitted bodymove.
check_game_module_regressions runs all eight focused cases and builds eight
independent restorations of the old behaviors. Each restoration must fail its
specified original-instruction comparison; build errors do not count.

Area-02 frame 24 additionally exposed ambient position padding. 8013D058,
8013D064 and 8013D094 write xyz at SP+24/+26/+28, while SP+30 is unwritten.
EffectSpawnFromDef's eight-byte copy transports that halfword into the effect
position. The old port forced it to zero. The minimal fix asks the existing
AeSpawnHooks.position_padding service for function 8013D000. Native sessions
default to zero; the integration oracle observes the original input before
the call and supplies it in call order. The observational patch executes the
original final xyz store and resumes at the original jal; all gameplay code
and all resulting effect bytes remain compared.

The module-regression suite also runs a passing 25-frame area-02 baseline and
requires the old forced-zero source to fail at 800BB002. Total: 49 positive
comparisons and nine old overlay sources rejected. The new check_game_area
shared_random row records this caller input and is pending owner review;
there is no new RAM mask, unreachable range, lower minimum or status change.

The complete wrapper run found a separate checker-interface issue: the new
shared zako hook signature tried to apply Atan2Safe's filler-register mask in
check_zakoai/check_zakohelpers, where that mask was never registered. Their
event collectors now record the API's two scalar arguments. Both actual
arguments are compared; no additional mask or exclusion was added.

## Pass 4: shared water height in PlayerSplashStart

Area 05 frame 62 originally wrote FA00 to 800B4D90 while C retained zero.
Instruction tracing identified the writer as 8006B8B8 in PlayerSplashStart,
not PlayerWaterEffectsUpdate. At 8006B89C..8006B8B0 the original reads the
surface-top and extent halfwords at scratchpad 1F8003DE/3E4, subtracts them,
and stores the height to splash-state +4. The jal memset delay slot at
8006B8B8 stores the same halfword to the shared particle-water height.
The C port omitted this second store. Publishing the already computed height
to the named AE_P2_G_WATER_PARTICLE_HEIGHT field is the complete fix.

check_player2 now seeds scratch surface values and a distinct old water
height instead of allowing zero to hide the omission. The integrated
check_game_module_regressions also calls the actual port and original entry
with this fixture. Restoring the complete old ae_player2_leaf.c source from
HEAD fails at 800B4D90/91. A permanent mutation removes that exact store.
The final player2 checker passes all 807 cases; the proxy tests the same cases.

## Pass 4: slow-entry height in PlayerWaterEffectsUpdate

Inspecting the related original water-entry routine exposed a second defect.
8006BAB8 tests absolute vertical speed against 17; 8006BABC branches around
splash creation when speed is lower. Its delay slot at 8006BAC0 writes the
surface height to 800B4D90 regardless of speed. The port incorrectly placed
that store inside the speed >=17 arm. Moving it immediately before the test
preserves the original result without adding any extra gameplay behavior.

The effects fixture now seeds a nonzero surface input and distinct old height;
its slow-entry cases compare the published value. A new permanent mutation
restores the old speed-gated store, and the checker catches all 44 mutations.
The integrated slow-entry regression independently rejects the complete old
ae_effects_water.c source from HEAD. Both old-source failures are recorded in
extract/codex-task/old-water-proof.log; final production regressions pass 13/13.
No masks, shared-input exclusions or ledger minimum reductions were added.

## Pass 4: water-entry depth and wake height are distinct

8006BAC4 reads the signed full word at player +12C before subtracting player
Y and testing the splash depth interval 49..143. Later wake tests read the
separate signed halfword at player +13C. The port reused its +13C field for
the full-word entry-depth query. A distinct WATER_PLAYER_GROUND_HEIGHT (+12C)
constant fixes that one read; the later +13C wake queries remain unchanged.

The strengthened effects fixture makes +12C=1096 and +13C=32767 for a player
at Y=1000. At vertical speed 20 the original emits the splash group, while
the old field choice skips it. A permanent mutation restores that field and
is rejected. The integrated regression compares the real spawn, RNG and RAM
effects; the old choice differs in 613 bytes. Its independent proof, also in
old-water-proof.log, isolates the wrong field while retaining both fixed
height stores. No field alias or zero-input fixture hides this error now.

## Pass 4: renderer boundary and SDK evidence

The axis routines at 800306F0/80030890/80030A30 mix two matrix rows in place
using the packed sine/cosine table at 800AA5C0. Original low products are
added/subtracted before the shift by 12; stores truncate rather than saturate.
The three new C maths ports preserve translation and the untouched row.
check_pose compares 160 cases per axis, including INT_MIN/MAX angles and
arbitrary matrix lanes; all 102 original instructions per routine execute.
96 additional game-hook comparisons prove the actual pause-table routing;
three executable wiring mutations replace each maths route and are caught.

Area-05 frames also newly reach inline renderer work at 80063958 (wobble GTE)
and 8006FBC4 (effect-object packet emission). The integration checker now uses
the same named boundaries and continuation addresses as the existing unit
checker. The effect-object boundary preserves the OT cursor in t1 for the
post-render store at 8006FE1C. All scratchpad bytes still compare; no renderer
scratch masking or copying was introduced. Original gameplay entries remain
unintercepted, including PauseMenuUpdate and all three axis routines.

Task2 changes the owner-authorized collision climb hook and copied misc hook type. Camera gameplay code remains unchanged. The earlier diagnostic
copies of ae_stage.c, ae_fxscript.c, ae_player_act_state.c and ae_camupd.c have
been removed, and `--diagnose-module-fixes` has been removed from the checker.
All continuation checks compile the actual module sources.

## Owner fixes verified

Commit 1a24273 merged the owner's corrections to AreaLoad fog colour (+1C),
camera sine/cosine calls, EffectScriptUpdate's player-coordinate stride,
PlayerSetEyeFrame's command address, and CameraRegionTest's Y/Z bounds.
Area loads 01, 02, 05 and 44 now match. Areas 01 and 02 complete eight matching
frames without diagnostic sources. Boss area 44 also completes eight frames
once its unported overlay update at 80140D60 is intercepted on both sides.

The previously reported original sites were: AreaLoad 80031B60;
EffectScriptUpdate 80069BEC, coordinate loads 80069C28/80069C38/80069C3C;
PlayerSetEyeFrame 80067C08, command arithmetic 80067C4C..80067C70;
CameraRegionTest 8004BD98, comparisons 8004BEBC..8004BEFC. These findings are
resolved by the owner and require no further module changes from this task.

## Resolved: radial climb callback dropped two arguments

`ae_collprobe.h` now declares a dedicated six-word climb callback;
`radial_round_response` forwards signed collider +0/+2/+4/+6/+8 and
type 59 ? 1 : 0. `ae_misc.h` uses the same type for its copied probe hook.
CollisionRayTest has no separate direct climb caller: its collider dispatch
table at 800B2108 category 3 reaches CollProbe_Radial (80074BBC), which calls
PlayerSetClimbTarget (80066E28). No other misc hook changed.

Original 80074F60 loads the signed body type into $t0; 80074F68 branches away
unless it equals 1. The store of $t0 at 80074F98 therefore proves the sixth
argument for type 59 is 1. Type 89 stores zero at 80074FB0. The fifth signed
halfword is loaded at 80074F90/80074FAC and stored at sp+10 in the jal delay
slot (80074FB8). `ae_game_call6` supplies both stack words to the existing
central wrapper and restores them after returning; both game hook adapters
and the copied misc probe adapter preserve all six words.

Both differential checkers read sp+10/sp+14 after the original jal delay slot,
record six C parameters, and compare all six. Explicit type-59/type-89 cases
use half-Y 173/219 and assert that the original climb call occurred. Misc
collider cases include dispatch category bits (303B/3059). Each checker catches
fifth-word, sixth-word and both-word loss. The collprobe suite catches 43/43
mutations; misc catches 70/70 existing mutants plus all three climb mutants.
The integrated module reproducer now passes both cases with the actual
PlayerSetClimbTarget executing on both sides, with no exclusion.

`extract/mips/task2_old_source_proof.log` also records failures using unchanged
HEAD sources and headers: the old collision module fails the strengthened
checker, and the old misc plus collision modules fail the strengthened misc
checker. Old four-word calls expose undefined fifth/sixth host arguments;
the controlled both-word-drop mutations independently prove detection without
depending on those undefined values. Diagnostic old sources remain ignored.

## Resolved by declared exclusion: camera focus padding

CameraSlotGetFocusPoint writes three coordinate halfwords into its local.
CameraSlotUpdate copies all eight bytes at 8004CE54..8004CE70, including the
uninitialised fourth halfword, to slot +12..+19. Area 05 previously differed
only at slot +18/+19 (original caller stack residue 800E versus native zero).

The owner chose declaration rather than reproducing residue. The new
`check_game_area` ram_mask row is exactly
`camera slot target padding (slot +0x18:2)`. Both integrated frame checkers read
the current table pointer at 800B4D40 each comparison, inspect twelve 0xB0-byte
slots and mask exactly two bytes for each live (nonzero kind) slot. Only
comparison copies change; actual states remain independent. No camera module
change, padding callback, shared-random declaration or other new mask exists.

Reader audit: searches of AE_CAM_TARGET and target +12/+14/+16 in the camera
and camera-update modules were checked against original listings:

- CameraSlotSmoothTarget reads +12/+14/+16 at
  80047558/800475AC/800475FC; no +18 data read.
- CameraNudge reads +12/+14/+16 at 800476EC/800476F8/80047704.
- CameraApplySlot copies eight target bytes into its local at
  8004814C..80048168, then consumes only local +8/+A/+C at
  80048220/80048254/80048288. Local +E (slot +18) is never consumed.
- CameraLookAt consumes target +0/+2/+4 at
  800466CC/8004671C/800466E0. Other vector copies transport padding;
  target arithmetic uses the three coordinate halfwords.

The exclusion reason records this evidence. Audit listings are ignored files
under `extract/mips/task2_*listing.txt`. Areas 01, 02, 05 and 44 now match their
loads and all eight consecutive frames: 36 comparisons, zero failures.

## Resolved: Indicator_Register allocated-index return (2026-10-15)

The merged misc batch declared 800716EC void, but Gimmick_ProxTrigger_Init
uses its result to identify the registered indicator. After wiring the port,
area 01 load differed at later gimmick records +0x14: the generated void
adapter supplied zero for every registration.

Original evidence: 80071788 (`addu $v0, $s1, $zero`) returns the old count;
80071794 increments it separately into v1 and 80071798 stores the new count.
The caller's 80091114 `jal 800716EC` is followed by 8009111C
`sh $v0, 20($s1)`, storing the allocated index at gimmick +0x14.

Minimal fix: change the declaration/definition to int32_t and return count
following the existing increment. The misc checker now compares the return
as signed, including existing counts 0, 3 and the returning exit model at 4.
All 297 default cases pass. The strengthened checker rejects the unchanged
HEAD module/header on its count-3 scenario: C=03D3AC90, original=00000003.
The controlled old-adapter zero-result behavior also fails (C=00000000 versus
original=00000003). The old void function leaves an unspecified native result; the integrated
regression independently requires explicit indices 0, 1 and 3, avoiding any
reliance on that unspecified value. All five module regressions pass.

Reproducible unchanged-source proof and original listings:
extract/codex-task/prove_old_indicator.py and old_indicator_proof.log.
No ledger minimum, exclusion, gameplay approximation or new port was added.

## Pass 2: copied ray-test user contract (2026-10-15)

The bodymove merge already changed CollisionRayTest's copied probe table to use `h->user`. The older game adapter still treated that value as an AeMiscHooks pointer and unwrapped it again, causing an access violation. The chosen contract is uniform: copied callbacks receive the original callback user, unchanged. ae_game_collision_ray now calls ae_CollisionRayTest with g->misc directly; its callbacks receive AeGame, or the checker-supplied user when testing a wrapper. All seven probe-forward adapters were removed. Camera's separate nested-owner contract is unchanged.

The misc checker now supplies a non-null sentinel user to all callbacks and asserts that every direct and copied callback receives it, including the six-word climb cases. The module documents the contract at its copy site. No ray gameplay logic changed.

## Pass 2: CollProbe_Box vertical return blind spot

The bodymove merge already fixed the port, but the checker never reached the rejects. Original 80073F44 branches when the player's bottom is at/above the collider top; its delay slot 80073F48 sets v0=1. The lower reject at 80073F5C similarly sets v0=1 at 80073F60. Lateral response has already been written, so these are contacts even when vertical overlap is rejected.

The checker now runs explicit upper/lower cases (y=242,height=241 and y=-129,height=240), forces acceptance of the earlier mocked range test through odd XOR inputs, asserts execution of each delay slot, and compares the return. The existing 100-case Box sample becomes 102 cases. A return-0 source copy fails both cases with original=1/C=0. The added return mutation is caught; all 44 collision-probe mutations are caught. Both cases also pass through the real central wrapper in check_game_module_regressions.

## Pass 2: MessageBoxDrawWindow renderer ordering entry

The active-message integration fixture exposed an incorrect request field hidden by check_hud's broad renderer packet exclusions. The original loads GPU depth/cursor at 8008E364/8008E368, adds 0x80 to the GPU base at 8008E36C, computes depth*0x4078 at 8008E370..8008E384, and links every frame packet through that bucket. The last 40-byte packet is linked at 8008E568..8008E590. The port used hud_draw_current, which assigns the ordinary sprite bucket GPU+0x4080 instead.

The minimal fix supplies GPU+0x80+depth*0x4078 explicitly while retaining the same cursor update and typed composite draw request. check_hud now checks the request entry and the original final packet link. A copy restoring the old helper call fails 36 of the 580 HUD cases. The integrated regression covers buffer 1 separately. No GPU implementation was added to production.

Old-behavior proof: extract/codex-task/prove_old_wiring2.py and old_proof.log. These are temporary copies of current source with only the old return/helper behavior restored; they do not rely on unspecified host arguments.

## Pass 2: integration comparison boundaries and fixture corrections

Two narrow evidence-backed exclusion rows were registered under this task's instruction allowing new masks only with registered evidence. HUD packet spans/ordering tags belong to the renderer: only bytes named by emitted AeHudDraw requests are excluded. Standard 24-byte sprite fields are decoded from the original packets and compared; all cursor and gameplay RAM changes remain compared. Renderer bytes are shared after comparison solely to prevent old packet contents appearing as a new frame difference. BeginFrame initializes a dedicated arena at 801E8000 on both sides; a zero cursor let message GPU commands alias unrelated sound data. Ported HUD functions still execute their original instructions on the oracle side.

The other row covers only scratchpad 1F8003FC:2 when the current area header disables the heightfield. The existing 1F8003FE:2 evidence already identifies both never-written slope halfwords copied by GetGroundHeight's no-heightfield fallback. Real terrain slopes continue to be compared. Existing particle-padding exclusions are applied to every integration comparison (including boundary scenarios). SpawnEffectGroup's explicit padding callback receives the observed original local at sp+1E, using the existing check_spawn shared-random contract; effect-object padding is therefore compared rather than masked. No camera or effect-object mask was added.

The retired body_move callback was removed from monkey_harness's callback setup, and check_bodies' ctypes layout now includes the embedded AeBodyMoveHooks, preventing truncated-table copies by the wrapper proxy. Misc standalone builds link the real verified closure; central wrapper checker builds resolve newly merged dependencies against the proxy DLL. These are checker ABI/build repairs, not gameplay ports.

Scratchpad evidence listing: extract/codex-task/ground_height_evidence.txt.
80043468/8004346C checks header flag 2 and skips to 80043858 without writing
sp+10/sp+12. 80043894/80043898 loads those halfwords; 8004389C/800438A8
copies them to scratchpad +3FC/+3FE after the terrain<solid comparison. The
new +3FC exclusion is conditional on that same header flag, so valid
heightfield slope output remains checked.

The old owner-table user behavior was also restored in a temporary combined
misc source. The strengthened checker rejects it on CollisionRayTest's copied
HalfPipeRangeTest and PointInBoxXZ callbacks with `callback user context lost`.
The same proof script records this independently of the game-host crash.

## Pass 3: null player-model bootstrap (fixture/host integration)

The fault at FFFFF400 was at 800644A8 in PlayerRunAnimEvents, which reads its
event pointer through s3. The pointer at g_PlayerStateSets+0xC was initially
valid (8012A9D0) but became FFFFF400 during animation. With player+0 and
player+0xC both zero, the original bone builder entered its do-while loop
with an invalid zero bone count and wrote beyond the matrix pool into the
state-set table. Observed writes include 8002FDBC, 8001564C/58/64 and
80030098/9C/A0. These are real original instructions, not a bad interpreter
mapping or a bodymove stack-pointer defect.

PlayerInit reads member-008's model word offset at 80032770..80032784 and
initializes the actor at 80032840..80032854. The area fixture and native viewer
now use that real header with the existing ModelInitAuto port. The fixture
also runs original ModelInitAuto and compares the resulting actor before
frames. No PlayerInit implementation was added. Original PlayerUpdate and
PlayerAnimateAndDraw execute on the oracle with no entry interception.

## Pass 3: ledge direction padding (explicit caller input)

PlayerTryLedgeGrab writes xyz at SP+0x10/+0x12/+0x14 (80063F64..80063FB8),
normalizes those three lanes, and later copies eight bytes at
800641D0..800641EC. SP+0x16 is never initialized. At area-05 frame 43 the
original copied 8007 into player+0x66, while native scratch storage held zero.
The fourth lane is caller residue, not a fourth direction coordinate.

AeBodyMoveHooks.LocalDirectionPadding now supplies that input explicitly;
native game hosts default to zero. The integration checker runs the original
first, confirms execution of the copy through instruction coverage, and
supplies the observed halfword before running C. All player RAM bytes,
including +0x66, remain compared. The standalone bodymove checker poisons its
native local halfword with original XOR A55A, so ignoring the service fails.
Restoring the old module source fails the successful ledge case at
800EC246/247. The integrated module regression includes that case.

Two new shared_random registry rows, for check_game_area and check_bodymove,
document this same input and remain for the owner's review. No RAM mask,
coverage exclusion, ledger minimum or function status was weakened.

## Pass 3: RibbonDraw first projected point

8006F468 initializes the previous packed screen position to zero.
8006F498..8006F4B4 compares every projection against it, including point zero;
a duplicate decrements the effective point count and may return immediately.
The port incorrectly guarded the comparison with i != 0. Removing that guard
is the complete production fix.

The strengthened check_fxscript case first-projection-origin uses input
(-896,480,100), which its reviewed projection boundary maps to packed SXY=0.
For count=2, original code returns after one projection. Old source projects
a second point and emits a ribbon, changing the packet cursor by 40 bytes
and changing scratchpad contents. The new case rejects that source, and the
central module regression independently tests the zero projection boundary.
RibbonDraw now has 12 default cases; all existing coverage requirements remain.

The fxscript standalone/mutation builders now use check_game's content-cached
verified dependency closure. Their old source list omitted the bodymove
functions called by merged CollisionUpdate. No link-only gameplay stubs were
introduced. Old-behavior proof scripts/logs are ignored under
extract/codex-task/prove_old.py and old_proof_final2.log.


## Wiring pass 6: stage-clear view translation

Member 187 `StageClearUi_LookAt` called `80030450` through a one-word
`SetRotMatrix` hook. The original `80136118..80136120` supplies the view
matrix in a0 and its translation at +20 in both a1 and a2. The call at
`80136168` is ApplyMatrixLV: `80030478..80030480` reads the input vector,
and `8003059C..800305A4` writes the transformed vector. A renderer
notification discarded that in-place translation.

The port now calls the existing exact `ae_sdk_apply_matrix_lv` arithmetic
helper on those three pointers. `801361A4` is also named correctly as
ScaleMatrixL (two words), retaining its existing native SDK boundary.
The strengthened check_ovl187 no longer intercepts ApplyMatrixLV: its
original instructions execute against the GTE oracle, and the complete
view matrix remains compared. The standalone build links the existing
native_sdk_apply.c source. check_game_module_regressions exercises eight
LookAt fixtures and reconstructs the former one-word call; that old source
must fail on RAM, independently of hook-label differences. No ledger
minimum or exclusion is changed.

The mode headers also exposed an include cycle: ae_overlay_hooks.h included
all of AeGame while AeGame's generated fields needed AeOverlayHooks.
The public call declarations now live in ae_calls_api.h; ae_calls.h keeps
its existing full-host include contract. The overlay header includes only
the API declarations. This changes no gameplay behavior.


The merged 187 checker also referenced two absent temporary files under
extract/codex-task. Its dispatcher and the existing named EXE dependency
models are now tracked as tools/mips/ovl187_test_api.c and
ovl187_test_stubs.c. These files are test-only; game/viewer builds link the
real dependency ports. The central ABI proxy redirects o187_call to its
public adapters and keeps those same unit dependency models in its private
187 copy, so all 23 original entries receive actual adapter counts. The
proxy proof identity includes both support files.


The 181/182 unit host predated the resident-mode merge and left AeModeHooks
empty. Mode_03's original StageInitMemory instructions completed, while C
failed at its ClearWords boundary. ovl_game_stub.c now binds that existing
mode table to ae_game_mode_call; its normal dependencies still run as ports.
This is test-host closure, with no new game behavior or comparison mask.


The existing CollProbe_Box vertical-return regression still attempted to
force HalfPipeRangeTest=1 after that helper became ported. Both former
inputs actually returned 0 from the original broad phase; C matched them,
but the fixture asserted 1. It now uses (y,height)=(32,16) and (95,94).
The original 80050490 subtracts 96, placing the sample at -64/-1 inside
(-128,0); the later Box y-height test is 16/1 >= shape top 0 and returns 1.
Both the helper and Box execute original instructions. The stale double is
removed, both cases remain, and no mask, minimum or exclusion is weakened.


## Wiring pass 6: boot/minigame clear-image payloads

Comparing the eight RECT bytes consumed by ClearImage/ClearImage2 exposed two
member-181 errors hidden by the existing stack-window comparison:

- Mode_Boot writes height 512 at 8013613C/80136150 and retains that caller-local
  rectangle until ClearImage2 at 801361CC. The C logo helpers reused their
  common temporary rectangle and the caller incorrectly rebuilt height 256.
  The final clear now reconstructs the original 640x512 rectangle.
- Mode_02 loads eight bytes from 80136918 at 801368AC..801368CC and passes
  them to ClearImage at 801368D0. The real template is (0,0,384,512), while C
  invented (0,0,640,256). The fix copies both original template words, through
  the named AE_MODE02_CLEAR_TEMPLATE address, rather than hardcoding its size.

check_ovl181 now records both RECT payloads in native call comparisons.
Mode_02 keeps its four cases: one real template and three edited templates,
so replacing the copy with even the corrected fixed dimensions fails.
check_game_modes compares these payloads during original main -> boot ->
title too. Its --mutate run independently rejects the former final boot
height and reconstructs/rejects the former Mode_02 rectangle through the
original-instruction boot unit host. The lost-member mutation remains caught.
No stack mask is widened, no case minimum changes, and no new exclusion is used.
