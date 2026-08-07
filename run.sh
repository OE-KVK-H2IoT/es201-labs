#!/usr/bin/env bash
# es201 lab runner — build, flash (SWD via the Debug Probe), and monitor demos.
# Each lab is a standalone Pico project (its own CMakeLists + build/); this just finds
# the lab folder that owns a target and drives cmake there.
#
#   ./run.sh list                list buildable lab targets
#   ./run.sh doctor              check the toolchain/env for every lab step
#   ./run.sh build [target]      build one target (or build-all if omitted)
#   ./run.sh build-all           configure + build every lab (compile check)
#   ./run.sh flash <target>      build that target and flash it over SWD
#   ./run.sh load <file.elf>     flash a prebuilt ELF/HEX over SWD, then monitor
#   ./run.sh debug <target>      build, flash, and drop into GDB stopped at main()
#   ./run.sh monitor [device]    open the USB-serial console (default: auto-detect)
#   ./run.sh <target>            shorthand: flash <target>, then monitor
#   ./run.sh flash l4_optimize START_MODE=1 LCD_MHZ=75   override compile-time #defines
#
# Trailing KEY=VAL args become -DKEY=VAL build defines for that lab (e.g. line above).
#
# Environment variables:
#   PICO_SDK_PATH         REQUIRED to build. Path to your Pico SDK checkout, e.g.
#                           export PICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.1.1"  # VS Code ext's SDK
#                           export PICO_SDK_PATH=/path/to/pico-sdk            # a manual clone
#   FREERTOS_KERNEL_PATH  Only for the L5 RTOS lab. Path to a FreeRTOS-Kernel checkout,
#                         or add lib/FreeRTOS-Kernel as a submodule. e.g.
#                           export FREERTOS_KERNEL_PATH=/path/to/FreeRTOS-Kernel
#   PICO_BOARD            Board type (default: pico2_w).
#   SERIAL                Serial device for `monitor` (default: auto-detect probe/USB-CDC).
#   ADAPTER_SPEED         SWD adapter speed in kHz (default: 5000).
# Make any of these permanent by adding the `export ...` line to your ~/.bashrc.
# Also needs a toolchain + RP2350-capable OpenOCD — see the course "Environment Setup".
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ADAPTER_SPEED="${ADAPTER_SPEED:-5000}"
BUILD_DIR=""   # set by build_target() to the lab's build/ dir

# Prefer Ninja when present so CLI builds match the Pico VS Code extension (which ships its
# own Ninja); fall back to CMake's default generator (Make) if ninja isn't installed.
GEN_ARGS=(); command -v ninja >/dev/null 2>&1 && GEN_ARGS=(-G Ninja)

# Find a ttyACM by USB PID: 0009 = Pico's own USB-CDC, 000c = Debug Probe UART bridge.
find_acm_by_pid() {
    local pid="$1" d p
    for d in /dev/ttyACM*; do
        [ -e "$d" ] || continue
        p="$(udevadm info -q property -n "$d" 2>/dev/null)"
        echo "$p" | grep -q 'ID_USB_VENDOR_ID=2e8a' && echo "$p" | grep -q "ID_USB_MODEL_ID=$pid" && { echo "$d"; return 0; }
    done
    return 1
}
OCD_IFACE="interface/cmsis-dap.cfg"
OCD_TARGET="target/rp2350.cfg"

# If OPENOCD_SCRIPTS isn't exported, fall back to the Pi-fork install under ~/.pico-sdk.
ocd_scripts_arg() {
    [ -n "${OPENOCD_SCRIPTS:-}" ] && return
    local d
    d=$(ls -d "$HOME"/.pico-sdk/openocd/*/scripts 2>/dev/null | head -1 || true)
    [ -n "$d" ] && printf -- '-s\n%s\n' "$d"
}

# Find the openocd binary. Prefer one on PATH; otherwise fall back to the Pi-fork
# install the Pico setup drops under ~/.pico-sdk (which isn't added to PATH). This
# is why a bare `openocd` says "command not found" even though flashing works in
# the official VS Code extension — it knows the absolute path; we resolve it here.
openocd_bin() {
    if command -v openocd >/dev/null 2>&1; then echo openocd; return; fi
    local b
    b=$(ls "$HOME"/.pico-sdk/openocd/*/openocd 2>/dev/null | head -1 || true)
    [ -n "$b" ] && { echo "$b"; return; }
    echo "!! openocd not found — not on PATH and no ~/.pico-sdk/openocd/*/openocd." >&2
    echo "   Install it via the Pico setup, or put openocd on PATH." >&2
    exit 1
}

