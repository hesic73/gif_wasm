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

Uses [msf_gif](https://github.com/notnullnotvoid/msf_gif) for GIF encoding.

## License

MIT
