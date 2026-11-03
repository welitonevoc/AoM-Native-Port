// media_decoder.cpp — Decodificação real de vídeo MP4 via Android MediaCodec NDK.
// API mínima: 24 (NdkMediaExtractor + NdkMediaCodec).
// Entrega frames RGBA8888 via callback on_frame na thread da game loop.

#include "video/media_decoder.h"

#ifdef __ANDROID__
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#define LOG_TAG "AoM-Video"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGI(...) std::printf(__VA_ARGS__)
#define LOGE(...) std::fprintf(stderr, __VA_ARGS__)
#endif

#include <cstring>
#include <algorithm>

namespace aom {
namespace video {

// ─────────────────────────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────────────────────────
struct MediaDecoder::Impl {
#ifdef __ANDROID__
    AMediaExtractor* extractor  = nullptr;
    AMediaCodec*     codec      = nullptr;
    AMediaFormat*    format     = nullptr;
    int              video_track = -1;
    int32_t          width      = 0;
    int32_t          height     = 0;
    int64_t          duration_us = 0;
    int64_t          last_pts_us = 0;
    bool             eos_input  = false;
    bool             eos_output = false;
    bool             started    = false;
    bool             paused     = false;
    std::vector<uint8_t> rgba_buf;

    void release() {
        if (codec)     { AMediaCodec_stop(codec); AMediaCodec_delete(codec); codec = nullptr; }
        if (extractor) { AMediaExtractor_delete(extractor); extractor = nullptr; }
        if (format)    { AMediaFormat_delete(format); format = nullptr; }
        video_track = -1; eos_input = false; eos_output = false;
        started = false; paused = false; last_pts_us = 0;
    }
#endif
};

MediaDecoder::MediaDecoder() : impl_(std::make_unique<Impl>()) {}
MediaDecoder::~MediaDecoder() {
#ifdef __ANDROID__
    impl_->release();
#endif
}

bool MediaDecoder::open(const std::string& path) {
#ifdef __ANDROID__
    impl_->release();
    eos_ = false; playing_ = false;

    impl_->extractor = AMediaExtractor_new();
    if (AMediaExtractor_setDataSource(impl_->extractor, path.c_str()) != AMEDIA_OK) {
        LOGE("AMediaExtractor_setDataSource failed: %s", path.c_str());
        impl_->release(); return false;
    }

    return finish_open();
#else
    (void)path; return false;
#endif
}

// ── Helper: seleciona faixa, cria e inicia o codec ──────────────────────────
bool MediaDecoder::finish_open() {
#ifdef __ANDROID__
    size_t track_count = AMediaExtractor_getTrackCount(impl_->extractor);
    for (size_t i = 0; i < track_count; ++i) {
        AMediaFormat* fmt = AMediaExtractor_getTrackFormat(impl_->extractor, i);
        const char* mime = nullptr;
        AMediaFormat_getString(fmt, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime && std::strncmp(mime, "video/", 6) == 0) {
            impl_->video_track = static_cast<int>(i);
            impl_->format = fmt;
            AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_WIDTH,  &impl_->width);
            AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, &impl_->height);
            int64_t dur = 0;
            AMediaFormat_getInt64(fmt, AMEDIAFORMAT_KEY_DURATION, &dur);
            impl_->duration_us = dur;
            duration_s_ = (dur > 0) ? static_cast<float>(dur) / 1e6f : 0.0f;
            LOGI("Video track %zu: %s %dx%d dur=%.1fs", i, mime,
                 impl_->width, impl_->height, duration_s_);
            break;
        } else { AMediaFormat_delete(fmt); }
    }

    if (impl_->video_track < 0) {
        LOGE("No video track in media source");
        impl_->release(); return false;
    }

    AMediaExtractor_selectTrack(impl_->extractor,
                                static_cast<size_t>(impl_->video_track));

    const char* mime = nullptr;
    AMediaFormat_getString(impl_->format, AMEDIAFORMAT_KEY_MIME, &mime);
    impl_->codec = AMediaCodec_createDecoderByType(mime);
    if (!impl_->codec) {
        LOGE("AMediaCodec_createDecoderByType failed: %s", mime);
        impl_->release(); return false;
    }

    if (AMediaCodec_configure(impl_->codec, impl_->format,
                              nullptr, nullptr, 0) != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed");
        impl_->release(); return false;
    }
    if (AMediaCodec_start(impl_->codec) != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed");
        impl_->release(); return false;
    }

