/*
Copyright (c) 2017 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef RPI_SHADER_CMD_H
#define RPI_SHADER_CMD_H

#pragma pack(push, 4)

#if RPI_QPU_EMU_C && RPI_QPU_EMU_Y
// If mixed then we are just confused and get a lot of warnings....
typedef const uint8_t * qpu_mc_src_addr_t;
typedef uint8_t * qpu_mc_dst_addr_t;
#else
typedef uint32_t qpu_mc_src_addr_t;
typedef uint32_t qpu_mc_dst_addr_t;
#endif

typedef struct qpu_mc_src_s
{
    int16_t y;
    int16_t x;
    qpu_mc_src_addr_t base;
} qpu_mc_src_t;


typedef struct qpu_mc_pred_c_p_s {
    qpu_mc_src_t next_src;
    uint16_t h;
    uint16_t w;
    uint32_t coeffs_x;
    uint32_t coeffs_y;
    uint32_t wo_u;
    uint32_t wo_v;
    qpu_mc_dst_addr_t dst_addr_c;
    uint32_t next_fn;
} qpu_mc_pred_c_p_t;

typedef struct qpu_mc_pred_c_b_s {
    qpu_mc_src_t next_src1;
    uint16_t h;
    uint16_t w;
    uint32_t coeffs_x1;
    uint32_t coeffs_y1;
    int16_t weight_u1;
    int16_t weight_v1;
    qpu_mc_src_t next_src2;
    uint32_t coeffs_x2;
    uint32_t coeffs_y2;
    uint32_t wo_u2;
    uint32_t wo_v2;
    qpu_mc_dst_addr_t dst_addr_c;
    uint32_t next_fn;
} qpu_mc_pred_c_b_t;

typedef struct qpu_mc_pred_c_s_s {
    qpu_mc_src_t next_src1;
    uint32_t pic_cw;            // C Width (== Y width / 2)
    uint32_t pic_ch;            // C Height (== Y Height / 2)
    uint32_t stride2;
    uint32_t stride1;
    qpu_mc_src_t next_src2;
    uint32_t next_fn;
} qpu_mc_pred_c_s_t;

typedef struct qpu_mc_pred_c_s {
    union {
        qpu_mc_pred_c_p_t p;
        qpu_mc_pred_c_b_t b;
        qpu_mc_pred_c_s_t s;
    };
} qpu_mc_pred_c_t;


typedef struct qpu_mc_pred_y_p_s {
    qpu_mc_src_t next_src1;
    qpu_mc_src_t next_src2;
    uint16_t h;
    uint16_t w;
    uint32_t mymx21;
    uint32_t wo1;
    uint32_t wo2;
    qpu_mc_dst_addr_t dst_addr;
    uint32_t next_fn;
} qpu_mc_pred_y_p_t;

typedef struct qpu_mc_pred_y_p00_s {
    qpu_mc_src_t next_src1;
    uint16_t h;
    uint16_t w;
    uint32_t wo1;
    qpu_mc_dst_addr_t dst_addr;
    uint32_t next_fn;
} qpu_mc_pred_y_p00_t;

typedef struct qpu_mc_pred_y_s_s {
    qpu_mc_src_t next_src1;
    qpu_mc_src_t next_src2;
    uint16_t pic_h;
    uint16_t pic_w;
    uint32_t stride2;
    uint32_t stride1;
    uint32_t next_fn;
} qpu_mc_pred_y_s_t;

typedef struct qpu_mc_pred_sync_s {
    uint32_t next_fn;
} qpu_mc_pred_sync_t;

// Only a useful structure in that it allows us to return something other than a void *
typedef struct qpu_mc_pred_y_s {
    union {
        qpu_mc_pred_y_p_t p;
        qpu_mc_pred_y_p00_t p00;
        qpu_mc_pred_y_s_t s;
    };
} qpu_mc_pred_y_t;

typedef union qpu_mc_pred_cmd_u {
    qpu_mc_pred_y_t y;
    qpu_mc_pred_c_t c;
    qpu_mc_pred_sync_t sync;
} qpu_mc_pred_cmd_t;

static void inline qpu_mc_link_set(qpu_mc_pred_cmd_t * const cmd, const uint32_t fn)
{
    // Link is last el of previous cmd
    ((uint32_t *)cmd)[-1] = fn;
}

#define QPU_MC_PRED_N_Y8        12
#define QPU_MC_PRED_N_C8        12

#define QPU_MC_PRED_N_Y10       12
#define QPU_MC_PRED_N_C10       12

#define QPU_MC_DENOM            7

#pragma pack(pop)

#endif

