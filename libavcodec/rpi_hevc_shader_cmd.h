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
    uint32_t data[1];
} qpu_mc_pred_cmd_t;

#define QPU_MC_PRED_N_Y8        12
#define QPU_MC_PRED_N_C8        12

#define QPU_MC_PRED_N_Y10       12
#define QPU_MC_PRED_N_C10       12

#define QPU_MC_DENOM            7

#pragma pack(pop)

#endif

