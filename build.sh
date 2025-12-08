#!/bin/bash
set -e

# Create build directory
mkdir -p build
cd build

# Configure with Emscripten
emcmake cmake ..

# Build
emmake make -j$(nproc)

# Copy TypeScript types if they exist in parent directory
if [ -f "../types/gif_encoder.d.ts" ]; then
    cp ../types/gif_encoder.d.ts .
fi

echo ""
echo "Build complete!"
echo "Output files:"
ls -la gif_encoder.js gif_encoder.wasm gif_encoder.d.ts 2>/dev/null || echo "  (check for build errors above)"
