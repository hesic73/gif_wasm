// Adaptive-palette GIF encoding: palette generation, dithering and a GIF writer.
// Plain C++17 with no Emscripten dependency, so it can be built and tested natively.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace pgif {

// Channel weights for squared RGB distance; green differences are the most visible, blue the least.
constexpr float kWeight[3] = {2.0f, 4.0f, 3.0f};

inline float dist2(const float* a, const float* b) {
    float dr = a[0] - b[0], dg = a[1] - b[1], db = a[2] - b[2];
    return kWeight[0] * dr * dr + kWeight[1] * dg * dg + kWeight[2] * db * db;
}

struct Color3 { float v[3]; };

inline uint8_t clampByte(float v) { return static_cast<uint8_t>(v < 0.f ? 0.f : v > 255.f ? 255.f : v + 0.5f); }

/**
 * Accumulates a colour histogram over sample frames and builds a palette from it:
 * variance-minimising median cut for the initial palette, then weighted k-means to refine it.
 */
class PaletteBuilder {
public:
    PaletteBuilder() : count_(kBins, 0), sum_(kBins * 3, 0) {}

    void addSample(const uint8_t* rgba, size_t pixelCount) {
        for (size_t i = 0; i < pixelCount; ++i) {
            const uint8_t* p = rgba + i * 4;
            uint32_t bin = (p[0] >> 3) << 10 | (p[1] >> 3) << 5 | (p[2] >> 3);
            count_[bin] += 1;
            sum_[bin * 3 + 0] += p[0];
            sum_[bin * 3 + 1] += p[1];
            sum_[bin * 3 + 2] += p[2];
        }
    }

    /** Returns up to maxColors RGB triplets; empty if no samples were added. */
    std::vector<uint8_t> build(int maxColors) const {
        std::vector<Bin> bins;
        for (int i = 0; i < kBins; ++i) {
            if (!count_[i]) continue;
            double w = count_[i];
            bins.push_back({{float(sum_[i * 3] / w), float(sum_[i * 3 + 1] / w), float(sum_[i * 3 + 2] / w)}, w});
        }
        std::vector<Color3> centers;
        if (static_cast<int>(bins.size()) <= maxColors) {
            for (const Bin& b : bins) centers.push_back({b.c[0], b.c[1], b.c[2]});
        } else {
            centers = medianCut(bins, maxColors);
            refine(bins, centers, 10);
        }
        std::vector<uint8_t> palette;
        for (const auto& c : centers) {
            palette.push_back(clampByte(c.v[0]));
            palette.push_back(clampByte(c.v[1]));
            palette.push_back(clampByte(c.v[2]));
        }
        return palette;
    }

private:
    static constexpr int kBins = 1 << 15;
    struct Bin { float c[3]; double w; };
    struct Box { size_t begin, end; double sse; };
    struct Stats { double w, s[3], q[3]; };

    std::vector<uint32_t> count_;
    std::vector<uint64_t> sum_;

    static double sseOf(const Stats& st) {
        if (st.w <= 0) return 0;
        double e = 0;
        for (int c = 0; c < 3; ++c) e += kWeight[c] * (st.q[c] - st.s[c] * st.s[c] / st.w);
        return e > 0 ? e : 0;
    }

    static Stats statsOf(const std::vector<Bin>& bins, size_t begin, size_t end) {
        Stats st{};
        for (size_t i = begin; i < end; ++i) {
            const Bin& b = bins[i];
            st.w += b.w;
            for (int c = 0; c < 3; ++c) { st.s[c] += b.w * b.c[c]; st.q[c] += b.w * b.c[c] * b.c[c]; }
        }
        return st;
    }