# Same story for the ARM GDB: prefer one on PATH, else the Pico toolchain's.
gdb_bin() {
    if command -v arm-none-eabi-gdb >/dev/null 2>&1; then echo arm-none-eabi-gdb; return; fi
    local b
    b=$(ls "$HOME"/.pico-sdk/toolchain/*/bin/arm-none-eabi-gdb 2>/dev/null | head -1 || true)
    [ -n "$b" ] && { echo "$b"; return; }
    echo "!! arm-none-eabi-gdb not found — not on PATH and no ~/.pico-sdk/toolchain/*/bin/." >&2
    exit 1
}

# Building needs the Pico SDK. Fail early with copy-paste setup rather than letting
# CMake error cryptically deep in pico_sdk_import.cmake.
require_sdk() {
    if [ -z "${PICO_SDK_PATH:-}" ]; then
        echo "!! PICO_SDK_PATH is not set — the Pico SDK is required to build." >&2
        echo "   Set it to your SDK checkout, e.g.:" >&2
        echo "     export PICO_SDK_PATH=\"\$HOME/.pico-sdk/sdk/2.1.1\"   # the VS Code extension's SDK" >&2
        echo "     export PICO_SDK_PATH=/path/to/pico-sdk              # a manual clone" >&2
        echo "   (add that line to ~/.bashrc to make it permanent). See './run.sh --help'." >&2
        exit 1
    fi
    if [ ! -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ]; then
        echo "!! PICO_SDK_PATH=\"$PICO_SDK_PATH\" doesn't look like a Pico SDK" >&2
        echo "   (no pico_sdk_init.cmake there). Point it at your SDK checkout — see './run.sh --help'." >&2
        exit 1
    fi
}

# All lab folders that are standalone Pico projects (have a CMakeLists.txt).
lab_dirs() { for d in "$HERE"/l[0-9]*/; do [ -f "$d/CMakeLists.txt" ] && echo "${d%/}"; done; }

# A target is declared either directly, add_executable(<t> …), or via a lab's local
# helper, add_lN_app(<t> …) (l4/l5 use one to avoid repeating stdio/output lines).
TGT_RE='add_(executable|l[0-9]+_app)\('

# target -> the lab folder whose CMakeLists declares it.
lab_dir_for() {
    grep -lE "${TGT_RE}\s*$1\b" "$HERE"/l[0-9]*/CMakeLists.txt 2>/dev/null | head -1 | xargs -r dirname
}

list_targets() {
    grep -rhoE "${TGT_RE}\s*[a-z0-9_]+" "$HERE"/l[0-9]*/CMakeLists.txt 2>/dev/null \
        | sed -E 's/.*\(\s*//' | sort -u
}

# Recognised build knobs = the names in each lab's `foreach(opt ...)` (forwarded to
# -D defines), plus PICO_BOARD. Used to catch typos in KEY=VAL args.
known_knobs() {
    { printf '%s\n' PICO_BOARD
      grep -rhoE 'foreach\(opt[^)]*\)' "$HERE"/l[0-9]*/CMakeLists.txt 2>/dev/null \
          | sed -E 's/foreach\(opt//; s/\)//'
    } | tr ' ' '\n' | sed '/^$/d' | sort -u
}

# Build one target inside its own lab folder. Sets BUILD_DIR. Uses the global `defs`.
build_target() {
    local t="$1" lab
    require_sdk
    lab="$(lab_dir_for "$t")"
    [ -n "$lab" ] || { echo "!! unknown target '$t' — run './run.sh list'"; exit 1; }
    # (Re)configure when there is no build dir, when defines change, or when a prior
    # configure left an incomplete cache (no CMAKE_PROJECT_NAME) — that stale state is
    # what produces the cryptic "could not find CMAKE_PROJECT_NAME in Cache" on build.
    if [ ! -f "$lab/build/CMakeCache.txt" ] || [ "${#defs[@]}" -gt 0 ] \
       || ! grep -q '^CMAKE_PROJECT_NAME' "$lab/build/CMakeCache.txt" 2>/dev/null; then
        echo ">> configuring $(basename "$lab") (PICO_BOARD=${PICO_BOARD:-pico2_w})"
        rm -rf "$lab/build"   # clean slate: never build on a half-configured cache
        cmake -S "$lab" -B "$lab/build" "${GEN_ARGS[@]}" ${PICO_BOARD:+-DPICO_BOARD=$PICO_BOARD} "${defs[@]}"
    fi
    echo ">> building $t"
    # If the target doesn't exist (e.g. an optional lab whose dependency is missing),
    # cmake --build fails with an opaque message — give a targeted hint for the RTOS lab.
    if ! cmake --build "$lab/build" --target "$t" -j"$(nproc)"; then
        case "$lab" in
            *l5-freertos-tasks) echo "!! '$t' not built — the FreeRTOS kernel is missing. See $lab/README.md (add the submodule or set FREERTOS_KERNEL_PATH), then retry.";;
        esac
        exit 1
    fi
    BUILD_DIR="$lab/build"
}

