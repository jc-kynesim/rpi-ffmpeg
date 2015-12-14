#ifndef AVCODEC_ALT1CABAC_FNS_H
#define AVCODEC_ALT1CABAC_FNS_H

#if ALTCABAC_VER != 1
#error Unexpected CABAC VAR
#endif

#if ARCH_ARM
#   include "arm/cabac.h"
#else
// Helper fns
static inline uint32_t bmem_peek4(const void * buf, const unsigned int offset)
{
    const uint8_t * const p = (const uint8_t *)buf + (offset >> 3);
    return ((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]) << (offset & 7);
}

static inline unsigned int lmbd1(const uint32_t x)
{
    return __builtin_clz(x);
}

#endif

#define get_cabac_inline get_alt1cabac_inline
static av_always_inline int get_alt1cabac_inline(CABACContext * const c, uint8_t * const state){
    unsigned int s = *state;
    unsigned int range = c->codIRange;
    unsigned int offset = c->codIOffset;
    const unsigned int RangeLPS= ff_h264_lps_range[2*(range & 0xC0) + s];
    unsigned int s3 = alt1cabac_cabac_transIdx[s];
    const unsigned int next_bits = bmem_peek4(c->bytestream_start, c->b_offset);
    unsigned int n;

//    printf("%02x -> s=%02x s2=%02x s3=%04x\n", bit, s, s2, s3);

    range -= RangeLPS;
    if (offset >= range) {
        offset -= range;
        range = RangeLPS;
        s3 >>= 8;
    }

    n = lmbd1(range) - 23;
    c->codIRange = range << n;
    c->codIOffset = (offset << n) | ((next_bits << (c->b_offset & 7)) >> (32 - n));
    c->b_offset += n;
    *state = (s3 >> 1) & 0x7f;

    return s3 & 1;
}

#define get_cabac_bypass get_alt1cabac_bypass
static inline int get_alt1cabac_bypass(CABACContext *c){
    c->codIOffset = (c->codIOffset << 1) |
        ((c->bytestream_start[c->b_offset >> 3] >> (~c->b_offset & 7)) & 1);
    ++c->b_offset;
    if (c->codIOffset < c->codIRange) {
//        printf("bypass 0: o=%u, r=%u\n", c->codIOffset, c->codIRange);
        return 0;
    }
    c->codIOffset -= c->codIRange;
//    printf("bypass 1: o=%u, r=%u\n", c->codIOffset, c->codIRange);
    return 1;
}

#define get_cabac_bypeek22 get_alt1cabac_bypeek22
static inline uint32_t get_alt1cabac_bypeek22(CABACContext * c, uint32_t * pX)
{
    const uint32_t nb = bmem_peek4(c->bytestream_start, c->b_offset) << (c->b_offset & 7);
    uint32_t x = (c->codIOffset << 23) | ((nb >> 9) & ~1U);
    const uint32_t y = (alt1cabac_inv_range - 256)[c->codIRange];
    *pX = x;

    if (c->codIRange != 256) {
        x = (uint32_t)(((uint64_t)x * (uint64_t)y) >> 32);
    }
    return x << 1;
}

#define get_cabac_byflush get_alt1cabac_byflush
static inline void get_alt1cabac_byflush(CABACContext * c, const unsigned int n, const uint32_t val, const uint32_t x)
{
    c->b_offset += n;
    c->codIOffset = (x >> (23 - n)) - (val >> (32 - n)) * c->codIRange;
}

#define get_cabac_bypass_sign get_alt1cabac_bypass_sign
static av_always_inline int get_alt1cabac_bypass_sign(CABACContext *c, int val){
    return get_cabac_bypass(c) ? val : -val;
}

#define get_cabac_terminate get_alt1cabac_terminate
static int av_unused get_alt1cabac_terminate(CABACContext *c){
    c->codIRange -= 2;
    if (c->codIOffset >= c->codIRange) {
        return (c->b_offset + 7) >> 3;
    }

    // renorm
    {
        int n = (int)lmbd1(c->codIRange) - 23;
        if (n > 0)
        {
            const unsigned int next_bits = bmem_peek4(c->bytestream_start, c->b_offset);
            c->codIRange <<= n;
            c->codIOffset = (c->codIOffset << n) | ((next_bits << (c->b_offset & 7)) >> (32 - n));
            c->b_offset += n;
        }
    }
    return 0;
}

#define skip_bytes alt1cabac_skip_bytes
static av_unused const uint8_t* alt1cabac_skip_bytes(CABACContext *c, int n) {
    const uint8_t *ptr = c->bytestream_start + ((c->b_offset + 7) >> 3);

    if ((int) (c->bytestream_end - ptr) < n)
        return NULL;
    ff_init_cabac_decoder(c, ptr + n, c->bytestream_end - ptr - n);

    return ptr;
}


#endif

