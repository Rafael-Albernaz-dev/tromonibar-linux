#include "../src/layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_extend_and_portrait_right(void) {
    DisplayState s = {TV_EXTEND, HP_PORTRAIT_RIGHT};
    char buf[1024];
    assert(build_metamode(&s, false, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-1: 1920x1080_60 @1920x1080 +0+0") != NULL);
    assert(strstr(buf, "ViewPortOut=1824x1026+48+27") != NULL);
    assert(strstr(buf, "DPY-2: 1920x1080_165 @1920x1080 +1920+0") != NULL);
    assert(strstr(buf, "DPY-0: nvidia-auto-select @900x1440 +3840+0") != NULL);
    assert(strstr(buf, "Rotation=270") != NULL);

    DisplayState parsed;
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_EXTEND);
    assert(parsed.hp == HP_PORTRAIT_RIGHT);
}

static void test_mirror_and_landscape(void) {
    DisplayState s = {TV_MIRROR, HP_LANDSCAPE};
    char buf[1024];
    assert(build_metamode(&s, false, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-1: 1920x1080_60 @1920x1080 +0+0") != NULL);
    assert(strstr(buf, "DPY-2: 1920x1080_165 @1920x1080 +0+0") != NULL);
    assert(strstr(buf, "DPY-0: nvidia-auto-select @1440x900 +1920+0") != NULL);

    DisplayState parsed;
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_MIRROR);
    assert(parsed.hp == HP_LANDSCAPE);
}

static void test_tv_720p_preserves_layout(void) {
    DisplayState s = {TV_EXTEND, HP_LANDSCAPE};
    DisplayState parsed;
    char buf[1024];

    assert(build_metamode(&s, true, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-1: 1280x720 @1920x1080 +0+0") != NULL);
    assert(strstr(buf, "ViewPortOut=1216x684+32+18") != NULL);
    assert(strstr(buf, "DPY-2: 1920x1080_165 @1920x1080 +1920+0") != NULL);
    assert(strstr(buf, "DPY-0: nvidia-auto-select @1440x900 +3840+0") != NULL);
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_EXTEND);

    s.tv = TV_MIRROR;
    assert(build_metamode(&s, true, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-1: 1280x720 @1920x1080 +0+0") != NULL);
    assert(strstr(buf, "DPY-2: 1920x1080_165 @1920x1080 +0+0") != NULL);
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_MIRROR);
}

static void test_tv_off_and_hp_off(void) {
    DisplayState s = {TV_OFF, HP_OFF};
    char buf[1024];
    assert(build_metamode(&s, false, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-1:") == NULL);
    assert(strstr(buf, "DPY-0:") == NULL);
    assert(strstr(buf, "DPY-2: 1920x1080_165 @1920x1080 +0+0") != NULL);

    DisplayState parsed;
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_OFF);
    assert(parsed.hp == HP_OFF);
}

static void test_portrait_left(void) {
    DisplayState s = {TV_OFF, HP_PORTRAIT_LEFT};
    char buf[1024];
    assert(build_metamode(&s, false, buf, sizeof(buf)) == 0);
    assert(strstr(buf, "DPY-0: nvidia-auto-select @900x1440 +1920+0") != NULL);
    assert(strstr(buf, "Rotation=90") != NULL);

    DisplayState parsed;
    assert(parse_metamode(buf, &parsed) == 0);
    assert(parsed.tv == TV_OFF);
    assert(parsed.hp == HP_PORTRAIT_LEFT);
}

static void test_null_outputs_do_not_inherit_other_origins(void) {
    DisplayState parsed;
    assert(parse_metamode("DPY-1: NULL, DPY-0: NULL, DPY-2: 1920x1080_165 @1920x1080 +0+0 {}", &parsed) == 0);
    assert(parsed.tv == TV_OFF);
    assert(parsed.hp == HP_OFF);
}

int main(void) {
    test_null_outputs_do_not_inherit_other_origins();
    test_extend_and_portrait_right();
    test_mirror_and_landscape();
    test_tv_720p_preserves_layout();
    test_tv_off_and_hp_off();
    test_portrait_left();
    printf("All layout tests passed.\n");
    return 0;
}
