#!/bin/sh
# Two-pass profile-guided optimization build for Lemon.
#
# Pass 1 builds an instrumented binary, you run it normally for a few minutes
# (switch workspaces, move/resize windows, scroll, open/close apps) to gather a
# representative profile, then pass 2 rebuilds using that profile. Expect a
# few percent lower latency and CPU on the hot paths.
#
# Usage:  scripts/pgo-build.sh [build-dir]
set -eu

BUILD="${1:-build-pgo}"

echo ">> Pass 1: instrumented build in '$BUILD'"
meson setup "$BUILD" --buildtype=release -Dprefix=/usr -Dpgo=generate \
	--reconfigure 2>/dev/null || meson setup "$BUILD" --buildtype=release \
	-Dprefix=/usr -Dpgo=generate
ninja -C "$BUILD"

cat <<EOF

>> Now run the instrumented compositor for 5-10 minutes of NORMAL use:

     $BUILD/lemon            (from a TTY)   or
     WAYLAND_DISPLAY=wayland-1 $BUILD/lemon -c assets/lemon.conf   (nested)

   Exercise it: switch workspaces, move/resize/tile windows, scroll, open and
   close apps. Then exit the compositor cleanly.

   When done, re-run this script with the SAME build dir to do pass 2:

     scripts/pgo-build.sh $BUILD --use

EOF

[ "${2:-}" = "--use" ] || exit 0

echo ">> Pass 2: optimized build using collected profile"
meson configure "$BUILD" -Dpgo=use
ninja -C "$BUILD"
echo ">> Done. Install with: sudo ninja -C $BUILD install"
