#!/bin/bash
# Builds build/gif_encoder.mjs (ES module loader) and build/gif_encoder.wasm.
# Requires Emscripten (emcc on PATH) and the msf_gif submodule.
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f vendor/msf_gif/msf_gif.h ]; then
    git submodule update --init
fi

mkdir -p build

# -msimd128 -msse2 compile msf_gif's SSE2 path down to WASM SIMD; without them
# __SSE2__ is undefined and the scalar fallback runs instead.
em++ src/bindings.cpp \
    -I vendor/msf_gif \
    -std=c++17 \
    -O3 -flto \
    -msimd128 -msse2 \
    -lembind \
    -sMODULARIZE=1 \
    -sEXPORT_ES6=1 \
    -sEXPORT_NAME=createGifEncoderModule \
    -sALLOW_MEMORY_GROWTH=1 \
    -sENVIRONMENT=web,worker \
    -o build/gif_encoder.mjs

cp types/gif_encoder.d.ts build/gif_encoder.d.ts

ls -la build/gif_encoder.mjs build/gif_encoder.wasm build/gif_encoder.d.ts
