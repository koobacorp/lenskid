#!/bin/sh
# Builds the universal (Apple Silicon + Intel) LENSKID.ofx.bundle, the LUT pack and the zip on macOS.
set -e
cd "$(dirname "$0")"
V=1.0
OUT=build/LENSKID.ofx.bundle
rm -rf build && mkdir -p "$OUT/Contents/MacOS" build/LUTs/LENSKID
ARCH="-arch arm64 -arch x86_64 -mmacosx-version-min=11.0"
clang++ -std=c++17 -O2 -ffp-contract=off $ARCH -fvisibility=hidden -bundle -Iinclude -Isrc src/lenskid.cpp -o "$OUT/Contents/MacOS/LENSKID.ofx"
strip -x "$OUT/Contents/MacOS/LENSKID.ofx"
cp Info.plist "$OUT/Contents/Info.plist"
codesign --force --deep -s - "$OUT"
clang++ -std=c++17 -O2 -ffp-contract=off -Isrc tools/make_luts.cpp -o build/make_luts && build/make_luts build/LUTs/LENSKID
lipo -info "$OUT/Contents/MacOS/LENSKID.ofx"
nm -gU "$OUT/Contents/MacOS/LENSKID.ofx"
(cd build && cp ../INSTALL.txt ../README.md . && zip -qry LENSKID-macOS-universal-v$V.zip LENSKID.ofx.bundle LUTs INSTALL.txt README.md)
ls -la build
