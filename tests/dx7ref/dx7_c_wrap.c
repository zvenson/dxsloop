/* the C port, at Dexed's block size, for tests/dx7_exact_test.cc */
#include <stdint.h>
#define DX_LG_N 6
#include "../../firmware/src/dx7_core.c"
void *cport_new(void) { static dxv_t v[4]; static int k; return &v[k++ & 3]; }
void cport_init(void *v, const uint8_t *p, int note, int vel) { dx_init((dxv_t *)v, p, note, vel); }
void cport_compute(void *v, int32_t *out, int32_t lv, int32_t ld) { dx_compute_ext((dxv_t *)v, out, lv, ld, 0); }
void cport_keyup(void *v) { dx_keyup((dxv_t *)v); }
int cport_playing(void *v) { return dx_playing((dxv_t *)v); }
void cport_lfo(void *v, int32_t *lv, int32_t *ld) { *lv = dx_lfo_sample(&((dxv_t *)v)->lfo); *ld = dx_lfo_delay(&((dxv_t *)v)->lfo); }