build_all() {
    local lab
    require_sdk
    for lab in $(lab_dirs); do
        echo ">> === $(basename "$lab") ==="
        cmake -S "$lab" -B "$lab/build" "${GEN_ARGS[@]}" ${PICO_BOARD:+-DPICO_BOARD=$PICO_BOARD} >/dev/null
        cmake --build "$lab/build" -j"$(nproc)"
    done
}

# Program a single image file onto the chip over SWD, then verify, reset and run.
# OpenOCD's `program` autodetects ELF/HEX/BIN — but NOT .uf2 (that's the BOOTSEL
# mass-storage format; flash it by dragging to the RP2350 drive or `picotool load`).
program_image() {
    local img="$1"
    case "$img" in
        *.uf2) echo "!! OpenOCD can't program a .uf2 over SWD — that's the BOOTSEL format." >&2
               echo "   Drag it to the RP2350 drive, or: picotool load \"$img\"." >&2
               echo "   To flash over SWD, pass the matching .elf (or .hex) instead." >&2
               exit 2 ;;
    esac
    [ -f "$img" ] || { echo "!! no such file: $img" >&2; exit 1; }
    echo ">> flashing $img over SWD (power the Pico's own USB too!)"
    local s=(); mapfile -t s < <(ocd_scripts_arg)
    "$(openocd_bin)" "${s[@]}" -f "$OCD_IFACE" -f "$OCD_TARGET" \
        -c "adapter speed $ADAPTER_SPEED" \
        -c "program \"$img\" verify reset exit"
}

flash_target() {
    local target="$1" elf
    build_target "$target"
    elf="$(find "$BUILD_DIR" -name "${target}.elf" -print -quit)"
    [ -n "$elf" ] || { echo "!! no ELF for '$target' — run './run.sh list'"; exit 1; }
    program_image "$elf"
}

# Load a prebuilt image file (ELF/HEX) directly — no build, no course target — then
# open the serial monitor. Handy for flashing an .elf built elsewhere.
load_image() {
    local img="$1"
    [ -n "$img" ] || { echo "usage: ./run.sh load <file.elf>"; exit 2; }
    program_image "$img"
    monitor
}

debug_target() {
    local target="$1" elf
    build_target "$target"
    elf="$(find "$BUILD_DIR" -name "${target}.elf" -print -quit)"
    [ -n "$elf" ] || { echo "!! no ELF for '$target' — run './run.sh list'"; exit 1; }

    # GDB launches OpenOCD itself as a "pipe" subprocess: GDB talks to it over the
    # pipe's stdio instead of a TCP port, so there's no separate server to start or
    # leftover process to kill — quit GDB and OpenOCD dies with it. OpenOCD's own
    # chatter is sent to debug.openocd.log so it doesn't clutter the GDB console.
    local s=(); mapfile -t s < <(ocd_scripts_arg)
    local ocd_cmd="$(openocd_bin) ${s[*]} -f $OCD_IFACE -f $OCD_TARGET \
        -c 'adapter speed $ADAPTER_SPEED' -c 'gdb_port pipe; log_output $BUILD_DIR/debug.openocd.log'"

    echo ">> debugging $elf (power the Pico's own USB too!)"
    echo "   load+halt at main(); 'c' run, 'n' step, 'p var' inspect, 'q' quit."
    # -q quiet banner; load the image, reset, stop the CPU at the start of main().
    "$(gdb_bin)" -q "$elf" \
        -ex "target extended-remote | $ocd_cmd" \
        -ex "monitor reset halt" \
        -ex "load" \
        -ex "monitor reset halt" \
        -ex "break main" \
        -ex "continue"
}

