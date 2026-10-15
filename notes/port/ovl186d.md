# Overlay 186d: MCX native boundary

Member 186 is loaded at `0x80136000`. This batch covers `0x80141EBC..0x801455AF` in
`extract/members/186.bin`. The task table and `notes/inventory/functions.csv` agree on
197 entries totaling 12,192 bytes. The readable C adapter is in
[`ae_ovl186d.c`](../../src/modes/ae_ovl186d.c) and
[`ae_ovl186d.h`](../../src/modes/ae_ovl186d.h); the differential boundary checker is
[`check_ovl186d.py`](../../tools/mips/check_ovl186d.py).

## Call tree and boundary

The instruction listing identifies this range as the MCX serial/accessory driver and
its private command and interrupt handlers. `MCXINIT_OBJ_32C` enters `_mcxStepSio`;
that state machine calls `_mcxInitSioMode` and `_mcxSioRW`, and dispatches its transmit
and receive callbacks through driver-owned function pointers. Public `Mcx*` calls
stage command-specific records and use the private `MCXCMD_OBJ_*` callbacks. The fast
DSR path installs or removes the handler and services SIO status/data through
PlayStation I/O registers. The two direct calls outside the member are `SysDeqIntRP`
(`0x8002F260`) and `FlushCache` (`0x8002F290`). The checker confirms 26 direct internal
call sites, no direct call into the earlier overlay ranges, and no direct internal
target missing from the 197-entry inventory.

These routines own the accessory protocol, SIO register work, DSR lifecycle, and
private transfer buffers. The native card backend therefore replaces this whole
driver at its public calls. The menu, save-file state, card-manager decisions, input,
timers, and text layout stay in the C ports from overlay parts 1–3. No code in this
batch directly calls those earlier ranges.

The native adapter dispatches by original address and forwards the original o32 words
without interpreting them. A missing backend, unknown address, or malformed argument
vector sets the `AeRam` error flag. The host owns protocol execution and any output
buffer writes. The 22 public hook entry points are:

| Address | Original function | Native operation |
|---|---|---|
| `8014236C` | `McxSync` | Poll/collect the result of a pending MCX operation |
| `8014275C` | `McxGetApl` | Get accessory application data |
| `801428E0` | `McxExecApl` | Execute an accessory application |
| `80142A90` | `McxGetTime` | Get device time |
| `80142BE4` | `McxGetMem` | Read a device-memory request |
| `80142E7C` | `McxSetMem` | Write a device-memory request |
| `8014311C` | `McxReadDev` | Read from the MCX device |
| `801434CC` | `McxWriteDev` | Write to the MCX device |
| `80143884` | `McxSetTime` | Set device time |
| `801439A4` | `McxGetUIFS` | Get UIFS data |
| `80143A64` | `McxSetUIFS` | Set UIFS data |
| `80143F54` | `McxShowTrans` | Show the MCX transfer indicator |
| `80144084` | `McxHideTrans` | Hide the MCX transfer indicator |
| `80144128` | `McxCurrCtrl` | Request the current controller value |
| `80144264` | `McxFlashAcs` | Run a flash-access operation |
| `80144344` | `McxGetSerial` | Read the device serial identifier |
| `80144400` | `McxSetLED` | Set the device LED state |
| `801444D4` | `McxCardType` | Query the attached card type |
| `80144DF4` | `McxGetInfo` | Query MCX information |
| `80144500` | `McxGetMcxInfo` | Query MCX device information |
| `8014464C` | `McxAllInfo` | Query the complete MCX information record |
| `80144850` | `McxExecFlag` | Run the MCX execution-flag operation |

`McxStartCom` and `McxStopCom` are before this batch at `0x80141B90` and `0x80141C00`;
they remain named native hooks in the earlier-range work. `_mcx*`, `MCXCMD_OBJ_*`, and
`MCXDSR_OBJ_*` entries are internal to the native driver and are not separate host
callbacks. The 4-byte `MCXDSR_OBJ_17C` entry at `0x801453FC` is a single NOP.

## Function status

