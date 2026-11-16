/* Test-only independent oracle. TRACEGPU_V35_WAVE_SOURCE is the exact,
 * SHA256-recorded protected v3.5 source, never the candidate implementation. */
#include "wave_render.h"
#ifndef TRACEGPU_V35_WAVE_SOURCE
#define TRACEGPU_V35_WAVE_SOURCE "../../../v3.5/software_v3_5/src/wave_render.c"
#endif
#define tracegpu_wave_build tracegpu_v35_reference_build
#define tracegpu_wave_cache_enable tracegpu_v35_reference_cache_enable
#define tracegpu_wave_cache_hits tracegpu_v35_reference_cache_hits
#define tracegpu_wave_cache_misses tracegpu_v35_reference_cache_misses
#define tracegpu_wave_cpu tracegpu_v35_reference_cpu
#define tracegpu_wave_cpu_range tracegpu_v35_reference_cpu_range
#define tracegpu_render_rect_valid tracegpu_v35_reference_rect_valid
#include TRACEGPU_V35_WAVE_SOURCE
