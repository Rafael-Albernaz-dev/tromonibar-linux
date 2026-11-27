#ifndef TROMONIBAR_LAYOUT_H
#define TROMONIBAR_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TV_EXTEND = 0,
    TV_MIRROR = 1,
    TV_OFF = 2
} TvMode;

typedef enum {
    HP_PORTRAIT_RIGHT = 0,
    HP_PORTRAIT_LEFT = 1,
    HP_LANDSCAPE = 2,
    HP_OFF = 3
} HpMode;

typedef struct {
    TvMode tv;
    HpMode hp;
} DisplayState;

const char *tv_mode_to_str(TvMode mode);
const char *hp_mode_to_str(HpMode mode);
int tv_mode_from_str(const char *s, TvMode *out);
int hp_mode_from_str(const char *s, HpMode *out);

/**
 * Formats an NVIDIA CurrentMetaMode string into `buf` for the given state.
 * Returns 0 on success, -1 if buffer is too small.
 */
int build_metamode(const DisplayState *state, bool tv_720p, char *buf, size_t buf_size);

/**
 * Parses `nvidia-settings -q CurrentMetaMode` output into `out`.
 * Returns 0 on success, -1 if DPY-2 (main monitor) was not found.
 */
int parse_metamode(const char *raw, DisplayState *out);

#endif
