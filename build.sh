#!/bin/bash

set -e
set -x

if [ ! -d "./emsdk" ]; then
  git clone https://github.com/emscripten-core/emsdk.git --branch 1.39.20
  ./emsdk/emsdk install 1.39.20-fastcomp
  ./emsdk/emsdk activate 1.39.20-fastcomp
fi

source ./emsdk/emsdk_env.sh >/dev/null 2>&1
source ./.venv/bin/activate >/dev/null 2>&1

if [ ! -f "doomgeneric/doom1.wad" ]; then
  wget "https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad" -O "doomgeneric/doom1.wad"
fi
if [ "$1" = "clean" ]; then
  emmake make -C doomgeneric -f Makefile.pdfjs clean
fi
emmake make -C doomgeneric -f Makefile.pdfjs -j$(nproc --all)

mkdir -p out
cat pre.js doomgeneric/doomgeneric.js > out/compiled.js
#emcc test.c -s WASM=0 -o out/compiled.js -Wno-fastcomp
python3 generate.py out/compiled.js out/out.pdf