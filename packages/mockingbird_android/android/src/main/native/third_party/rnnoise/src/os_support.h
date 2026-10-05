/* os_support.h — Mockingbird-provided shim, NOT part of the rnnoise 0.2 release.
 *
 * rnnoise 0.2's vec.h / vec_neon.h include "os_support.h" and use OPUS_CLEAR,
 * but the release tarball ships no such header and defines the macro nowhere
 * (x86 builds take the vec_avx.h branch and never notice). These are the
 * definitions from opus' celt/os_support.h, verbatim in effect. */
#ifndef RNNOISE_OS_SUPPORT_H
#define RNNOISE_OS_SUPPORT_H

#include <stdlib.h>
#include <string.h>

#define OPUS_COPY(dst, src, n) (memcpy((dst), (src), (n) * sizeof(*(dst)) + 0 * ((dst) - (src))))
#define OPUS_MOVE(dst, src, n) (memmove((dst), (src), (n) * sizeof(*(dst)) + 0 * ((dst) - (src))))
#define OPUS_CLEAR(dst, n)     (memset((dst), 0, (n) * sizeof(*(dst))))

static inline void *opus_alloc(size_t size) { return malloc(size); }
static inline void  opus_free(void *ptr)    { free(ptr); }

#endif