    static std::vector<Color3> medianCut(std::vector<Bin>& bins, int maxColors) {
        std::vector<Box> boxes{{0, bins.size(), sseOf(statsOf(bins, 0, bins.size()))}};
        while (static_cast<int>(boxes.size()) < maxColors) {
            int pick = -1;
            for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
                if (boxes[i].end - boxes[i].begin < 2 || boxes[i].sse <= 0) continue;
                if (pick < 0 || boxes[i].sse > boxes[pick].sse) pick = i;
            }
            if (pick < 0) break;
            Box box = boxes[pick];
            Stats total = statsOf(bins, box.begin, box.end);

            int axis = 0;
            double best = -1;
            for (int c = 0; c < 3; ++c) {
                double var = kWeight[c] * (total.q[c] - total.s[c] * total.s[c] / total.w);
                if (var > best) { best = var; axis = c; }
            }
            std::sort(bins.begin() + box.begin, bins.begin() + box.end,
                      [axis](const Bin& a, const Bin& b) { return a.c[axis] < b.c[axis]; });

            // Split where the summed error of both halves is smallest.
            Stats left{};
            size_t split = box.begin + 1;
            double bestSse = -1, leftSse = 0, rightSse = 0;
            for (size_t i = box.begin; i + 1 < box.end; ++i) {
                const Bin& b = bins[i];
                left.w += b.w;
                for (int c = 0; c < 3; ++c) { left.s[c] += b.w * b.c[c]; left.q[c] += b.w * b.c[c] * b.c[c]; }
                Stats right{total.w - left.w, {}, {}};
                for (int c = 0; c < 3; ++c) { right.s[c] = total.s[c] - left.s[c]; right.q[c] = total.q[c] - left.q[c]; }
                double l = sseOf(left), r = sseOf(right);
                if (bestSse < 0 || l + r < bestSse) { bestSse = l + r; split = i + 1; leftSse = l; rightSse = r; }
            }
            boxes[pick] = {box.begin, split, leftSse};
            boxes.push_back({split, box.end, rightSse});
        }

        std::vector<Color3> centers;
        for (const Box& box : boxes) {
            Stats st = statsOf(bins, box.begin, box.end);
            centers.push_back({{float(st.s[0] / st.w), float(st.s[1] / st.w), float(st.s[2] / st.w)}});
        }
        return centers;
    }

    static void refine(const std::vector<Bin>& bins, std::vector<Color3>& centers, int iterations) {
        const size_t k = centers.size();
        std::vector<double> acc(k * 4);
        for (int it = 0; it < iterations; ++it) {
            std::fill(acc.begin(), acc.end(), 0.0);
            for (const Bin& b : bins) {
                size_t nearest = 0;
                float bestD = dist2(b.c, centers[0].v);
                for (size_t j = 1; j < k; ++j) {
                    float d = dist2(b.c, centers[j].v);
                    if (d < bestD) { bestD = d; nearest = j; }
                }
                double* a = &acc[nearest * 4];
                a[0] += b.w;
                for (int c = 0; c < 3; ++c) a[c + 1] += b.w * b.c[c];
            }
            float moved = 0;
            for (size_t j = 0; j < k; ++j) {
                const double* a = &acc[j * 4];
                if (a[0] <= 0) continue;  // empty cluster keeps its colour
                for (int c = 0; c < 3; ++c) {
                    float v = float(a[c + 1] / a[0]);
                    moved = std::max(moved, std::fabs(v - centers[j].v[c]));
                    centers[j].v[c] = v;
                }
            }
            if (moved < 0.25f) break;
        }
    }
};

enum class Dither { None = 0, Ordered = 1, Diffusion = 2 };

/**
 * Writes an animated GIF with one global palette. Pixels whose source colour has not moved more
 * than `kKeepThreshold` since they were last drawn are left transparent, so they keep showing the
 * previous frame: static areas compress to almost nothing and dither patterns do not shimmer.
 * Each frame is cropped to the bounding box of the pixels it changes.
 */
class PaletteGifEncoder {
public:
    static constexpr int kKeepThreshold = 4;