    width_  = static_cast<uint32_t>(impl_->width);
    height_ = static_cast<uint32_t>(impl_->height);
    impl_->rgba_buf.resize(width_ * height_ * 4);
    impl_->started = true;
    playing_ = true;
    current_s_ = 0.0f;
    LOGI("MediaDecoder ready: %ux%u  %.1fs", width_, height_, duration_s_);
    return true;
#else
    return false;
#endif
}

// ── Abertura de asset empacotado no APK ─────────────────────────────────────
bool MediaDecoder::open_asset(void* asset_manager_void,
                              const std::string& asset_path,
                              const std::string& cache_dir) {
#ifdef __ANDROID__
    auto* mgr = static_cast<AAssetManager*>(asset_manager_void);
    if (!mgr) {
        LOGE("open_asset: no AAssetManager available");
        return false;
    }

    std::string in = asset_path;
    if (in.rfind("assets/", 0) == 0) in = in.substr(7);

    AAsset* asset = AAssetManager_open(mgr, in.c_str(), AASSET_MODE_RANDOM);
    if (!asset) {
        LOGI("open_asset: asset not found in APK: %s", in.c_str());
        return false;
    }

    // 1ª tentativa: fd direto (asset armazenado sem compressão)
    off_t fd_off = 0, fd_len = 0;
    int fd = AAsset_openFileDescriptor(asset, &fd_off, &fd_len);
    if (fd >= 0) {
        impl_->release();
        eos_ = false; playing_ = false;
        impl_->extractor = AMediaExtractor_new();
        if (AMediaExtractor_setDataSourceFd(impl_->extractor, fd, fd_off, fd_len)
                == AMEDIA_OK) {
            close(fd);
            AAsset_close(asset);
            if (finish_open()) {
                LOGI("open_asset: opened via file descriptor: %s", in.c_str());
                return true;
            }
            return false;
        }
        LOGI("open_asset: setDataSourceFd failed — falling back to copy");
        close(fd);
        AMediaExtractor_delete(impl_->extractor);
        impl_->extractor = nullptr;
    }

    // Fallback: copia o asset (comprimido) para o cache interno e abre por caminho
    if (cache_dir.empty()) {
        AAsset_close(asset);
        LOGE("open_asset: no cache_dir provided for copy fallback");
        return false;
    }
    mkdir(cache_dir.c_str(), 0774); // ok se já existir

    std::string name = in.substr(in.find_last_of('/') + 1);
    std::string dest = cache_dir + "/" + name;
    FILE* out = fopen(dest.c_str(), "wb");
    if (!out) {
        AAsset_close(asset);
        LOGE("open_asset: cannot create %s", dest.c_str());
        return false;
    }
    std::vector<uint8_t> chunk(64 * 1024);
    long long total = 0;
    for (;;) {
        int n = AAsset_read(asset, chunk.data(), chunk.size());
        if (n <= 0) break;
        fwrite(chunk.data(), 1, static_cast<size_t>(n), out);
        total += n;
    }
    fclose(out);
    AAsset_close(asset);

    if (total <= 0) {
        LOGE("open_asset: copy produced 0 bytes: %s", dest.c_str());
        return false;
    }
    LOGI("open_asset: copied %lld bytes: %s -> %s",
         total, in.c_str(), dest.c_str());
    return open(dest);
#else
    (void)asset_manager_void; (void)asset_path; (void)cache_dir;
    return false;
#endif
}

// ── YUV420 (I420 / NV12) → RGBA8888  BT.601 ─────────────────────────────────
static void yuv_to_rgba(const uint8_t* src, int stride,
                        int w, int h, int color_fmt, uint8_t* dst) {
    const bool nv12 = (color_fmt == 21 || color_fmt == 0x7F420888);
    const uint8_t* Y = src;
    const uint8_t* U = nv12 ? src + stride * h       : src + stride * h;
    const uint8_t* V = nv12 ? U + 1                  : U + (stride / 2) * (h / 2);
    const int uv_row_stride = nv12 ? stride : stride / 2;
    const int uv_col_step   = nv12 ? 2      : 1;

    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int y = Y[row * stride + col] - 16;
            int ui = (row / 2) * uv_row_stride + (col / 2) * uv_col_step;
            int u = U[ui] - 128;
            int v = V[nv12 ? ui : ui] - 128;
            int c = 298 * y;
            uint8_t* px = dst + (row * w + col) * 4;
            px[0] = (uint8_t)std::clamp((c + 409*v + 128) >> 8, 0, 255);
            px[1] = (uint8_t)std::clamp((c - 100*u - 208*v + 128) >> 8, 0, 255);
            px[2] = (uint8_t)std::clamp((c + 516*u + 128) >> 8, 0, 255);
            px[3] = 255;
        }
    }
}

