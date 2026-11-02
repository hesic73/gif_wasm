#define MSF_GIF_IMPL
#include "msf_gif.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <cstdint>
#include <vector>

using namespace emscripten;

/**
 * WASM wrapper for msf_gif.
 *
 * Pixel data crosses the JS/WASM boundary as one bulk TypedArray copy in each
 * direction. Never go through per-element embind accessors (vecFromJSArray,
 * val::set in a loop): those cost a JS round trip per byte and dwarf the
 * encoder itself by more than an order of magnitude.
 */
class GifEncoder {
public:
    GifEncoder(int width, int height)
        : width_(width), height_(height), frame_(static_cast<size_t>(width) * height * 4) {
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
     * @param pixelData RGBA pixels, a Uint8Array or Uint8ClampedArray of width*height*4 bytes
     * @param centiSeconds Frame duration in centiseconds
     * @param quality Color quality 1-16 (higher = better)
     */
    bool addFrame(const val& pixelData, int centiSeconds, int quality) {
        if (!valid_ || finished_) return false;
        if (pixelData["length"].as<size_t>() != frame_.size()) return false;

        // The view is created fresh on every call because memory growth detaches old views.
        val(typed_memory_view(frame_.size(), frame_.data())).call<void>("set", pixelData);
        return msf_gif_frame(&state_, frame_.data(), centiSeconds, quality, width_ * 4) != 0;
    }

    /**
     * @returns a Uint8Array owned by JS (not a view of the WASM heap), or null on error
     */
    val finish() {
        if (!valid_ || finished_) return val::null();

        finished_ = true;
        MsfGifResult result = msf_gif_end(&state_);
        if (!result.data) return val::null();

        val out = val(typed_memory_view(result.dataSize, static_cast<uint8_t*>(result.data))).call<val>("slice");
        msf_gif_free(result);
        return out;
    }

    bool isValid() const { return valid_ && !finished_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    MsfGifState state_;
    int width_;
    int height_;
    std::vector<uint8_t> frame_;
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
