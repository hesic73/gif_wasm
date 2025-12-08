# GIF Encoder WASM

WebAssembly GIF encoder for JavaScript/TypeScript. Converts video frames to animated GIFs in the browser.

## Usage

```typescript
import createGifEncoderModule from './gif_encoder';

const Module = await createGifEncoderModule();
const encoder = new Module.GifEncoder(width, height);

encoder.addFrame(rgbaPixels, 10, 16); // 10 centiseconds, quality 16
encoder.addFrame(rgbaPixels2, 10, 16);

const gifData = encoder.finish(); // Uint8Array
```

## Build

```bash
git submodule update --init
./build.sh
```

Requires [Emscripten](https://emscripten.org/).

## Output

- `build/gif_encoder.js` - WASM loader
- `build/gif_encoder.wasm` - WASM binary
- `build/gif_encoder.d.ts` - TypeScript types

## Credits

Uses [msf_gif](https://github.com/notnullnotvoid/msf_gif) for GIF encoding.

## License

MIT
