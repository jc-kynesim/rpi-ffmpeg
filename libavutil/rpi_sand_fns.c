#include "config.h"
#include <stdint.h>
#include <string.h>
#include "rpi_sand_fns.h"
#include "avassert.h"

#define PW 1
#include "rpi_sand_fn_pw.h"
#undef PW

#define PW 2
#include "rpi_sand_fn_pw.h"
#undef PW

#if HAVE_NEON
void rpi_sand128b_stripe_to_8_10(uint8_t * dest, const uint8_t * src1, const uint8_t * src2, unsigned int lines);
#endif

#if 1
// Simple round
static void cpy16_to_8(uint8_t * dst, const uint8_t * _src, unsigned int n, const unsigned int shr)
{
    const unsigned int rnd = (1 << shr) >> 1;
    const uint16_t * src = (const uint16_t *)_src;

    for (; n != 0; --n) {
        *dst++ = (*src++ + rnd) >> shr;
    }
}
#else
// Dithered variation
static void cpy16_to_8(uint8_t * dst, const uint8_t * _src, unsigned int n, const unsigned int shr)
{
    unsigned int rnd = (1 << shr) >> 1;
    const unsigned int mask = ((1 << shr) - 1);
    const uint16_t * src = (const uint16_t *)_src;

    for (; n != 0; --n) {
        rnd = *src++ + (rnd & mask);
        *dst++ = rnd >> shr;
    }
}
#endif

// w/h in pixels
void av_rpi_sand16_to_sand8(uint8_t * dst, const unsigned int dst_stride1, const unsigned int dst_stride2,
                         const uint8_t * src, const unsigned int src_stride1, const unsigned int src_stride2,
                         unsigned int w, unsigned int h, const unsigned int shr)
{
    const unsigned int n = dst_stride1 / 2;
    unsigned int j;

    // This is true for our current layouts
    av_assert0(dst_stride1 == src_stride1);

    // As we have the same stride1 for src & dest and src is wider than dest
    // then if we loop on src we can always write contiguously to dest
    // We make no effort to copy an exact width - round up to nearest src stripe
    // as we will always have storage in dest for that

#if HAVE_NEON
    if (shr == 3 && src_stride1 == 128) {
        for (j = 0; j + n < w; j += dst_stride1) {
            uint8_t * d = dst + j * dst_stride2;
            const uint8_t * s1 = src + j * 2 * src_stride2;
            const uint8_t * s2 = s1 + src_stride1 * src_stride2;

            rpi_sand128b_stripe_to_8_10(d, s1, s2, h);
        }
    }
    else
#endif
    {
        for (j = 0; j + n < w; j += dst_stride1) {
            uint8_t * d = dst + j * dst_stride2;
            const uint8_t * s1 = src + j * 2 * src_stride2;
            const uint8_t * s2 = s1 + src_stride1 * src_stride2;

            for (unsigned int i = 0; i != h; ++i, s1 += src_stride1, s2 += src_stride1, d += dst_stride1) {
                cpy16_to_8(d, s1, n, shr);
                cpy16_to_8(d + n, s2, n, shr);
            }
        }
    }

    // Fix up a trailing dest half stripe
    if (j < w) {
        uint8_t * d = dst + j * dst_stride2;
        const uint8_t * s1 = src + j * 2 * src_stride2;

        for (unsigned int i = 0; i != h; ++i, s1 += src_stride1, d += dst_stride1) {
            cpy16_to_8(d, s1, n, shr);
        }
    }
}

