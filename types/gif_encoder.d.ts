/**
 * GIF Encoder WASM Module Type Definitions
 * 
 * Usage:
 *   import createGifEncoderModule from './gif_encoder.mjs';
 *   
 *   const Module = await createGifEncoderModule();
 *   const encoder = new Module.GifEncoder(width, height);
 *   encoder.addFrame(pixelData, centiSeconds, quality);
 *   const gifData = encoder.finish();
 */

/**
 * Fast GIF encoder with a uniform palette and ordered dithering (msf_gif)
 */
export interface GifEncoder {
    /**
     * Add a frame to the GIF
     * @param pixelData - RGBA pixel data (length = width * height * 4); `ImageData.data` can be passed as is.
     *                    Copied into WASM memory in one bulk copy.
     * @param centiSeconds - Frame duration in centiseconds (1/100th of a second)
     * @param quality - Color quality 1-16 (16 = best quality, larger file)
     * @returns true if frame was added successfully; false if the length is wrong or the encoder is finished
     */
    addFrame(pixelData: Uint8Array | Uint8ClampedArray, centiSeconds: number, quality: number): boolean;

    /**
     * Finish encoding and get the GIF data
     * @returns Uint8Array containing GIF file data (a JS-owned copy, safe after delete()), or null on error
     */
    finish(): Uint8Array | null;

    /**
     * Check if the encoder is in a valid state
     * @returns true if encoder can accept more frames
     */
    isValid(): boolean;

    /**
     * Get the width of the GIF
     */
    getWidth(): number;

    /**
     * Get the height of the GIF
     */
    getHeight(): number;

    /**
     * Delete the encoder and free memory
     * Call this when done if not calling finish()
     */
    delete(): void;
}

/**
 * GIF Encoder constructor interface
 */
export interface GifEncoderConstructor {
    /**
     * Create a new GIF encoder
     * @param width - Image width in pixels (1-65535)
     * @param height - Image height in pixels (1-65535)
     */
    new(width: number, height: number): GifEncoder;
}

/**
 * Collects colour statistics from sample frames and builds an adaptive palette
 * (variance-minimising median cut refined with k-means).
 */
export interface PaletteBuilder {
    /**
     * Add a sample frame's colours to the histogram
     * @param pixelData - RGBA pixels of any size; `ImageData.data` can be passed as is
     */
    addSample(pixelData: Uint8Array | Uint8ClampedArray): void;

    /**
     * Build the palette
     * @param maxColors - Upper bound on the palette size, 1-255
     * @returns RGB triplets (length = colours * 3); empty if no samples were added
     */
    build(maxColors: number): Uint8Array;

    delete(): void;
}

export interface PaletteBuilderConstructor {
    new(): PaletteBuilder;
}

/** Dithering for {@link PaletteGifEncoder}: 0 = none, 1 = ordered (8x8 Bayer), 2 = error diffusion (Floyd-Steinberg) */
export type Dither = 0 | 1 | 2;

/**
 * GIF encoder with one global palette. Pixels that have not changed since they were last
 * drawn are left transparent and each frame is cropped to the area it changes.
 */
export interface PaletteGifEncoder {
    /**
     * Add a frame to the GIF
     * @param pixelData - RGBA pixel data (length = width * height * 4)
     * @param centiSeconds - Frame duration in centiseconds
     * @returns false if the length is wrong, the encoder is finished, or construction was rejected
     */
    addFrame(pixelData: Uint8Array | Uint8ClampedArray, centiSeconds: number): boolean;

    /**
     * @returns Uint8Array containing GIF file data (a JS-owned copy), or null on error
     */
    finish(): Uint8Array | null;

    /**
     * @returns false if the constructor arguments were rejected or finish() was called
     */
    isValid(): boolean;

    delete(): void;
}

export interface PaletteGifEncoderConstructor {
    /**
     * @param width - Image width in pixels (1-65535)
     * @param height - Image height in pixels (1-65535)
     * @param palette - RGB triplets for 1-255 colours, e.g. from {@link PaletteBuilder.build}
     * @param dither - Dithering method
     */
    new(width: number, height: number, palette: Uint8Array, dither: Dither): PaletteGifEncoder;
}

/**
 * Emscripten module containing the GIF encoders
 */
export interface GifEncoderModule {
    /** Fast encoder with a uniform palette (msf_gif) */
    GifEncoder: GifEncoderConstructor;
    PaletteBuilder: PaletteBuilderConstructor;
    PaletteGifEncoder: PaletteGifEncoderConstructor;
}

/**
 * Factory function to create the WASM module
 * @returns Promise that resolves to the loaded module
 */
declare function createGifEncoderModule(moduleArg?: Record<string, unknown>): Promise<GifEncoderModule>;

export default createGifEncoderModule;
