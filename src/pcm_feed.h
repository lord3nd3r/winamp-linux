#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>

// projectM 2.x addPCMfloat() reads `samples` mono floats and copies each one
// into both channels. addPCM16Data() reads `samples` interleaved stereo frames
// (2 shorts each). These helpers build exactly those buffers and never index
// past `frames * channels` samples of the source.

inline int copyMonoFloat(const float *src, int frames, int channels, float *dst, int dstCap) {
    if (!src || !dst || frames <= 0 || channels <= 0 || dstCap <= 0)
        return 0;
    const int n = std::min(frames, dstCap);
    for (int i = 0; i < n; ++i) {
        const float *frame = src + static_cast<std::size_t>(i) * static_cast<std::size_t>(channels);
        float sum = 0.f;
        for (int c = 0; c < channels; ++c)
            sum += frame[c];
        dst[i] = sum / static_cast<float>(channels);
    }
    return n;
}

inline int copyStereoInt16(const std::int16_t *src, int frames, int channels,
                            std::int16_t *dst, int dstFrames) {
    if (!src || !dst || frames <= 0 || channels <= 0 || dstFrames <= 0)
        return 0;
    const int n = std::min(frames, dstFrames);
    for (int i = 0; i < n; ++i) {
        const std::int16_t *frame = src + static_cast<std::size_t>(i) * static_cast<std::size_t>(channels);
        const std::int16_t left = frame[0];
        const std::int16_t right = channels > 1 ? frame[1] : left;
        dst[static_cast<std::size_t>(i) * 2] = left;
        dst[static_cast<std::size_t>(i) * 2 + 1] = right;
    }
    return n;
}