All 22 public calls below are native hooks; the other 175 entries are private native
driver code and are subsumed by those hooks. There are no original C functions in this
batch, so there are no `tools/verified.csv` rows and no instruction-coverage exclusions.
The inventory has five overlapping spans in two areas; they are retained as listed
rather than treated as disjoint coverage ranges: `MCXCMD_OBJ_AB0` overlaps
`MCXCMD_OBJ_AE4`, `MCXCMD_OBJ_AEC`, `MCXCMD_OBJ_AF4`, and `MCXCMD_OBJ_B04`;
`MCXCMD_OBJ_1658` overlaps `MCXCMD_OBJ_16A8`. The per-entry status is listed here.

| Address | Original function | Status |
|---|---|---|
| 80141EBC | MCXINIT_OBJ_32C | native driver internals |
| 80141FE0 | MCXINIT_OBJ_450 | native driver internals |
| 80141FF0 | MCXINIT_OBJ_460 | native driver internals |
| 80142040 | _mcxInitSioMode | native driver internals |
| 80142104 | MCXMAIN_OBJ_C4 | native driver internals |
| 80142114 | _mcxSetRC2wait | native driver internals |
| 80142134 | _mcxChkRC2wait | native driver internals |
| 8014217C | MCXMAIN_OBJ_13C | native driver internals |
| 801421C8 | MCXMAIN_OBJ_188 | native driver internals |
| 801421D4 | _mcxSioRW | native driver internals |
| 8014226C | MCXMAIN_OBJ_22C | native driver internals |
| 80142354 | MCXMAIN_OBJ_314 | native driver internals |
| 8014235C | MCXMAIN_OBJ_31C | native driver internals |
| 8014236C | McxSync | native hook |
| 80142458 | MCXMAIN_OBJ_418 | native driver internals |
| 801424A4 | MCXMAIN_OBJ_464 | native driver internals |
| 801424B0 | MCXMAIN_OBJ_470 | native driver internals |
| 801424E0 | _mcxChkRsrv | native driver internals |
| 80142528 | MCXCMD_OBJ_48 | native driver internals |
| 80142530 | _mcxStepSio | native driver internals |
| 8014274C | MCXCMD_OBJ_26C | native driver internals |
| 8014275C | McxGetApl | native hook |
| 801427A4 | MCXCMD_OBJ_2C4 | native driver internals |
| 80142800 | MCXCMD_OBJ_320 | native driver internals |
| 80142808 | MCXCMD_OBJ_328 | native driver internals |
| 80142844 | MCXCMD_OBJ_364 | native driver internals |
| 8014284C | MCXCMD_OBJ_36C | native driver internals |
| 801428D4 | MCXCMD_OBJ_3F4 | native driver internals |
| 801428D8 | MCXCMD_OBJ_3F8 | native driver internals |
| 801428E0 | McxExecApl | native hook |
| 80142908 | MCXCMD_OBJ_428 | native driver internals |
| 80142950 | MCXCMD_OBJ_470 | native driver internals |
| 801429AC | MCXCMD_OBJ_4CC | native driver internals |
| 801429B4 | MCXCMD_OBJ_4D4 | native driver internals |
| 80142A14 | MCXCMD_OBJ_534 | native driver internals |
| 80142A60 | MCXCMD_OBJ_580 | native driver internals |
| 80142A68 | MCXCMD_OBJ_588 | native driver internals |
| 80142A90 | McxGetTime | native hook |
| 80142AD8 | MCXCMD_OBJ_5F8 | native driver internals |
| 80142B28 | MCXCMD_OBJ_648 | native driver internals |
| 80142B30 | MCXCMD_OBJ_650 | native driver internals |
| 80142B50 | MCXCMD_OBJ_670 | native driver internals |
| 80142BB4 | MCXCMD_OBJ_6D4 | native driver internals |
| 80142BD8 | MCXCMD_OBJ_6F8 | native driver internals |
| 80142BDC | MCXCMD_OBJ_6FC | native driver internals |
| 80142BE4 | McxGetMem | native hook |
| 80142C7C | MCXCMD_OBJ_79C | native driver internals |
| 80142CDC | MCXCMD_OBJ_7FC | native driver internals |
| 80142CF8 | MCXCMD_OBJ_818 | native driver internals |
| 80142D2C | MCXCMD_OBJ_84C | native driver internals |
| 80142D34 | MCXCMD_OBJ_854 | native driver internals |
| 80142D3C | MCXCMD_OBJ_85C | native driver internals |
| 80142D4C | MCXCMD_OBJ_86C | native driver internals |
| 80142D5C | MCXCMD_OBJ_87C | native driver internals |
| 80142D6C | MCXCMD_OBJ_88C | native driver internals |
| 80142D7C | MCXCMD_OBJ_89C | native driver internals |
| 80142D8C | MCXCMD_OBJ_8AC | native driver internals |
| 80142D90 | MCXCMD_OBJ_8B0 | native driver internals |
| 80142D98 | MCXCMD_OBJ_8B8 | native driver internals |
| 80142E6C | MCXCMD_OBJ_98C | native driver internals |
| 80142E7C | McxSetMem | native hook |
| 80142F14 | MCXCMD_OBJ_A34 | native driver internals |
| 80142F74 | MCXCMD_OBJ_A94 | native driver internals |
| 80142F90 | MCXCMD_OBJ_AB0 | native driver internals |
| 80142FC4 | MCXCMD_OBJ_AE4 | native driver internals |
| 80142FCC | MCXCMD_OBJ_AEC | native driver internals |
| 80142FD4 | MCXCMD_OBJ_AF4 | native driver internals |
| 80142FE4 | MCXCMD_OBJ_B04 | native driver internals |
| 80142FF4 | MCXCMD_OBJ_B14 | native driver internals |
| 80143004 | MCXCMD_OBJ_B24 | native driver internals |
| 80143014 | MCXCMD_OBJ_B34 | native driver internals |
| 80143048 | MCXCMD_OBJ_B68 | native driver internals |
| 8014304C | MCXCMD_OBJ_B6C | native driver internals |
| 80143054 | MCXCMD_OBJ_B74 | native driver internals |
| 8014310C | MCXCMD_OBJ_C2C | native driver internals |
| 8014311C | McxReadDev | native hook |
| 80143164 | MCXCMD_OBJ_C84 | native driver internals |
| 801431D4 | MCXCMD_OBJ_CF4 | native driver internals |
| 801431DC | MCXCMD_OBJ_CFC | native driver internals |
| 80143244 | MCXCMD_OBJ_D64 | native driver internals |
| 801432AC | MCXCMD_OBJ_DCC | native driver internals |
| 801432B4 | MCXCMD_OBJ_DD4 | native driver internals |
| 801433A8 | MCXCMD_OBJ_EC8 | native driver internals |
| 801434BC | MCXCMD_OBJ_FDC | native driver internals |
| 801434CC | McxWriteDev | native hook |
| 80143514 | MCXCMD_OBJ_1034 | native driver internals |
| 80143584 | MCXCMD_OBJ_10A4 | native driver internals |
| 8014358C | MCXCMD_OBJ_10AC | native driver internals |
| 801435F4 | MCXCMD_OBJ_1114 | native driver internals |
| 80143688 | MCXCMD_OBJ_11A8 | native driver internals |
| 80143690 | MCXCMD_OBJ_11B0 | native driver internals |
| 80143780 | MCXCMD_OBJ_12A0 | native driver internals |
| 80143874 | MCXCMD_OBJ_1394 | native driver internals |
| 80143884 | McxSetTime | native hook |
| 801438CC | MCXCMD_OBJ_13EC | native driver internals |
| 801439A4 | McxGetUIFS | native hook |
| 801439EC | MCXCMD_OBJ_150C | native driver internals |
| 80143A5C | MCXCMD_OBJ_157C | native driver internals |
| 80143A64 | McxSetUIFS | native hook |
| 80143AAC | MCXCMD_OBJ_15CC | native driver internals |
| 80143B30 | MCXCMD_OBJ_1650 | native driver internals |
| 80143B38 | MCXCMD_OBJ_1658 | native driver internals |
| 80143B88 | MCXCMD_OBJ_16A8 | native driver internals |
| 80143C58 | MCXCMD_OBJ_1778 | native driver internals |
| 80143C60 | MCXCMD_OBJ_1780 | native driver internals |
| 80143C7C | MCXCMD_OBJ_179C | native driver internals |
| 80143C84 | MCXCMD_OBJ_17A4 | native driver internals |
| 80143C98 | MCXCMD_OBJ_17B8 | native driver internals |
| 80143C9C | MCXCMD_OBJ_17BC | native driver internals |
| 80143CA4 | MCXCMD_OBJ_17C4 | native driver internals |
| 80143D04 | MCXCMD_OBJ_1824 | native driver internals |
| 80143DD4 | MCXCMD_OBJ_18F4 | native driver internals |
| 80143F44 | MCXCMD_OBJ_1A64 | native driver internals |
| 80143F54 | McxShowTrans | native hook |
| 80143F9C | MCXCMD_OBJ_1ABC | native driver internals |
| 80143FFC | MCXCMD_OBJ_1B1C | native driver internals |
| 80144004 | MCXCMD_OBJ_1B24 | native driver internals |
| 8014407C | MCXCMD_OBJ_1B9C | native driver internals |
| 80144084 | McxHideTrans | native hook |
| 801440CC | MCXCMD_OBJ_1BEC | native driver internals |
| 80144120 | MCXCMD_OBJ_1C40 | native driver internals |
| 80144128 | McxCurrCtrl | native hook |
| 80144170 | MCXCMD_OBJ_1C90 | native driver internals |
| 801441DC | MCXCMD_OBJ_1CFC | native driver internals |
| 801441E4 | MCXCMD_OBJ_1D04 | native driver internals |
| 8014425C | MCXCMD_OBJ_1D7C | native driver internals |
| 80144264 | McxFlashAcs | native hook |
| 801442AC | MCXCMD_OBJ_1DCC | native driver internals |
| 80144300 | MCXCMD_OBJ_1E20 | native driver internals |
| 80144308 | MCXCMD_OBJ_1E28 | native driver internals |
| 8014433C | MCXCMD_OBJ_1E5C | native driver internals |
| 80144344 | McxGetSerial | native hook |
| 8014438C | MCXCMD_OBJ_1EAC | native driver internals |
| 801443F8 | MCXCMD_OBJ_1F18 | native driver internals |
| 80144400 | McxSetLED | native hook |
| 80144448 | MCXCMD_OBJ_1F68 | native driver internals |
| 80144464 | MCXCMD_OBJ_1F84 | native driver internals |
| 801444D4 | McxCardType | native hook |
| 80144500 | McxGetMcxInfo | native hook |
| 80144548 | MCXCMD_OBJ_2068 | native driver internals |
| 801445A0 | MCXCMD_OBJ_20C0 | native driver internals |
| 801445A8 | MCXCMD_OBJ_20C8 | native driver internals |
| 801445CC | MCXCMD_OBJ_20EC | native driver internals |
| 8014463C | MCXCMD_OBJ_215C | native driver internals |
| 80144644 | MCXCMD_OBJ_2164 | native driver internals |
| 8014464C | McxAllInfo | native hook |
| 80144694 | MCXCMD_OBJ_21B4 | native driver internals |
| 801446E4 | MCXCMD_OBJ_2204 | native driver internals |
| 801446EC | MCXCMD_OBJ_220C | native driver internals |
| 80144710 | MCXCMD_OBJ_2230 | native driver internals |
| 80144744 | MCXCMD_OBJ_2264 | native driver internals |
| 80144758 | MCXCMD_OBJ_2278 | native driver internals |
| 80144768 | MCXCMD_OBJ_2288 | native driver internals |
| 80144778 | MCXCMD_OBJ_2298 | native driver internals |
| 80144788 | MCXCMD_OBJ_22A8 | native driver internals |
| 801447A8 | MCXCMD_OBJ_22C8 | native driver internals |
| 801447B8 | MCXCMD_OBJ_22D8 | native driver internals |
| 801447CC | MCXCMD_OBJ_22EC | native driver internals |
| 8014483C | MCXCMD_OBJ_235C | native driver internals |
| 80144844 | MCXCMD_OBJ_2364 | native driver internals |
| 80144848 | MCXCMD_OBJ_2368 | native driver internals |
| 80144850 | McxExecFlag | native hook |
| 801448B0 | MCXCMD_OBJ_23D0 | native driver internals |
| 80144910 | MCXCMD_OBJ_2430 | native driver internals |
| 80144918 | MCXCMD_OBJ_2438 | native driver internals |
| 80144A5C | MCXCMD_OBJ_257C | native driver internals |
| 80144AB0 | MCXCMD_OBJ_25D0 | native driver internals |
| 80144AB8 | MCXCMD_OBJ_25D8 | native driver internals |
| 80144AC0 | MCXCMD_OBJ_25E0 | native driver internals |
| 80144C88 | MCXCMD_OBJ_27A8 | native driver internals |
| 80144DA0 | MCXCMD_OBJ_28C0 | native driver internals |
| 80144DE4 | MCXCMD_OBJ_2904 | native driver internals |
| 80144DF4 | McxGetInfo | native hook |
| 80144E3C | MCXCMD_OBJ_295C | native driver internals |
| 80144E94 | MCXCMD_OBJ_29B4 | native driver internals |
| 80144E9C | MCXCMD_OBJ_29BC | native driver internals |
| 80144EC0 | MCXCMD_OBJ_29E0 | native driver internals |
| 80144EF4 | MCXCMD_OBJ_2A14 | native driver internals |
| 80144F08 | MCXCMD_OBJ_2A28 | native driver internals |
| 80144F20 | MCXCMD_OBJ_2A40 | native driver internals |
| 80144F30 | MCXCMD_OBJ_2A50 | native driver internals |
| 80144F40 | MCXCMD_OBJ_2A60 | native driver internals |
| 80144F4C | MCXCMD_OBJ_2A6C | native driver internals |
| 80144F54 | MCXCMD_OBJ_2A74 | native driver internals |
| 80144F60 | MCXCMD_OBJ_2A80 | native driver internals |
| 80144F74 | MCXCMD_OBJ_2A94 | native driver internals |
| 80144F78 | MCXCMD_OBJ_2A98 | native driver internals |
| 80144F80 | MCXCMD_OBJ_2AA0 | native driver internals |
| 80144FD8 | MCXCMD_OBJ_2AF8 | native driver internals |
| 80145280 | _mcxStartFastDsr | native driver internals |
| 80145374 | _mcxStopFastDsr | native driver internals |
| 801453FC | MCXDSR_OBJ_17C | native no-op |
| 80145400 | MCXDSR_OBJ_180 | native driver internals |
| 80145440 | _mcxFastDsrEnt | native driver internals |
| 801454CC | MCXDSR_OBJ_24C | native driver internals |
| 80145560 | MCXDSR_OBJ_2E0 | native driver internals |
| 80145578 | MCXDSR_OBJ_2F8 | native driver internals |

## Validation

The default `check_ovl186d.py` run loads the real EXE, member 186, and real area members
for areas 1, 2, and 5. It compares the C dispatch and MIPS oracle at the same intercepted
native entry point, including return value, forwarded name/address/arguments, RAM, and
scratchpad. The default run uses four boundary cases per public API (88 total); each MIPS
call has a 64-instruction budget and is intercepted at entry, so no private driver
instruction is executed in the checker. Original instruction coverage is not applicable
because the entire range is native-only.

Mutation mode caught 44/44 native API mapping mutations (two observable mapping changes
for each of the 22 public APIs). There are no per-original-function C mutations because
this batch contains no C-portable original functions. No RAM mask, stale-stack value,
or unreachable-code exclusion was added.
