// NdkMediaExtractor.h — stub para compilação on-device com Termux clang
// Baseado no NDK r27 público (API 24+)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "NdkMediaFormat.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AMediaExtractor;
typedef struct AMediaExtractor AMediaExtractor;

AMediaExtractor* AMediaExtractor_new(void);
media_status_t   AMediaExtractor_delete(AMediaExtractor* mData);

media_status_t AMediaExtractor_setDataSource(AMediaExtractor* mData, const char* location);

size_t         AMediaExtractor_getTrackCount(AMediaExtractor* mData);
AMediaFormat*  AMediaExtractor_getTrackFormat(AMediaExtractor* mData, size_t idx);
media_status_t AMediaExtractor_selectTrack(AMediaExtractor* mData, size_t idx);
media_status_t AMediaExtractor_unselectTrack(AMediaExtractor* mData, size_t idx);

ssize_t        AMediaExtractor_readSampleData(AMediaExtractor* mData, uint8_t* buffer, size_t capacity);
int64_t        AMediaExtractor_getSampleTime(AMediaExtractor* mData);
uint32_t       AMediaExtractor_getSampleFlags(AMediaExtractor* mData);
bool           AMediaExtractor_advance(AMediaExtractor* mData);
media_status_t AMediaExtractor_seekTo(AMediaExtractor* mData, int64_t seekPosUs, int mode);

/** Open a source given a file descriptor. The extractor duplicates the fd. */
media_status_t AMediaExtractor_setDataSourceFd(AMediaExtractor* mData,
                                               int fd,
                                               off_t offset,
                                               off_t length);

#ifdef __cplusplus
}
#endif
