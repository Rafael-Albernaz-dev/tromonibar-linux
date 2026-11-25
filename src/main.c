#define _POSIX_C_SOURCE 200809L
#include "layout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define STATE_DIR_REL ".config/tromonibar"
#define STATE_FILE_REL ".config/tromonibar/state.conf"

static int ensure_state_dir(char *path_buf, size_t path_size) {
    const char *home = getenv("HOME");
    if (!home) {
        return -1;
    }
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/%s", home, STATE_DIR_REL);
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    snprintf(path_buf, path_size, "%s/%s", home, STATE_FILE_REL);
    return 0;
}

static int save_state(const DisplayState *state) {
    char path[512];
    if (ensure_state_dir(path, sizeof(path)) != 0) {
        return -1;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "tv=%s\nhp=%s\n", tv_mode_to_str(state->tv), hp_mode_to_str(state->hp));
    fclose(f);
    return 0;
}

static void load_saved_state(DisplayState *state) {
    state->tv = TV_EXTEND;
    state->hp = HP_PORTRAIT_RIGHT;

    const char *home = getenv("HOME");
    if (!home) {
        return;
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", home, STATE_FILE_REL);
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "tv=", 3) == 0) {
            tv_mode_from_str(line + 3, &state->tv);
        } else if (strncmp(line, "hp=", 3) == 0) {
            hp_mode_from_str(line + 3, &state->hp);
        }
    }
    fclose(f);
}

static int query_live_state(DisplayState *state) {
    FILE *pipe = popen("nvidia-settings -q CurrentMetaMode 2>/dev/null", "r");
    if (!pipe) {
        return -1;
    }
    char buf[4096];
    size_t total = 0;
    size_t n;
    while (total < sizeof(buf) - 1 &&
           (n = fread(buf + total, 1, sizeof(buf) - 1 - total, pipe)) > 0) {
        total += n;
    }
    buf[total] = '\0';
    int rc = pclose(pipe);
    if (rc != 0) {
        return -1;
    }
    return parse_metamode(buf, state);
}

static int is_output_connected(const char *output_name) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "xrandr --query 2>/dev/null | grep -q \"^%s connected\"",
             output_name);
    return system(cmd) == 0;
}

static int apply_state(const DisplayState *desired, int persist) {
    DisplayState effective = *desired;

    /* Avoid failing CurrentMetaMode if a secondary monitor is physically off/unplugged. */
    if (effective.tv != TV_OFF && !is_output_connected("HDMI-0")) {
        effective.tv = TV_OFF;
    }
    if (effective.hp != HP_OFF && !is_output_connected("DVI-D-0")) {
        effective.hp = HP_OFF;
    }

    char metamode[1024];
    if (build_metamode(&effective, metamode, sizeof(metamode)) != 0) {
        fprintf(stderr, "tromonibar: failed to build MetaMode\n");
        return 1;
    }

    char cmd[1280];
    snprintf(cmd, sizeof(cmd),
             "nvidia-settings --assign \"CurrentMetaMode=%s\" >/dev/null", metamode);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "tromonibar: nvidia-settings failed (exit %d)\n", rc);
        return 1;
    }

    if (persist) {
        save_state(desired);
    }
    return 0;
}

static void print_usage(void) {
    printf("Usage:\n"
           "  tromonibar status [--json]\n"
           "  tromonibar tv <extend|mirror|off>\n"
           "  tromonibar hp <portrait-right|portrait-left|landscape|off>\n"
           "  tromonibar apply\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        DisplayState state;
        if (query_live_state(&state) != 0) {
            load_saved_state(&state);
        }
        if (argc >= 3 && strcmp(argv[2], "--json") == 0) {
            printf("{\"tv\":\"%s\",\"hp\":\"%s\"}\n",
                   tv_mode_to_str(state.tv), hp_mode_to_str(state.hp));
        } else {
            printf("tv=%s hp=%s\n",
                   tv_mode_to_str(state.tv), hp_mode_to_str(state.hp));
        }
        return 0;
    }

    if (strcmp(argv[1], "tv") == 0 && argc >= 3) {
        TvMode new_tv;
        if (tv_mode_from_str(argv[2], &new_tv) != 0) {
            fprintf(stderr, "Unknown tv mode: %s\n", argv[2]);
            return 1;
        }
        DisplayState state;
        if (query_live_state(&state) != 0) {
            load_saved_state(&state);
        }
        state.tv = new_tv;
        return apply_state(&state, 1);
    }

    if (strcmp(argv[1], "hp") == 0 && argc >= 3) {
        HpMode new_hp;
        if (hp_mode_from_str(argv[2], &new_hp) != 0) {
            fprintf(stderr, "Unknown hp mode: %s\n", argv[2]);
            return 1;
        }
        DisplayState state;
        if (query_live_state(&state) != 0) {
            load_saved_state(&state);
        }
        state.hp = new_hp;
        return apply_state(&state, 1);
    }

    if (strcmp(argv[1], "apply") == 0) {
        const char *session = getenv("XDG_SESSION_TYPE");
        if (session && strcmp(session, "x11") != 0) {
            return 0;
        }
        DisplayState state;
        load_saved_state(&state);
        return apply_state(&state, 0);
    }

    print_usage();
    return 1;
}
