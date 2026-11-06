#!/bin/sh
# build.sh [--release X.Y]: PINK TROMBONE as a firmware of its own for the M-VAVE FM-1 (fm1-dock with it as the
# only synth, starting in it, and the sine as the fallback) -> build/pinktrombone.fwsc
#   FM1_DOCK      fm1-dock's tree (default ../fm1-dock next to this directory)
#   JIELI_TOOLCHAIN, AC79_SDK   as fm1-dock's README says (~/.jieli/toolchain, ~/fw-AC79_AIoT_SDK)
# Without Linux x86-64 the toolchain runs in Docker (colima is enough).
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(dirname "$0")"
HERE=$PWD
HB="${FM1_DOCK:-$HERE/../fm1-dock}"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
mkdir -p build
(cd "$HB" && HB_MOUNT="$HERE" python3 tools/build.py --modules "$HERE/dock" --name pinktrombone \
    --app-name "PINK TROMBONE" --identity FM-1_944 "$@")
cp "$HB"/build/pinktrombone*.fwsc "$HB"/build/pinktrombone.bin "$HB"/build/pinktrombone_xip.bin "$HB"/build/pinktrombone.syms build/ 2>/dev/null || true
ls -la build/*.fwsc