monitor() {
    # Programs print to BOTH the Pico USB-CDC and UART0 (GP0/1 -> Debug Probe bridge).
    # Prefer the probe's UART bridge: it's the reliable path (USB-CDC can be flaky on
    # some RP2350 setups). Override with: SERIAL=/dev/ttyACMx ./run.sh monitor
    local dev="${1:-${SERIAL:-}}"
    [ -z "$dev" ] && dev="$(find_acm_by_pid 000c || true)"   # Debug Probe UART bridge
    [ -z "$dev" ] && dev="$(find_acm_by_pid 0009 || true)"   # Pico USB-CDC
    [ -z "$dev" ] && { echo "!! no Pico/probe serial port found (probe plugged in? UART wired to GP0/GP1?)"; exit 1; }
    echo ">> serial monitor on $dev"
    exec "$HERE/serial-term.sh" "$dev" 115200
}

# doctor — walk every step the lab tutorials rely on and report ok/FAIL/note, each
# with the setup section that fixes it. Ends with a real compile of the smallest
# target, so a clean run proves configure→compile→link actually works.
doctor() {
    local ok=0 bad=0 note=0
    mark() {  # mark <ok|FAIL|note> <label> <detail...>
        local s="$1" label="$2"; shift 2
        case "$s" in ok) ok=$((ok+1));; FAIL) bad=$((bad+1));; note) note=$((note+1));; esac
        printf '  [%-4s] %-18s %s\n' "$s" "$label" "$*"
    }
    echo "== es201 environment doctor — the toolchain every lab step uses =="

    if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        mark ok  "cross gcc" "arm-none-eabi-gcc $(arm-none-eabi-gcc -dumpversion 2>/dev/null)"
    else mark FAIL "cross gcc" "missing — install the ARM cross toolchain (setup §1)"; fi

    command -v cmake >/dev/null 2>&1 \
        && mark ok "cmake" "$(cmake --version | head -1 | awk '{print $3}')" \
        || mark FAIL "cmake" "missing (setup §1)"

    if command -v ninja >/dev/null 2>&1; then mark ok "ninja" "$(ninja --version)"
    elif command -v make >/dev/null 2>&1; then mark note "ninja" "absent — will fall back to make (setup §1)"
    else mark FAIL "ninja/make" "no build tool found (setup §1)"; fi

    command -v git >/dev/null 2>&1 \
        && mark ok "git" "$(git --version | awk '{print $3}')" || mark FAIL "git" "missing (setup §1)"

    # Pico SDK + the two submodules that bite (setup §2 danger box)
    if [ -z "${PICO_SDK_PATH:-}" ]; then
        mark FAIL "PICO_SDK_PATH" "not set — export it (setup §2, or ./run.sh --help)"
    elif [ ! -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ]; then
        mark FAIL "PICO_SDK_PATH" "not an SDK: $PICO_SDK_PATH (setup §2)"
    else
        mark ok "PICO_SDK_PATH" "$PICO_SDK_PATH ($(git -C "$PICO_SDK_PATH" describe --tags 2>/dev/null || echo '?'))"
        local miss=""
        [ -f "$PICO_SDK_PATH/tools/pioasm/CMakeLists.txt" ] || miss="pioasm"
        [ -n "$(ls -A "$PICO_SDK_PATH/lib/tinyusb" 2>/dev/null)" ] || miss="${miss:+$miss, }tinyusb"
        [ -z "$miss" ] && mark ok "SDK submodules" "pioasm + tinyusb present" \
            || mark FAIL "SDK submodules" "missing: $miss — git -C \"\$PICO_SDK_PATH\" submodule update --init (setup §2)"
    fi

    # OpenOCD + a usable rp2350 target config
    local ocd=""
    command -v openocd >/dev/null 2>&1 && ocd=openocd \
        || ocd="$(ls "$HOME"/.pico-sdk/openocd/*/openocd 2>/dev/null | head -1 || true)"
    if [ -n "$ocd" ]; then
        local ver cfg="" sdir
        ver="$("$ocd" --version 2>&1 | grep -oE '0\.[0-9][0-9.]*[^ ]*' | head -1)"
        for sdir in "${OPENOCD_SCRIPTS:-}" "$(dirname "$ocd")/scripts" \
                    /usr/local/share/openocd/scripts /usr/share/openocd/scripts; do
            [ -n "$sdir" ] && [ -f "$sdir/target/rp2350.cfg" ] && { cfg="$sdir"; break; }
        done
        [ -n "$cfg" ] && mark ok "openocd" "$ver (rp2350.cfg found)" \
            || mark FAIL "openocd" "$ver but no target/rp2350.cfg — need the Pi fork / OPENOCD_SCRIPTS (setup §3)"
    else mark FAIL "openocd" "not found — install the Pi fork or add ~/.pico-sdk to PATH (setup §3)"; fi

    if command -v arm-none-eabi-gdb >/dev/null 2>&1; then
        mark ok "gdb" "arm-none-eabi-gdb $(arm-none-eabi-gdb --version | head -1 | grep -oE '[0-9][0-9.]+' | head -1)"
    elif command -v gdb-multiarch >/dev/null 2>&1; then mark ok "gdb" "gdb-multiarch (Debian/Ubuntu)"
    else mark FAIL "gdb" "missing — arm-none-eabi-gdb / gdb-multiarch (setup §1)"; fi

    command -v picotool >/dev/null 2>&1 \
        && mark ok "picotool" "$(picotool version 2>/dev/null | grep -oE '[0-9][0-9.]+' | head -1)" \
        || mark note "picotool" "not found (optional — setup §5)"

    if ls /dev/ttyACM* >/dev/null 2>&1 && { find_acm_by_pid 000c >/dev/null 2>&1 || find_acm_by_pid 0009 >/dev/null 2>&1; }; then
        mark ok "Pico/probe USB" "serial device detected"
    else mark note "Pico/probe USB" "none detected (plug in to flash/debug/monitor)"; fi

    if [ -n "${FREERTOS_KERNEL_PATH:-}" ] && [ -d "$FREERTOS_KERNEL_PATH" ]; then
        mark ok "FreeRTOS kernel" "$FREERTOS_KERNEL_PATH"
    elif [ -f "$HERE/lib/FreeRTOS-Kernel/tasks.c" ]; then mark ok "FreeRTOS kernel" "lib/FreeRTOS-Kernel (submodule)"
    else mark note "FreeRTOS kernel" "not set up (only L5 needs it — setup §7)"; fi

    # The real thing: configure + build the smallest target end to end.
    if [ -n "${PICO_SDK_PATH:-}" ] && [ -f "$PICO_SDK_PATH/pico_sdk_init.cmake" ]; then
        local td; td="$(mktemp -d)"
        if cmake -S "$HERE/l0-blink" -B "$td" "${GEN_ARGS[@]}" -DPICO_BOARD="${PICO_BOARD:-pico2_w}" >"$td/log" 2>&1 \
           && cmake --build "$td" >>"$td/log" 2>&1 && [ -f "$td/l0_blink.uf2" ]; then
            mark ok "compile pipeline" "built l0_blink.uf2 (configure→compile→link OK)"
            rm -rf "$td"
        else mark FAIL "compile pipeline" "l0-blink failed — log kept at $td/log"; fi
    else mark note "compile pipeline" "skipped (fix PICO_SDK_PATH first)"; fi

    echo
    printf "Summary: %d ok, %d problem(s), %d note(s).\n" "$ok" "$bad" "$note"
    if [ "$bad" -eq 0 ]; then
        echo "Environment looks good — next: ./run.sh l0_blink   (build → flash → monitor on a connected board)."
    else
        echo "Fix the [FAIL] items above — each names its setup section. See ./run.sh --help."; return 1
    fi
}

