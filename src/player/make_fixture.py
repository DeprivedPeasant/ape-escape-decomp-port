#!/usr/bin/env python3
"""Build a synthetic archive-member-0x008 image of the no-gadget state set from the committed
evidence package (extract/cloud_evidence/player/state_tables.json).

The result has the same byte layout the game's member has (header at +0x14 + i*0x10 (four u32 slots) with u16 word
offsets, 8-byte state entries, 4-byte transitions ending in 0xFF, 4-byte event records ending in
0xFF), so the C loader that reads the real member 0x008 locally is exercised by the cloud tests.
Only set 0 is written; the other nine headers are zero.

usage: make_fixture.py <state_tables.json> <out.bin>
"""
import json
import struct
import sys


def build(tables_path: str) -> bytes:
    sets = json.load(open(tables_path))
    s0 = next(s for s in sets if s["set"] == 0)
    states = bytearray()
    for st in sorted(s0["states"], key=lambda s: s["state"]):
        assert st["state"] * 8 == len(states), "state numbers must be dense"
        turn_tilt = (st["tilt_mode"] << 4) | st["turn_mode"]
        states += struct.pack("<HHBBBB", st["first_frame"], st["last_frame"], turn_tilt, st["move_mode"], st["frame_action"] or 0, st["flags"])
    trans = bytearray()
    for t in s0["transitions"]:
        cond = t["condition"]["index"]
        cond = -cond if t["negated"] else cond
        trans += struct.pack("<BbBB", t["from"], cond, t["to"], t["action"]["index"] if t["action"] else 0)
    trans += bytes([0xFF, 0, 0, 0])
    events = bytearray()
    for e in s0["events"]:
        assert e["kind"] in (1, 4, 8), "fixture only encodes sfx / call / voice events"
        events += struct.pack("<BBBB", e["state"], e["relative_frame"], e["kind"], e["arg"])
    events += bytes([0xFF, 0, 0, 0])
    header = 0xC0
    off_states = header
    off_trans = off_states + len(states)
    off_events = off_trans + len(trans)
    end = off_events + len(events)
    blob = bytearray(end)
    struct.pack_into("<HxxHxxHxxHxx", blob, 0x14, 0, off_states // 4, off_trans // 4, off_events // 4)   # four u32 slots, u16 word offsets in the low halves
    blob[off_states:off_states + len(states)] = states
    blob[off_trans:off_trans + len(trans)] = trans
    blob[off_events:off_events + len(events)] = events
    return bytes(blob)


if __name__ == "__main__":
    data = build(sys.argv[1])
    open(sys.argv[2], "wb").write(data)
