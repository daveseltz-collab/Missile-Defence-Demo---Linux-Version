#!/usr/bin/env bash
set -euo pipefail

# Snap GTK overrides can survive in a terminal after SNAP_NAME is removed.
# Detect the paths themselves, including shared Snap runtimes, not the marker.
for variable in GSETTINGS_SCHEMA_DIR GTK_PATH GTK_EXE_PREFIX GTK_IM_MODULE_FILE \
                GIO_MODULE_DIR GIO_EXTRA_MODULES GDK_PIXBUF_MODULE_FILE GDK_PIXBUF_MODULEDIR; do
    value="$(printenv "${variable}" || true)"
    case "${value}" in
        *"/snap/"*) unset "${variable}" ;;
    esac
done
for variable in XDG_DATA_DIRS XDG_CONFIG_DIRS; do
    value="$(printenv "${variable}" || true)"
    case "${value}" in
        *"/snap/"*) ;;
        *) continue ;;
    esac
    # Prefer the original host paths when VS Code recorded them, but still
    # filter those paths in case the original environment was also polluted.
    if [[ "${value}" == *"/snap/code/"* ]]; then
        original="$(printenv "${variable}_VSCODE_SNAP_ORIG" || true)"
        if [[ -n "${original}" ]]; then
            value="${original}"
        fi
    fi
    IFS=: read -r -a directories <<< "${value}"
    host_directories=
    for directory in "${directories[@]}"; do
        case "${directory}" in
            *"/snap/"*|"") continue ;;
        esac
        host_directories="${host_directories:+${host_directories}:}${directory}"
    done
    if [[ -n "${host_directories}" ]]; then
        export "${variable}=${host_directories}"
    else
        unset "${variable}"
    fi
done

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_APPS="${ROOT_DIR}/build/apps"

find_executable() {
    local app="$1"
    local executable="$2"
    local candidate

    for candidate in \
        "${BUILD_APPS}/${app}/${executable}" \
        "${BUILD_APPS}/${app}/Debug/${executable}" \
        "${BUILD_APPS}/${app}/Release/${executable}"; do
        if [[ -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done
    return 1
}

SENSOR_BIN="$(find_executable sensor sensor)" || {
    echo "Cannot find the sensor executable under ${BUILD_APPS}. Build the demo first." >&2
    exit 1
}
EFFECTOR_BIN="$(find_executable effector effector)" || {
    echo "Cannot find the effector executable under ${BUILD_APPS}. Build the demo first." >&2
    exit 1
}
CONTROL_BIN="$(find_executable command_control command_control)" || {
    echo "Cannot find the command_control executable under ${BUILD_APPS}. Build the demo first." >&2
    exit 1
}

RUNTIME_PATH_FILE="${ROOT_DIR}/build/dds-library-path.txt"
if [[ ! -r "${RUNTIME_PATH_FILE}" ]]; then
    echo "DDS runtime configuration is missing. Configure and build with -DUSE_CONNEXT=ON." >&2
    exit 1
fi
IFS= read -r RTI_LIB_DIR < "${RUNTIME_PATH_FILE}"
if [[ ! -d "${RTI_LIB_DIR}" ]]; then
    echo "The configured DDS runtime directory does not exist: ${RTI_LIB_DIR}" >&2
    exit 1
fi
export LD_LIBRARY_PATH="${RTI_LIB_DIR}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

for executable in "${SENSOR_BIN}" "${EFFECTOR_BIN}" "${CONTROL_BIN}"; do
    if ! "${executable}" --check-dds; then
        echo "The demo requires DDS-enabled binaries. Rebuild with -DUSE_CONNEXT=ON." >&2
        exit 1
    fi
done

cd -- "${ROOT_DIR}"

sensor_pid=
effector_pid=

cleanup() {
    local status=$?
    trap - EXIT INT TERM
    if [[ -n "${sensor_pid}" ]]; then
        kill "${sensor_pid}" 2>/dev/null || true
        wait "${sensor_pid}" 2>/dev/null || true
    fi
    if [[ -n "${effector_pid}" ]]; then
        kill "${effector_pid}" 2>/dev/null || true
        wait "${effector_pid}" 2>/dev/null || true
    fi
    exit "${status}"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Starting sensor: ${SENSOR_BIN}"
"${SENSOR_BIN}" &
sensor_pid=$!

echo "Starting effector: ${EFFECTOR_BIN}"
"${EFFECTOR_BIN}" &
effector_pid=$!

sleep 1
for service_pid in "${sensor_pid}" "${effector_pid}"; do
    if ! kill -0 "${service_pid}" 2>/dev/null; then
        echo "A DDS service failed to start. Check the diagnostics above." >&2
        exit 1
    fi
done

echo "Starting command and control: ${CONTROL_BIN}"
"${CONTROL_BIN}"
