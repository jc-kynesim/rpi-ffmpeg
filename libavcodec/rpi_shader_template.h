
struct HEVCContext;
struct HEVCRpiInterPredEnv;

void rpi_shader_c8(struct HEVCContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

void rpi_shader_c16(struct HEVCContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

