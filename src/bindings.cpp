#define MSF_GIF_IMPL
#include "msf_gif.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <cstdint>

using namespace emscripten;

/**
 * WASM wrapper for msf_gif library
 * Exposes GIF encoding functionality to JavaScript/TypeScript
 */
class GifEncoder {
public:
    /**
     * Create a new GIF encoder
     * @param width Image width in pixels
     * @param height Image height in pixels
     */
    GifEncoder(int width, int height) : width_(width), height_(height) {
        state_ = {};
        valid_ = msf_gif_begin(&state_, width, height) != 0;
    }
    
    ~GifEncoder() {
        if (valid_ && !finished_) {
            MsfGifResult result = msf_gif_end(&state_);
            msf_gif_free(result);
        }
    }
    
    /**
     * Add a frame to the GIF
     * @param pixelData RGBA pixel data as Uint8Array
     * @param centiSeconds Frame duration in centiseconds (1/100th second)
     * @param quality Color quality 1-16 (higher = better)
     * @returns true on success
     */
    bool addFrame(const val& pixelData, int centiSeconds, int quality) {
        if (!valid_ || finished_) return false;
        
        std::vector<uint8_t> pixels = vecFromJSArray<uint8_t>(pixelData);
        
        int pitchInBytes = width_ * 4;
        return msf_gif_frame(&state_, pixels.data(), centiSeconds, quality, pitchInBytes) != 0;
    }
    
    /**
     * Finish encoding and return the GIF data
     * @returns Uint8Array containing GIF data, or null on error
     */
    val finish() {
        if (!valid_ || finished_) {
            return val::null();
        }
        
        finished_ = true;
        MsfGifResult result = msf_gif_end(&state_);
        
        if (!result.data) {
            return val::null();
        }
        
        val Uint8Array = val::global("Uint8Array");
        val jsArray = Uint8Array.new_(result.dataSize);
        
        for (size_t i = 0; i < result.dataSize; ++i) {
            jsArray.set(i, static_cast<uint8_t*>(result.data)[i]);
        }
        
        msf_gif_free(result);
        return jsArray;
    }
    
    /**
     * Check if encoder is in valid state
     */
    bool isValid() const { return valid_ && !finished_; }
    
    /**
     * Get the width of the GIF
     */
    int getWidth() const { return width_; }
    
    /**
     * Get the height of the GIF
     */
    int getHeight() const { return height_; }

private:
    MsfGifState state_;
    int width_;
    int height_;
    bool valid_ = false;
    bool finished_ = false;
};

EMSCRIPTEN_BINDINGS(gif_encoder) {
    class_<GifEncoder>("GifEncoder")
        .constructor<int, int>()
        .function("addFrame", &GifEncoder::addFrame)
        .function("finish", &GifEncoder::finish)
        .function("isValid", &GifEncoder::isValid)
        .function("getWidth", &GifEncoder::getWidth)
        .function("getHeight", &GifEncoder::getHeight);
}
