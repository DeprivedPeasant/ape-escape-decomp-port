#!/bin/sh
# Build and run the portable player tests (no game data needed). Optional: CC=clang, CODEX_MATH=1 to
# cross-check the math routines against src/collision/native_collision.c, SANITIZE=0 to build without
# -fsanitize (e.g. MinGW, where ASan/UBSan are not available).
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
python3 -I "$here/make_fixture.py" "$root/extract/cloud_evidence/player/state_tables.json" "$tmp/member.bin"
extra=""
san="-fsanitize=address,undefined"
[ "${SANITIZE:-1}" = 0 ] && san=""
if [ "${CODEX_MATH:-1}" = 1 ]; then
    extra="-DWITH_CODEX_MATH $root/src/collision/native_collision.c $root/src/collision/native_ground.c"
fi
${CC:-gcc} -std=c99 -Wall -Wextra -O1 -g $san $extra \
    "$here/test_player.c" "$here/native_player.c" -lm -o "$tmp/test_player" 2>"$tmp/build.log" || { cat "$tmp/build.log"; exit 1; }
grep -E "native_player\.c|test_player\.c" "$tmp/build.log" || true
"$tmp/test_player" "$tmp/member.bin"
# Player collision-body pass (native_player_body.c): synthetic worlds, same flags.
${CC:-gcc} -std=c99 -Wall -Wextra -O1 -g $san \
    "$here/test_body.c" "$here/native_player_body.c" "$here/native_player.c" -lm -o "$tmp/test_body" 2>"$tmp/build_body.log" || { cat "$tmp/build_body.log"; exit 1; }
grep -E "native_player_body\.c|test_body\.c" "$tmp/build_body.log" || true
"$tmp/test_body" "$tmp/member.bin"
# Volume queries (native_player_volume.c): BodyRecordColliderContact scan + CollisionRayTest on synthetic
# collider records, plus update+body frames wired through them.  Same flags; SquareRoot0 cross-check with CODEX_MATH.
${CC:-gcc} -std=c99 -Wall -Wextra -O1 -g $san $extra \
    "$here/test_volume.c" "$here/native_player_volume.c" "$here/native_player_body.c" "$here/native_player.c" -lm \
    -o "$tmp/test_volume" 2>"$tmp/build_volume.log" || { cat "$tmp/build_volume.log"; exit 1; }
grep -E "native_player_volume\.c|test_volume\.c" "$tmp/build_volume.log" || true
"$tmp/test_volume" "$tmp/member.bin"
# Gadget sets on the real data (Monkey Net swing); skipped when extract/ has no member 008 / EXE.
${CC:-gcc} -std=c99 -Wall -Wextra -O1 -g $san \
    "$here/test_gadget.c" "$here/native_player.c" "$here/native_player_pad.c" "$root/src/core/ae_pad.c" "$root/src/core/ae_ram.c" -lm -o "$tmp/test_gadget" 2>"$tmp/build_gadget.log" || { cat "$tmp/build_gadget.log"; exit 1; }
"$tmp/test_gadget" "$root/extract/members/008.bin" "$root/extract/disc/SCUS_944.23"
