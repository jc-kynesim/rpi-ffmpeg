#ifndef RPI_SHADER_CMD_H
#define RPI_SHADER_CMD_H

#pragma pack(push, 4)

typedef struct qpu_mc_pred_c_s {
    uint32_t next_fn;
    int16_t next_src_y;
    int16_t next_src_x;
    uint32_t next_src_base_c;
    union {
        struct {
            uint16_t h;
            uint16_t w;
            uint32_t coeffs_x;
            uint32_t coeffs_y;
            uint32_t wo_u;
            uint32_t wo_v;
            uint32_t dst_addr_c;
        } p;
        struct {
            uint16_t h;
            uint16_t w;
            uint32_t coeffs_x;
            uint32_t coeffs_y;
            uint32_t weight_u;
            uint32_t weight_v;
            uint32_t dummy0;
        } b0;
        struct {
            uint32_t dummy0;
            uint32_t coeffs_x;
            uint32_t coeffs_y;
            uint32_t wo_u;
            uint32_t wo_v;
            uint32_t dst_addr_c;
        } b1;
        struct {
            uint32_t pic_cw;            // C Width (== Y width / 2)
            uint32_t pic_ch;            // C Height (== Y Height / 2)
            uint32_t stride2;
            uint32_t stride1;
            uint32_t wdenom;
            uint32_t dummy0;
        } s0;
        struct {
            uint32_t dummy0;
            uint32_t dummy1;
            uint32_t dummy2;
            uint32_t dummy3;
            uint32_t dummy4;
            uint32_t dummy5;
        } s1;
    };
} qpu_mc_pred_c_t;

typedef struct qpu_mc_pred_y_s {
    int16_t next_src1_x;
    int16_t next_src1_y;
    uint32_t next_src1_base;
    int16_t next_src2_x;
    int16_t next_src2_y;
    uint32_t next_src2_base;
    union {
        struct {
            uint16_t h;
            uint16_t w;
            uint32_t mymx21;
            uint32_t wo1;
            uint32_t wo2;
            uint32_t dst_addr;
        } p;
        struct {
            uint16_t pic_h;
            uint16_t pic_w;
            uint32_t stride2;
            uint32_t stride1;
            uint32_t wdenom;
            uint32_t dummy0;
        } s;
    };
    uint32_t next_fn;
} qpu_mc_pred_y_t;

#pragma pack(pop)

#endif