cmd="${1:-}"; shift || true

# Split remaining args into KEY=VAL build overrides (forwarded as -D...) and
# positional args, e.g.  ./run.sh flash l4_optimize START_MODE=1 ENABLE_DBUF=0
defs=(); pos=(); known="$(known_knobs)"
for a in "$@"; do
    case "$a" in
        *=*)
            key="${a%%=*}"
            if ! printf '%s\n' "$known" | grep -qx "$key"; then
                echo "!! unknown build knob '$key' — likely a typo, so it will do NOTHING." >&2
                echo "   Recognised knobs: $(printf '%s ' $known)" >&2
            fi
            defs+=("-D$a")
            ;;
        *)   pos+=("$a") ;;
    esac
done

case "$cmd" in
    list)         list_targets ;;
    doctor|check) doctor ;;
    build)        [ -n "${pos[0]:-}" ] && build_target "${pos[0]}" || build_all ;;
    build-all)    build_all ;;
    flash)        [ -n "${pos[0]:-}" ] || { echo "usage: ./run.sh flash <target> [KEY=VAL ...]"; exit 2; }; flash_target "${pos[0]}" ;;
    load)         load_image "${pos[0]:-}" ;;
    debug)        [ -n "${pos[0]:-}" ] || { echo "usage: ./run.sh debug <target> [KEY=VAL ...]"; exit 2; }; debug_target "${pos[0]}" ;;
    monitor)      monitor "${pos[0]:-}" ;;
    ""|-h|--help) sed -n '2,/^set -euo/p' "$0" | sed '/^set -euo/d; s/^# \{0,1\}//' ;;
    *)            flash_target "$cmd"; monitor ;;   # e.g. ./run.sh l4_optimize START_MODE=1
esac
