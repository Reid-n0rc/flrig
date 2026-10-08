#!/bin/bash
# FLTK from source into PREFIX (wiki debian_howto "Update the fltk library").
# usage: tests/ci/build_fltk.sh PREFIX   (env FLTK_VERSION, default 1.4.4)
set -eu
PREFIX=$1
V=${FLTK_VERSION:-1.4.4}
T=$(mktemp -d)
cd "$T"
curl -fsSL -o fltk.tar.gz \
	"https://github.com/fltk/fltk/releases/download/release-$V/fltk-$V-source.tar.gz"
tar xzf fltk.tar.gz
cd "fltk-$V"
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
	-DFLTK_BUILD_TEST=OFF -DFLTK_BUILD_EXAMPLES=OFF -DFLTK_BUILD_FLUID=OFF \
	-DFLTK_BUILD_FLTK_OPTIONS=OFF -DFLTK_BACKEND_WAYLAND=OFF \
	-DFLTK_BUILD_GL=OFF > cmake.log || { cat cmake.log; exit 1; }
cmake --build build -j 4 > build.log 2>&1 || { tail -40 build.log; exit 1; }
cmake --install build > /dev/null
"$PREFIX/bin/fltk-config" --version
