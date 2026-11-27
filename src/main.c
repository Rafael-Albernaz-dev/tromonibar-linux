#define _POSIX_C_SOURCE 200809L
#include "layout.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
    char temp[sizeof(path) + sizeof(".XXXXXX")];
    snprintf(temp, sizeof(temp), "%s.XXXXXX", path);
    int fd = mkstemp(temp);
    if (fd < 0) {
        return -1;
    }
    FILE *f = fdopen(fd, "w");
    int result = -1;
    if (f) {
        int written = fprintf(f, "tv=%s\nhp=%s\n", tv_mode_to_str(state->tv), hp_mode_to_str(state->hp));
        if (fclose(f) == 0 && written >= 0) {
            result = rename(temp, path);
        }
    } else {
        close(fd);
    }
    if (result != 0) {
        unlink(temp);
    }
    return result;
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

typedef struct {
    bool connected;
    bool active;
    bool has_1080p;
    bool has_720p;
} TvOutput;

static int query_tv_output(TvOutput *output) {
    FILE *pipe = popen("xrandr --query", "r");
    if (!pipe) {
        return -1;
    }

    *output = (TvOutput){0};
    bool in_hdmi = false;
    bool found_hdmi = false;
    char line[512];
    while (fgets(line, sizeof(line), pipe)) {
        if (line[0] != ' ' && line[0] != '\t') {
            char name[32], status[32];
            bool hdmi = sscanf(line, "%31s %31s", name, status) == 2 &&
                        strcmp(name, "HDMI-0") == 0;
            in_hdmi = hdmi && strcmp(status, "connected") == 0;
            found_hdmi |= in_hdmi || (hdmi && strcmp(status, "disconnected") == 0);
            output->connected |= in_hdmi;
        } else if (in_hdmi) {
            char mode[32];
            if (sscanf(line, "%31s", mode) == 1) {
                output->active |= strchr(line, '*') != NULL;
                output->has_1080p |= strcmp(mode, "1920x1080") == 0;
                output->has_720p |= strcmp(mode, "1280x720") == 0;
            }
        }
    }
    return pclose(pipe) == 0 && found_hdmi ? 0 : -1;
}

static int assign_metamode(const DisplayState *state, bool tv_720p) {
    char metamode[1024];
    if (build_metamode(state, tv_720p, metamode, sizeof(metamode)) != 0) {
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

    DisplayState actual;
    if (query_live_state(&actual) != 0 || actual.tv != state->tv || actual.hp != state->hp) {
        return 1;
    }
    TvOutput output;
    if (query_tv_output(&output) != 0 || output.active != (state->tv != TV_OFF)) {
        return 1;
    }
    return 0;
}

static int apply_state(const DisplayState *desired, const DisplayState *to_save, bool explicit_tv) {
    DisplayState effective = *desired;
    TvOutput output = {0};

    if (effective.tv != TV_OFF) {
        if (query_tv_output(&output) != 0) {
            fprintf(stderr, "Não foi possível consultar a saída HDMI-0.\n");
            return 1;
        }
        if (!output.connected) {
            if (explicit_tv) {
                fprintf(stderr, "TV não detectada em HDMI-0. Confira se a TV está ligada, na entrada HDMI correta e com o cabo encaixado.\n");
                return 1;
            }
            effective.tv = TV_OFF;
        }
    }
    if (effective.tv != TV_OFF && !output.has_1080p && !output.has_720p) {
        fprintf(stderr, "HDMI-0 conectada, mas sem modos 1080p ou 720p anunciados pelo driver.\n");
        return 1;
    }
    if (effective.hp != HP_OFF && !is_output_connected("DVI-D-0")) {
        effective.hp = HP_OFF;
    }

    if (effective.tv == TV_OFF) {
        if (assign_metamode(&effective, false) != 0) {
            fprintf(stderr, "Não foi possível confirmar que a saída HDMI-0 foi desativada.\n");
            return 1;
        }
    } else if ((!output.has_1080p || assign_metamode(&effective, false) != 0) &&
               (!output.has_720p || assign_metamode(&effective, true) != 0)) {
        fprintf(stderr, "Não foi possível ativar o modo da TV em HDMI-0 (1080p ou 720p).\n");
        return 1;
    }

    /* Keep DP-0 as Xinerama primary so GNOME Shell keeps the top bar on it. */
    if (system("nvidia-settings --assign XineramaInfoOrder=DP-0 >/dev/null 2>&1") != 0) {
        fprintf(stderr, "tromonibar: warning: failed to set XineramaInfoOrder\n");
    }

    if (to_save && save_state(to_save) != 0) {
        fprintf(stderr, "Modo aplicado, mas não foi possível gravar o perfil.\n");
        return 1;
    }
    if (!explicit_tv && desired->tv != TV_OFF && effective.tv == TV_OFF) {
        fprintf(stderr, "TV não detectada em HDMI-0. Perfil aplicado aos demais monitores.\n");
        return 2;
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
        TvOutput output;
        if (query_live_state(&state) != 0 || query_tv_output(&output) != 0) {
            fprintf(stderr, "Não foi possível consultar os monitores na sessão Xorg.\n");
            return 1;
        }
        if (!output.active) {
            state.tv = TV_OFF;
        }
        if (argc >= 3 && strcmp(argv[2], "--json") == 0) {
            printf("{\"tv\":\"%s\",\"hp\":\"%s\",\"hdmi_connected\":%s,\"hdmi_active\":%s}\n",
                   tv_mode_to_str(state.tv), hp_mode_to_str(state.hp),
                   output.connected ? "true" : "false", output.active ? "true" : "false");
        } else {
            printf("tv=%s hp=%s hdmi=%s\n", tv_mode_to_str(state.tv), hp_mode_to_str(state.hp),
                   output.active ? "active" : output.connected ? "connected" : "disconnected");
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
        return apply_state(&state, &state, true);
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
        DisplayState saved;
        load_saved_state(&saved);
        state.hp = saved.hp = new_hp;
        return apply_state(&state, &saved, false);
    }

    if (strcmp(argv[1], "apply") == 0) {
        const char *session = getenv("XDG_SESSION_TYPE");
        if (session && strcmp(session, "x11") != 0) {
            return 0;
        }
        DisplayState state;
        load_saved_state(&state);
        return apply_state(&state, NULL, false);
    }

    print_usage();
    return 1;
}
