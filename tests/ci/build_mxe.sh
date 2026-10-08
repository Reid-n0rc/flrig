#!/bin/bash
# MXE i686 static toolchain with the packages flrig needs, as fldigi's
# scripts/setupmxe.sh "setup32" does it.
# usage: tests/ci/build_mxe.sh DIR COMMIT
set -eu
DIR=$1 COMMIT=$2
git clone -q https://github.com/mxe/mxe.git "$DIR"
cd "$DIR"
git checkout -q "$COMMIT"
T="MXE_TARGETS=i686-w64-mingw32.static"
for pkg in cc zlib libpng pthreads pcre fltk libgnurx; do
	echo "=== $pkg $(date +%T)"
	make -j 4 JOBS=4 $T $pkg > "build-$pkg.log" 2>&1 \
		|| { tail -40 "build-$pkg.log"; exit 1; }
done
make clean-junk > /dev/null 2>&1 || true
rm -rf pkg .ccache
usr/i686-w64-mingw32.static/bin/fltk-config --version
