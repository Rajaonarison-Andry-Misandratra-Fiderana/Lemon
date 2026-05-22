#!/bin/sh
# Post-link binary optimization with LLVM BOLT.
#
# BOLT reorders basic blocks and functions in the already-linked binary
# according to a real run-time profile, packing hot code together so the
# I-cache and branch predictor do less work. Typical wins on a C compositor
# sit in the 5-15% CPU range on the hot path -- complementary to PGO (PGO
# tunes the IR; BOLT tunes the final layout). Run AFTER scripts/pgo-build.sh
# for the largest stacked gain.
#
# Required tools (Arch):
#     sudo pacman -S bolt perf
# Required kernel:
#     /proc/sys/kernel/perf_event_paranoid <= 1  (or run perf as root)
#
# Usage:  scripts/bolt-build.sh [build-dir]
#         scripts/bolt-build.sh build-pgo
set -eu

BUILD="${1:-build}"
BIN="$BUILD/lemon"
PERFDATA="$BUILD/lemon.perf.data"
BOLT_FDATA="$BUILD/lemon.fdata"
BOLT_OUT="$BUILD/lemon.bolt"

[ -x "$BIN" ] || { echo "no binary at $BIN -- build first" >&2; exit 1; }
command -v llvm-bolt   >/dev/null || { echo "install llvm-bolt"   >&2; exit 1; }
command -v perf2bolt   >/dev/null || { echo "install perf2bolt"   >&2; exit 1; }
command -v perf        >/dev/null || { echo "install perf"        >&2; exit 1; }

# Need a binary linked with -Wl,--emit-relocs so BOLT can rewrite it.
if ! readelf -S "$BIN" 2>/dev/null | grep -q '\.rela\.text'; then
	echo "binary lacks relocations -- re-link with -Wl,--emit-relocs"
	echo "(add it to meson link_args in meson.build and rebuild)"
	exit 1
fi

cat <<EOF

>> Pass 1: profile collection with perf (cycles + LBR)
>> The binary will start; use Lemon NORMALLY for 5-10 minutes (switch
>> workspaces, drag/resize, scroll, open/close apps). Exit cleanly.
>> If perf rejects LBR, lower /proc/sys/kernel/perf_event_paranoid to 1.

EOF

perf record -e cycles:u -j any,u -o "$PERFDATA" -- "$BIN"

echo ">> Pass 2: convert perf.data -> BOLT profile"
perf2bolt "$BIN" -p "$PERFDATA" -o "$BOLT_FDATA"

echo ">> Pass 3: rewrite the binary with BOLT"
llvm-bolt "$BIN" -o "$BOLT_OUT" -data="$BOLT_FDATA" \
	-reorder-blocks=ext-tsp -reorder-functions=hfsort+ -split-functions \
	-split-all-cold -split-eh -dyno-stats

cat <<EOF

>> Done. BOLT-optimized binary at:
     $BOLT_OUT

>> To install:
     sudo install -m755 $BOLT_OUT /usr/bin/lemon

>> To verify the layout actually changed:
     readelf -h $BIN     # before
     readelf -h $BOLT_OUT # after -- expect larger text, reordered

EOF
