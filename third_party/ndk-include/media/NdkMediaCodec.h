// NdkMediaCodec.h — stub para compilação on-device com Termux clang
// Baseado no NDK r27 público (API 24+)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "NdkMediaFormat.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaCodec;
typedef struct AMediaCodec AMediaCodec;

/** Buffer info for dequeueOutputBuffer / queueInputBuffer. */
typedef struct AMediaCodecBufferInfo {
    int32_t offset;
    int32_t count;
    int64_t presentationTimeUs;
    uint32_t flags;
} AMediaCodecBufferInfo;

enum {
    AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED = -3,
};

enum {
    AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM = 4,
};

AMediaCodec* AMediaCodec_createDecoderByType(const char* mime_type);
media_status_t AMediaCodec_delete(AMediaCodec* codec);

media_status_t AMediaCodec_configure(AMediaCodec* codec,
                                     const AMediaFormat* format,
                                     void* surface,
                                     void* crypto,
                                     uint32_t flags);
media_status_t AMediaCodec_start(AMediaCodec* codec);
media_status_t AMediaCodec_stop(AMediaCodec* codec);

ssize_t AMediaCodec_dequeueInputBuffer(AMediaCodec* codec, int64_t timeoutUs);
uint8_t* AMediaCodec_getInputBuffer(AMediaCodec* codec, size_t idx, size_t* out_size);
media_status_t AMediaCodec_queueInputBuffer(AMediaCodec* codec,
                                            size_t idx,
                                            off_t offset,
                                            size_t size,
                                            uint64_t time,
                                            uint32_t flags);

ssize_t AMediaCodec_dequeueOutputBuffer(AMediaCodec* codec,
                                        AMediaCodecBufferInfo* info,
                                        int64_t timeoutUs);
AMediaFormat* AMediaCodec_getOutputFormat(AMediaCodec* codec);
uint8_t* AMediaCodec_getOutputBuffer(AMediaCodec* codec, size_t idx, size_t* out_size);
media_status_t AMediaCodec_releaseOutputBuffer(AMediaCodec* codec, size_t idx, bool render);

#ifdef __cplusplus
}
#endif
