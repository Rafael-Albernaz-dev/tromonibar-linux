#!/bin/sh
set -eu

binary="$(pwd)/build/tromonibar"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
export HOME="$tmp/home" PATH="$tmp/bin:$PATH" FIXTURE="$tmp" XDG_SESSION_TYPE=x11
mkdir -p "$HOME/.config/tromonibar" "$tmp/bin"
state="$HOME/.config/tromonibar/state.conf"
printf 'tv=extend\nhp=off\n' > "$state"
cp "$state" "$tmp/original"

cat > "$tmp/bin/xrandr" <<'FAKE'
#!/bin/sh
printf 'DVI-D-0 disconnected\n'
cat "$FIXTURE/hdmi"
FAKE
cat > "$tmp/bin/nvidia-settings" <<'FAKE'
#!/bin/sh
case "$1:$2" in
    -q:CurrentMetaMode) cat "$FIXTURE/live" ;;
    --assign:CurrentMetaMode=*)
        printf '%s\n' "$2" >> "$FIXTURE/assignments"
        printf '%s\n' "${2#CurrentMetaMode=}" > "$FIXTURE/live"
        case "$2" in
            *'DPY-1: 1280x720'*)
                printf 'HDMI-0 connected\n   1280x720 60.00*\n' > "$FIXTURE/hdmi"
                ;;
        esac
        ;;
esac
FAKE
chmod +x "$tmp/bin/"*
printf 'DPY-2: 1920x1080_165 @1920x1080 +0+0 {}\n' > "$tmp/live"
printf 'HDMI-0 disconnected\n' > "$tmp/hdmi"

if "$binary" tv mirror 2>"$tmp/error"; then exit 1; fi
grep -q 'TV não detectada' "$tmp/error"
test ! -e "$tmp/assignments"
cmp "$state" "$tmp/original"
"$binary" status --json | grep -q '"tv":"off".*"hdmi_connected":false'
rc=0
"$binary" apply 2>"$tmp/error" || rc=$?
test "$rc" -eq 2
cmp "$state" "$tmp/original"

# An unrelated HP command must not save the temporary disconnected TV state.
"$binary" hp off
cmp "$state" "$tmp/original"

# MetaMode alone claims HDMI is on. Require an active XRandR mode and try 720p.
: > "$tmp/assignments"
printf 'HDMI-0 connected\n   1920x1080 60.00\n   1280x720 60.00\n' > "$tmp/hdmi"
"$binary" tv mirror
test "$(wc -l < "$tmp/assignments")" -eq 2
grep -q 'DPY-1: 1920x1080_60' "$tmp/assignments"
grep -q 'DPY-1: 1280x720' "$tmp/assignments"
grep -qx 'tv=mirror' "$state"
"$binary" status --json | grep -q '"tv":"mirror".*"hdmi_active":true'

# A successful assignment with no HDMI scanout must leave the saved profile intact.
cp "$state" "$tmp/original"
printf 'HDMI-0 connected\n   1920x1080 60.00\n' > "$tmp/hdmi"
if "$binary" tv extend 2>"$tmp/error"; then exit 1; fi
cmp "$state" "$tmp/original"
"$binary" status --json | grep -q '"tv":"off".*"hdmi_connected":true.*"hdmi_active":false'

printf 'HDMI-0 connected\n   800x600 60.00\n' > "$tmp/hdmi"
: > "$tmp/assignments"
if "$binary" tv extend 2>"$tmp/error"; then exit 1; fi
grep -q 'sem modos' "$tmp/error"
test ! -s "$tmp/assignments"
printf 'HDMI CLI regression tests passed.\n'
