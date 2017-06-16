
struct HEVCContext;
struct HEVCRpiInterPredEnv;

void rpi_shader_c(struct HEVCContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