    PaletteGifEncoder(int width, int height, const uint8_t* palette, int colors, Dither dither)
        : width_(width), height_(height), colors_(colors), dither_(dither),
          cache_(1 << 18, -1), shownSrc_(size_t(width) * height * 3), shownIdx_(size_t(width) * height),
          frameIdx_(size_t(width) * height) {
        for (int i = 0; i < colors; ++i)
            for (int c = 0; c < 3; ++c) palette_.push_back(palette[i * 3 + c]);

        transparent_ = colors;  // one slot past the real colours
        tableBits_ = 2;
        while ((1 << tableBits_) < colors + 1) ++tableBits_;
        ditherAmp_ = paletteSpacing();
        if (dither_ == Dither::Diffusion) {
            errCur_.assign(size_t(width + 2) * 3, 0.f);
            errNext_.assign(size_t(width + 2) * 3, 0.f);
        }
        writeHeader();
    }

    bool addFrame(const uint8_t* rgba, int centiSeconds) {
        if (finished_) return false;
        switch (dither_) {
            case Dither::None: mapFrame<Dither::None>(rgba); break;
            case Dither::Ordered: mapFrame<Dither::Ordered>(rgba); break;
            case Dither::Diffusion: mapFrame<Dither::Diffusion>(rgba); break;
        }
        writeFrame(centiSeconds);
        hasPrev_ = true;
        return true;
    }

    std::vector<uint8_t> finish() {
        if (!finished_) { out_.push_back(0x3B); finished_ = true; }
        return std::move(out_);
    }

private:
    int width_, height_, colors_;
    Dither dither_;
    std::vector<float> palette_;
    int transparent_ = 0, tableBits_ = 2;
    float ditherAmp_ = 0;
    std::vector<int16_t> cache_;      // 6-bit-per-channel colour -> nearest palette index
    std::vector<uint8_t> shownSrc_;   // source colour each pixel had when it was last drawn
    std::vector<uint8_t> shownIdx_;   // palette index currently displayed at each pixel
    std::vector<uint8_t> frameIdx_;   // this frame's output, transparent where unchanged
    std::vector<float> errCur_, errNext_;
    std::vector<uint8_t> out_;
    std::vector<int16_t> lzw_;
    bool hasPrev_ = false, finished_ = false;

    int nearest(float r, float g, float b) {
        int ri = clampByte(r) >> 2, gi = clampByte(g) >> 2, bi = clampByte(b) >> 2;
        int key = ri << 12 | gi << 6 | bi;
        int16_t& slot = cache_[key];
        if (slot < 0) {
            float c[3] = {ri * 4 + 1.5f, gi * 4 + 1.5f, bi * 4 + 1.5f};
            int best = 0;
            float bestD = dist2(c, &palette_[0]);
            for (int j = 1; j < colors_; ++j) {
                float d = dist2(c, &palette_[j * 3]);
                if (d < bestD) { bestD = d; best = j; }
            }
            slot = static_cast<int16_t>(best);
        }
        return slot;
    }

    // Typical distance between neighbouring palette colours; sets the ordered dither amplitude.
    float paletteSpacing() const {
        if (colors_ < 2) return 0;
        std::vector<float> nn;
        for (int i = 0; i < colors_; ++i) {
            float best = 1e9f;
            for (int j = 0; j < colors_; ++j) {
                if (i == j) continue;
                float d = 0;
                for (int c = 0; c < 3; ++c) { float t = palette_[i * 3 + c] - palette_[j * 3 + c]; d += t * t; }
                best = std::min(best, d);
            }
            nn.push_back(std::sqrt(best));
        }
        std::nth_element(nn.begin(), nn.begin() + nn.size() / 2, nn.end());
        return nn[nn.size() / 2];
    }

