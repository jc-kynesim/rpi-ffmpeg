#ifndef LIBAVCODEC_RPI_SHADER_TEMPLATE_H
#define LIBAVCODEC_RPI_SHADER_TEMPLATE_H

struct HEVCRpiContext;
struct HEVCRpiInterPredEnv;

void ff_hevc_rpi_shader_c8(struct HEVCRpiContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

void ff_hevc_rpi_shader_c16(struct HEVCRpiContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

void rpi_sand_dump8(const char * const name,
                    const uint8_t * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c);

void rpi_sand_dump16(const char * const name,
                     const uint8_t * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c);

#endif

