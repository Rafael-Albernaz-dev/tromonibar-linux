#include "layout.h"
#include <stdio.h>
#include <string.h>

const char *tv_mode_to_str(TvMode mode) {
    switch (mode) {
    case TV_EXTEND:
        return "extend";
    case TV_MIRROR:
        return "mirror";
    case TV_OFF:
        return "off";
    }
    return "extend";
}

const char *hp_mode_to_str(HpMode mode) {
    switch (mode) {
    case HP_PORTRAIT_RIGHT:
        return "portrait-right";
    case HP_PORTRAIT_LEFT:
        return "portrait-left";
    case HP_LANDSCAPE:
        return "landscape";
    case HP_OFF:
        return "off";
    }
    return "portrait-right";
}

int tv_mode_from_str(const char *s, TvMode *out) {
    if (!s || !out) {
        return -1;
    }
    if (strcmp(s, "extend") == 0) {
        *out = TV_EXTEND;
        return 0;
    }
    if (strcmp(s, "mirror") == 0) {
        *out = TV_MIRROR;
        return 0;
    }
    if (strcmp(s, "off") == 0) {
        *out = TV_OFF;
        return 0;
    }
    return -1;
}

int hp_mode_from_str(const char *s, HpMode *out) {
    if (!s || !out) {
        return -1;
    }
    if (strcmp(s, "portrait-right") == 0) {
        *out = HP_PORTRAIT_RIGHT;
        return 0;
    }
    if (strcmp(s, "portrait-left") == 0) {
        *out = HP_PORTRAIT_LEFT;
        return 0;
    }
    if (strcmp(s, "landscape") == 0) {
        *out = HP_LANDSCAPE;
        return 0;
    }
    if (strcmp(s, "off") == 0) {
        *out = HP_OFF;
        return 0;
    }
    return -1;
}

int build_metamode(const DisplayState *state, char *buf, size_t buf_size) {
    if (!state || !buf || buf_size == 0) {
        return -1;
    }

    int main_x = (state->tv == TV_EXTEND) ? 1920 : 0;
    int hp_x = main_x + 1920;

    char hp_part[256] = "";
    if (state->hp == HP_PORTRAIT_RIGHT) {
        snprintf(hp_part, sizeof(hp_part),
                 "DPY-0: nvidia-auto-select @900x1440 +%d+0 "
                 "{ViewPortIn=900x1440, ViewPortOut=1440x900+0+0, Rotation=270}",
                 hp_x);
    } else if (state->hp == HP_PORTRAIT_LEFT) {
        snprintf(hp_part, sizeof(hp_part),
                 "DPY-0: nvidia-auto-select @900x1440 +%d+0 "
                 "{ViewPortIn=900x1440, ViewPortOut=1440x900+0+0, Rotation=90}",
                 hp_x);
    } else if (state->hp == HP_LANDSCAPE) {
        snprintf(hp_part, sizeof(hp_part),
                 "DPY-0: nvidia-auto-select @1440x900 +%d+0 "
                 "{ViewPortIn=1440x900, ViewPortOut=1440x900+0+0}",
                 hp_x);
    }

    char tv_part[256] = "";
    if (state->tv == TV_EXTEND || state->tv == TV_MIRROR) {
        snprintf(tv_part, sizeof(tv_part),
                 "DPY-1: 1920x1080_60 @1920x1080 +0+0 "
                 "{ViewPortIn=1920x1080, ViewPortOut=1824x1026+48+27}");
    }

    int written = snprintf(
        buf, buf_size,
        "DPY-2: 1920x1080_165 @1920x1080 +%d+0 "
        "{ViewPortIn=1920x1080, ViewPortOut=1920x1080+0+0}%s%s%s%s",
        main_x,
        (tv_part[0] != '\0') ? ", " : "",
        tv_part,
        (hp_part[0] != '\0') ? ", " : "",
        hp_part);

    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    return 0;
}

/* Extracts the "+X+Y" origin token for a given "DPY-N:" section. */
static int extract_dpy_origin(const char *raw, const char *dpy_prefix, int *x_out) {
    const char *p = strstr(raw, dpy_prefix);
    if (!p) {
        return -1;
    }
    const char *at = strchr(p, '@');
    const char *brace = strchr(p, '{');
    if (!at || (brace && at > brace)) {
        return -1;
    }
    const char *plus = strchr(at, '+');
    if (!plus || (brace && plus > brace)) {
        return -1;
    }
    int x = 0;
    if (sscanf(plus, "+%d+", &x) != 1) {
        return -1;
    }
    *x_out = x;
    return 0;
}

int parse_metamode(const char *raw, DisplayState *out) {
    if (!raw || !out) {
        return -1;
    }

    int main_x = 0;
    if (extract_dpy_origin(raw, "DPY-2:", &main_x) != 0) {
        return -1;
    }

    int tv_x = 0;
    if (extract_dpy_origin(raw, "DPY-1:", &tv_x) == 0) {
        out->tv = (tv_x == main_x) ? TV_MIRROR : TV_EXTEND;
    } else {
        out->tv = TV_OFF;
    }

    const char *dpy0 = strstr(raw, "DPY-0:");
    if (!dpy0) {
        out->hp = HP_OFF;
    } else {
        const char *brace_end = strchr(dpy0, '}');
        size_t len = brace_end ? (size_t)(brace_end - dpy0) : strlen(dpy0);
        char section[512];
        if (len >= sizeof(section)) {
            len = sizeof(section) - 1;
        }
        memcpy(section, dpy0, len);
        section[len] = '\0';

        if (strstr(section, "Rotation=270")) {
            out->hp = HP_PORTRAIT_RIGHT;
        } else if (strstr(section, "Rotation=90")) {
            out->hp = HP_PORTRAIT_LEFT;
        } else {
            out->hp = HP_LANDSCAPE;
        }
    }

    return 0;
}