    template <Dither D>
    void mapFrame(const uint8_t* rgba) {
        static const uint8_t bayer[8][8] = {
            {0, 32, 8, 40, 2, 34, 10, 42}, {48, 16, 56, 24, 50, 18, 58, 26},
            {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
            {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
            {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21},
        };
        for (int y = 0; y < height_; ++y) {
            // Serpentine scan for error diffusion avoids the diagonal drift of always going left to right.
            const bool reverse = D == Dither::Diffusion && (y & 1);
            if (D == Dither::Diffusion) {
                std::swap(errCur_, errNext_);
                std::fill(errNext_.begin(), errNext_.end(), 0.f);
            }
            for (int step = 0; step < width_; ++step) {
                const int x = reverse ? width_ - 1 - step : step;
                const size_t i = size_t(y) * width_ + x;
                const uint8_t* p = rgba + i * 4;
                uint8_t* shown = &shownSrc_[i * 3];

                const bool keep = hasPrev_ && std::abs(p[0] - shown[0]) <= kKeepThreshold &&
                                  std::abs(p[1] - shown[1]) <= kKeepThreshold &&
                                  std::abs(p[2] - shown[2]) <= kKeepThreshold;
                if (keep && D != Dither::Diffusion) {
                    frameIdx_[i] = static_cast<uint8_t>(transparent_);
                    continue;
                }

                float v[3] = {float(p[0]), float(p[1]), float(p[2])};
                if (D == Dither::Ordered) {
                    float offset = ((bayer[y & 7][x & 7] + 0.5f) / 64.f - 0.5f) * ditherAmp_;
                    for (int c = 0; c < 3; ++c) v[c] += offset;
                } else if (D == Dither::Diffusion) {
                    for (int c = 0; c < 3; ++c) v[c] = std::min(255.f, std::max(0.f, v[c] + errCur_[(x + 1) * 3 + c]));
                }
                // A kept pixel counts as quantized to the colour it already shows, so its error still
                // diffuses; dropping it leaves neighbours uncorrected and brings banding back.
                const int idx = keep ? shownIdx_[i] : nearest(v[0], v[1], v[2]);

                if (D == Dither::Diffusion) {
                    const int dir = reverse ? -1 : 1;
                    for (int c = 0; c < 3; ++c) {
                        // 7/8 strength keeps gradients smooth while cutting the noisiest isolated pixels
                        float e = (v[c] - palette_[idx * 3 + c]) * 0.875f;
                        errCur_[(x + 1 + dir) * 3 + c] += e * 7.f / 16.f;
                        errNext_[(x + 1 - dir) * 3 + c] += e * 3.f / 16.f;
                        errNext_[(x + 1) * 3 + c] += e * 5.f / 16.f;
                        errNext_[(x + 1 + dir) * 3 + c] += e * 1.f / 16.f;
                    }
                    if (keep) {
                        frameIdx_[i] = static_cast<uint8_t>(transparent_);
                        continue;
                    }
                }

                shown[0] = p[0]; shown[1] = p[1]; shown[2] = p[2];
                // Redrawing a pixel with the colour it already shows is wasted bytes.
                frameIdx_[i] = hasPrev_ && shownIdx_[i] == idx ? static_cast<uint8_t>(transparent_) : static_cast<uint8_t>(idx);
                shownIdx_[i] = static_cast<uint8_t>(idx);
            }
        }
    }

    void put16(int v) { out_.push_back(v & 0xFF); out_.push_back((v >> 8) & 0xFF); }

    void writeHeader() {
        const char* sig = "GIF89a";
        out_.insert(out_.end(), sig, sig + 6);
        put16(width_);
        put16(height_);
        out_.push_back(static_cast<uint8_t>(0x80 | 0x70 | (tableBits_ - 1)));  // global table, 8-bit colour resolution
        out_.push_back(0);  // background colour index
        out_.push_back(0);  // pixel aspect ratio
        for (int i = 0; i < (1 << tableBits_); ++i)
            for (int c = 0; c < 3; ++c) out_.push_back(i < colors_ ? static_cast<uint8_t>(palette_[i * 3 + c]) : 0);
        const uint8_t loop[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};
        out_.insert(out_.end(), loop, loop + sizeof(loop));
    }

    void writeFrame(int centiSeconds) {
        int x0 = width_, y0 = height_, x1 = -1, y1 = -1;
        for (int y = 0; y < height_; ++y) {
            const uint8_t* row = &frameIdx_[size_t(y) * width_];
            for (int x = 0; x < width_; ++x) {
                if (row[x] == transparent_) continue;
                x0 = std::min(x0, x); x1 = std::max(x1, x);
                y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
        }
        if (x1 < 0) { x0 = y0 = x1 = y1 = 0; }  // nothing changed: a 1x1 transparent frame carries the delay
        const int w = x1 - x0 + 1, h = y1 - y0 + 1;

        // Graphic control extension: keep previous frame (disposal 1), transparency on
        const uint8_t gce[] = {0x21, 0xF9, 0x04, 0x05, static_cast<uint8_t>(centiSeconds & 0xFF),
                               static_cast<uint8_t>((centiSeconds >> 8) & 0xFF), static_cast<uint8_t>(transparent_), 0x00};
        out_.insert(out_.end(), gce, gce + sizeof(gce));
        out_.push_back(0x2C);
        put16(x0); put16(y0); put16(w); put16(h);
        out_.push_back(0x00);  // no local colour table, not interlaced

        std::vector<uint8_t> crop(size_t(w) * h);
        for (int y = 0; y < h; ++y)
            std::memcpy(&crop[size_t(y) * w], &frameIdx_[size_t(y + y0) * width_ + x0], w);
        writeLzw(crop);
    }

    // GIF LZW, with the code-width rule mirrored from msf_gif's encoder.
    void writeLzw(const std::vector<uint8_t>& indices) {
        const int minCodeSize = tableBits_;
        const int tableSize = 1 << minCodeSize;
        const int stride = tableSize;
        if (lzw_.empty()) lzw_.resize(size_t(4096) * stride);
        out_.push_back(static_cast<uint8_t>(minCodeSize));

        std::vector<uint8_t> bytes;
        uint32_t bitBuf = 0;
        int bitCount = 0;
        auto put = [&](int bits, int code) {
            bitBuf |= uint32_t(code) << bitCount;
            bitCount += bits;
            while (bitCount >= 8) { bytes.push_back(bitBuf & 0xFF); bitBuf >>= 8; bitCount -= 8; }
        };
        auto bitLog = [](int v) { int n = 0; while (v) { ++n; v >>= 1; } return n; };
        auto reset = [&]() { std::fill(lzw_.begin(), lzw_.end(), int16_t(-1)); return tableSize + 2; };

        int len = reset();
        put(bitLog(len - 1), tableSize);  // clear code
        int last = indices[0];
        for (size_t i = 1; i < indices.size(); ++i) {
            const int color = indices[i];
            const int code = lzw_[size_t(last) * stride + color];
            if (code >= 0) { last = code; continue; }
            const int bits = bitLog(len - 1);
            put(bits, last);
            if (len > 4095) {
                put(bits, tableSize);
                len = reset();
            } else {
                lzw_[size_t(last) * stride + color] = static_cast<int16_t>(len++);
            }
            last = color;
        }
        put(std::min(12, bitLog(len - 1)), last);
        put(std::min(12, bitLog(len)), tableSize + 1);  // end of information
        if (bitCount > 0) bytes.push_back(bitBuf & 0xFF);

        for (size_t i = 0; i < bytes.size(); i += 255) {
            const size_t n = std::min<size_t>(255, bytes.size() - i);
            out_.push_back(static_cast<uint8_t>(n));
            out_.insert(out_.end(), bytes.begin() + i, bytes.begin() + i + n);
        }
        out_.push_back(0x00);  // block terminator
    }
};

}  // namespace pgif
