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
 * GIF Encoder class for creating animated GIFs
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
 * Emscripten module containing the GIF encoder
 */
export interface GifEncoderModule {
    /**
     * GIF Encoder class
     */
    GifEncoder: GifEncoderConstructor;
}

/**
 * Factory function to create the WASM module
 * @returns Promise that resolves to the loaded module
 */
declare function createGifEncoderModule(moduleArg?: Record<string, unknown>): Promise<GifEncoderModule>;

export default createGifEncoderModule;
