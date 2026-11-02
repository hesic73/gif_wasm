#define MSF_GIF_IMPL
#include "msf_gif.h"
#include "palette_gif.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <algorithm>
#include <cstdint>
#include <memory>
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

// Copies a JS Uint8Array/Uint8ClampedArray into `dst` with one bulk TypedArray.set.
// The view is created per call because memory growth detaches old views.
static void copyIn(const val& src, std::vector<uint8_t>& dst) {
    dst.resize(src["length"].as<size_t>());
    val(typed_memory_view(dst.size(), dst.data())).call<void>("set", src);
}

/** Collects colour statistics from sample frames and builds an adaptive palette. */
class PaletteBuilder {
public:
    /** @param pixelData RGBA pixels of any size */
    void addSample(const val& pixelData) {
        copyIn(pixelData, buffer_);
        builder_.addSample(buffer_.data(), buffer_.size() / 4);
    }

    /** @returns RGB triplets, at most maxColors (1-255) of them; empty if no samples were added */
    val build(int maxColors) {
        std::vector<uint8_t> palette = builder_.build(std::max(1, std::min(255, maxColors)));
        return val(typed_memory_view(palette.size(), palette.data())).call<val>("slice");
    }

private:
    pgif::PaletteBuilder builder_;
    std::vector<uint8_t> buffer_;
};

/** GIF encoder with one global, caller-supplied palette and a choice of dithering. */
class PaletteGifEncoder {
public:
    /**
     * @param palette RGB triplets, 1-255 colours (one table slot is kept for transparency)
     * @param dither 0 = none, 1 = ordered, 2 = error diffusion
     */
    PaletteGifEncoder(int width, int height, const val& palette, int dither) : width_(width), height_(height) {
        std::vector<uint8_t> colors;
        copyIn(palette, colors);
        const size_t count = colors.size() / 3;
        if (width < 1 || height < 1 || width > 65535 || height > 65535 || colors.size() % 3 || count < 1 ||
            count > 255 || dither < 0 || dither > 2) {
            return;
        }
        encoder_ = std::make_unique<pgif::PaletteGifEncoder>(width, height, colors.data(), static_cast<int>(count),
                                                             static_cast<pgif::Dither>(dither));
    }

    bool addFrame(const val& pixelData, int centiSeconds) {
        if (!isValid()) return false;
        if (pixelData["length"].as<size_t>() != static_cast<size_t>(width_) * height_ * 4) return false;
        copyIn(pixelData, frame_);
        return encoder_->addFrame(frame_.data(), centiSeconds);
    }

    val finish() {
        if (!isValid()) return val::null();
        finished_ = true;
        std::vector<uint8_t> gif = encoder_->finish();
        return val(typed_memory_view(gif.size(), gif.data())).call<val>("slice");
    }

    /** False if the constructor arguments were rejected or finish() was called. */
    bool isValid() const { return encoder_ && !finished_; }

private:
    int width_, height_;
    std::unique_ptr<pgif::PaletteGifEncoder> encoder_;
    std::vector<uint8_t> frame_;
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

    class_<PaletteBuilder>("PaletteBuilder")
        .constructor<>()
        .function("addSample", &PaletteBuilder::addSample)
        .function("build", &PaletteBuilder::build);

    class_<PaletteGifEncoder>("PaletteGifEncoder")
        .constructor<int, int, const val&, int>()
        .function("addFrame", &PaletteGifEncoder::addFrame)
        .function("finish", &PaletteGifEncoder::finish)
        .function("isValid", &PaletteGifEncoder::isValid);
}