void MediaDecoder::update(float dt) {
#ifdef __ANDROID__
    if (!impl_->started || impl_->paused || impl_->eos_output) return;
    current_s_ += dt;

    // ── Alimenta o codec com amostras do extractor ───────────────────────────
    if (!impl_->eos_input) {
        ssize_t in_idx = AMediaCodec_dequeueInputBuffer(impl_->codec, 0);
        if (in_idx >= 0) {
            size_t buf_size = 0;
            uint8_t* buf = AMediaCodec_getInputBuffer(
                impl_->codec, static_cast<size_t>(in_idx), &buf_size);
            ssize_t sample_size = AMediaExtractor_readSampleData(
                impl_->extractor, buf, buf_size);
            if (sample_size < 0) {
                AMediaCodec_queueInputBuffer(impl_->codec,
                    static_cast<size_t>(in_idx), 0, 0, 0,
                    AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                impl_->eos_input = true;
            } else {
                int64_t pts = AMediaExtractor_getSampleTime(impl_->extractor);
                AMediaCodec_queueInputBuffer(impl_->codec,
                    static_cast<size_t>(in_idx), 0,
                    static_cast<size_t>(sample_size), pts, 0);
                AMediaExtractor_advance(impl_->extractor);
            }
        }
    }

    // ── Drena frame de saída ─────────────────────────────────────────────────
    AMediaCodecBufferInfo info{};
    ssize_t out_idx = AMediaCodec_dequeueOutputBuffer(impl_->codec, &info, 0);

    if (out_idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
        AMediaFormat* fmt = AMediaCodec_getOutputFormat(impl_->codec);
        AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_WIDTH,  &impl_->width);
        AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, &impl_->height);
        AMediaFormat_delete(fmt);
        width_  = static_cast<uint32_t>(impl_->width);
        height_ = static_cast<uint32_t>(impl_->height);
        impl_->rgba_buf.resize(width_ * height_ * 4);
        return;
    }

    if (out_idx >= 0) {
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            impl_->eos_output = true;
            eos_ = true;
            playing_ = false;
            AMediaCodec_releaseOutputBuffer(impl_->codec,
                static_cast<size_t>(out_idx), false);
            LOGI("MediaDecoder: video finished");
            if (on_eos) on_eos();
            return;
        }

        size_t out_size = 0;
        const uint8_t* out_buf = AMediaCodec_getOutputBuffer(
            impl_->codec, static_cast<size_t>(out_idx), &out_size);

        if (out_buf && on_frame && impl_->width > 0 && impl_->height > 0) {
            int32_t color_fmt = 21;
            int32_t stride    = impl_->width;
            AMediaFormat* fmt = AMediaCodec_getOutputFormat(impl_->codec);
            AMediaFormat_getInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color_fmt);
            AMediaFormat_getInt32(fmt, "stride", &stride);
            AMediaFormat_delete(fmt);

            yuv_to_rgba(out_buf, stride,
                        impl_->width, impl_->height,
                        color_fmt, impl_->rgba_buf.data());

            VideoFrame frame;
            frame.rgba   = impl_->rgba_buf;
            frame.width  = width_;
            frame.height = height_;
            frame.pts_us = info.presentationTimeUs;
            on_frame(frame);
        }

        AMediaCodec_releaseOutputBuffer(impl_->codec,
            static_cast<size_t>(out_idx), false);
    }
#else
    (void)dt;
#endif
}

void MediaDecoder::pause()  {
#ifdef __ANDROID__
    impl_->paused = true; playing_ = false;
#endif
}

void MediaDecoder::resume() {
#ifdef __ANDROID__
    if (impl_->started && !impl_->eos_output) { impl_->paused = false; playing_ = true; }
#endif
}

void MediaDecoder::stop() {
#ifdef __ANDROID__
    impl_->release(); playing_ = false; eos_ = false; current_s_ = 0.0f;
#endif
}

bool  MediaDecoder::is_playing() const { return playing_; }
bool  MediaDecoder::is_eos()     const { return eos_;     }
float MediaDecoder::progress()   const {
    return (duration_s_ > 0.0f) ? std::clamp(current_s_ / duration_s_, 0.0f, 1.0f) : 0.0f;
}
float MediaDecoder::duration()   const { return duration_s_; }

} // namespace video
} // namespace aom

