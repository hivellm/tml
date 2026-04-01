/**
 * @file essential_tracy.c
 * @brief TML Runtime - Tracy Profiler Integration (TML FFI bindings)
 *
 * Extracted from essential.c. All statics remain static (translation-unit scope).
 */

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Tracy Profiler Integration (TML FFI bindings)
// ============================================================================

#ifdef TML_PROFILE
#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif
#include "tracy/TracyC.h"

// Zone stack for TML — maps zone IDs to TracyCZoneCtx
#define MAX_TML_ZONES 256
static TracyCZoneCtx tml_zone_stack[MAX_TML_ZONES];
static int tml_zone_top = 0;

int64_t tml_tracy_zone_begin(const char* name) {
    if (tml_zone_top >= MAX_TML_ZONES)
        return -1;
    TracyCZone(ctx, 1);
    TracyCZoneName(ctx, name, strlen(name));
    int id = tml_zone_top;
    tml_zone_stack[tml_zone_top++] = ctx;
    return (int64_t)id;
}

void tml_tracy_zone_end(int64_t zone_id) {
    if (zone_id < 0 || zone_id >= tml_zone_top)
        return;
    TracyCZoneEnd(tml_zone_stack[zone_id]);
    if (zone_id == tml_zone_top - 1)
        tml_zone_top--;
}

void tml_tracy_message(const char* text) {
    TracyCMessage(text, strlen(text));
}

void tml_tracy_plot(const char* name, int64_t value) {
    TracyCPlotI(name, value);
}

void tml_tracy_frame_mark(void) {
    TracyCFrameMark;
}

#else // No profiling — stubs

int64_t tml_tracy_zone_begin(const char* name) {
    (void)name;
    return 0;
}
void tml_tracy_zone_end(int64_t zone_id) {
    (void)zone_id;
}
void tml_tracy_message(const char* text) {
    (void)text;
}
void tml_tracy_plot(const char* name, int64_t value) {
    (void)name;
    (void)value;
}
void tml_tracy_frame_mark(void) {}

#endif // TML_PROFILE
