// NdkMediaFormat.h — stub para compilação on-device com Termux clang
// Baseado no NDK r27 público (API 24+)
#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaFormat;
typedef struct AMediaFormat AMediaFormat;

typedef enum {
    AMEDIA_OK                   =  0,
    AMEDIA_ERROR_BASE           = -10000,
    AMEDIA_ERROR_UNKNOWN        = AMEDIA_ERROR_BASE,
    AMEDIA_ERROR_MALFORMED      = AMEDIA_ERROR_BASE - 1,
    AMEDIA_ERROR_UNSUPPORTED    = AMEDIA_ERROR_BASE - 2,
    AMEDIA_ERROR_INVALID_OBJECT = AMEDIA_ERROR_BASE - 3,
    AMEDIA_ERROR_INVALID_PARAMETER = AMEDIA_ERROR_BASE - 4,
    AMEDIA_ERROR_END_OF_STREAM  = AMEDIA_ERROR_BASE - 5,
    AMEDIA_ERROR_IO             = AMEDIA_ERROR_BASE - 6,
    AMEDIA_ERROR_WOULD_BLOCK    = AMEDIA_ERROR_BASE - 7,
    AMEDIA_DRM_ERROR_BASE       = -20000,
    AMEDIA_DRM_NOT_PROVISIONED  = AMEDIA_DRM_ERROR_BASE - 1,
} media_status_t;

AMediaFormat* AMediaFormat_new(void);
void          AMediaFormat_delete(AMediaFormat* format);

bool AMediaFormat_getInt32(AMediaFormat* format, const char* name, int32_t* out);
bool AMediaFormat_getInt64(AMediaFormat* format, const char* name, int64_t* out);
bool AMediaFormat_getString(AMediaFormat* format, const char* name, const char** out);

void AMediaFormat_setInt32(AMediaFormat* format, const char* name, int32_t value);
void AMediaFormat_setInt64(AMediaFormat* format, const char* name, int64_t value);
void AMediaFormat_setString(AMediaFormat* format, const char* name, const char* value);

// Standard key constants
#define AMEDIAFORMAT_KEY_MIME               "mime"
#define AMEDIAFORMAT_KEY_WIDTH              "width"
#define AMEDIAFORMAT_KEY_HEIGHT             "height"
#define AMEDIAFORMAT_KEY_DURATION           "durationUs"
#define AMEDIAFORMAT_KEY_COLOR_FORMAT       "color-format"
#define AMEDIAFORMAT_KEY_FRAME_RATE         "frame-rate"
#define AMEDIAFORMAT_KEY_MAX_INPUT_SIZE     "max-input-size"

#ifdef __cplusplus
}
#endif
