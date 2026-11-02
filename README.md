# GIF Encoder WASM

WebAssembly GIF encoder for JavaScript/TypeScript. Converts video frames to animated GIFs in the browser.

## Usage

```typescript
import createGifEncoderModule from './gif_encoder.mjs';

const Module = await createGifEncoderModule();
const encoder = new Module.GifEncoder(width, height);

encoder.addFrame(imageData.data, 10, 16); // RGBA bytes, 10 centiseconds, quality 16
encoder.addFrame(imageData2.data, 10, 16);

const gifData = encoder.finish(); // Uint8Array
encoder.delete();
```

Two encoders share the same frame API:

- `GifEncoder`: uniform palette with ordered dithering ([msf_gif](https://github.com/notnullnotvoid/msf_gif)). Fastest; `quality` (1-16) sets the bit depth.
- `PaletteGifEncoder`: one adaptive palette for the whole GIF, built by `PaletteBuilder` from sample frames (median cut refined with k-means), with no, ordered or error-diffusion dithering. Better colour at some extra cost. Unchanged pixels are left transparent and each frame is cropped to what it changes.

```typescript
const builder = new Module.PaletteBuilder();
for (const sample of sampleFrames) builder.addSample(sample.data);
const palette = builder.build(255); // RGB triplets; one of 256 slots stays free for transparency
builder.delete();

const encoder = new Module.PaletteGifEncoder(width, height, palette, 2); // 0 none, 1 ordered, 2 diffusion
encoder.addFrame(imageData.data, 10);
const gifData = encoder.finish();
encoder.delete();
```

## Build

```bash
./build.sh
```

Requires [Emscripten](https://emscripten.org/) (`em++` on `PATH`). The build enables WASM SIMD (`-msimd128 -msse2`), so msf_gif's SSE2 path is used; every current browser supports it.

## Output

- `build/gif_encoder.mjs` - WASM loader (ES module, works in pages and module workers)
- `build/gif_encoder.wasm` - WASM binary
- `build/gif_encoder.d.ts` - TypeScript types

## Credits

Uses [msf_gif](https://github.com/notnullnotvoid/msf_gif) for the uniform-palette encoder. The adaptive-palette encoder (`src/palette_gif.h`) has no dependencies and builds natively as plain C++17.

## License

MIT
