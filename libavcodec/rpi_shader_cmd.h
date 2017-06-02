#ifndef RPI_SHADER_CMD_H
#define RPI_SHADER_CMD_H

#pragma pack(push, 4)

typedef struct qpu_mc_src_s
{
    int16_t y;
    int16_t x;
    uint32_t base;
} qpu_mc_src_t;


typedef struct qpu_mc_pred_c_p_s {
    qpu_mc_src_t next_src;
    uint16_t h;
    uint16_t w;
    uint32_t coeffs_x;
    uint32_t coeffs_y;
    uint32_t wo_u;
    uint32_t wo_v;
    uint32_t dst_addr_c;
    uint32_t next_fn;
} qpu_mc_pred_c_p_t;

typedef struct qpu_mc_pred_c_b_s {
    qpu_mc_src_t next_src1;
    uint16_t h;
    uint16_t w;
    uint32_t coeffs_x1;
    uint32_t coeffs_y1;
    uint32_t weight_u1;
    uint32_t weight_v1;
    qpu_mc_src_t next_src2;
    uint32_t coeffs_x2;
    uint32_t coeffs_y2;
    uint32_t wo_u2;
    uint32_t wo_v2;
    uint32_t dst_addr_c;
    uint32_t next_fn;
} qpu_mc_pred_c_b_t;

typedef struct qpu_mc_pred_c_s_s {
    qpu_mc_src_t next_src1;
    uint32_t pic_cw;            // C Width (== Y width / 2)
    uint32_t pic_ch;            // C Height (== Y Height / 2)
    uint32_t stride2;
    uint32_t stride1;
    uint32_t wdenom;
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
    uint32_t dst_addr;
    uint32_t next_fn;
} qpu_mc_pred_y_p_t;

typedef struct qpu_mc_pred_y_p00_s {
    qpu_mc_src_t next_src1;
    uint16_t h;
    uint16_t w;
    uint32_t wo1;
    uint32_t dst_addr;
    uint32_t next_fn;
} qpu_mc_pred_y_p00_t;

typedef struct qpu_mc_pred_y_s_s {
    qpu_mc_src_t next_src1;
    qpu_mc_src_t next_src2;
    uint16_t pic_h;
    uint16_t pic_w;
    uint32_t stride2;
    uint32_t stride1;
    uint32_t wdenom;
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

#pragma pack(pop)

#endif

