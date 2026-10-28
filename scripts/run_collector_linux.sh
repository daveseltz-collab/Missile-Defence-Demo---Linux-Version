#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${NDDSHOME:-}" ]]; then
    echo "Set NDDSHOME to the RTI Connext DDS installation directory." >&2
    exit 2
fi

CONNEXTDDS_ARCH="${CONNEXTDDS_ARCH:-x64Linux4gcc8.5.0}"
export CONNEXTDDS_ARCH
export CONNEXT_VERSION="${CONNEXT_VERSION:-7.7.0}"
if [[ -z "${CONNEXT_ENV:-}" && -f "${NDDSHOME}/../.env" ]]; then
    export CONNEXT_ENV="${NDDSHOME}/../.env"
fi
export OBSERVABILITY_DOMAIN="${OBSERVABILITY_DOMAIN:-101}"
export OBSERVABILITY_CONTROL_PUBLIC_HOSTNAME="${OBSERVABILITY_CONTROL_PUBLIC_HOSTNAME:-localhost}"
export OBSERVABILITY_CONTROL_PORT="${OBSERVABILITY_CONTROL_PORT:-19098}"
export OBSERVABILITY_CONTROL_PUBLIC_PORT="${OBSERVABILITY_CONTROL_PUBLIC_PORT:-19098}"
if [[ -z "${RTI_LICENSE_FILE:-}" && -f "${NDDSHOME}/rti_license.dat" ]]; then
    export RTI_LICENSE_FILE="${NDDSHOME}/rti_license.dat"
fi

APP="${NDDSHOME}/resource/app/bin/${CONNEXTDDS_ARCH}/rticollectorserviceapp"
if [[ ! -x "${APP}" ]]; then
    echo "RTI Collector Service executable not found: ${APP}" >&2
    exit 1
fi

LD_LIBRARY_PATH="${NDDSHOME}/lib/${CONNEXTDDS_ARCH}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
for library_dir in "${NDDSHOME}"/third_party/*/"${CONNEXTDDS_ARCH}"/release/lib; do
    if [[ -d "${library_dir}" ]]; then
        LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${library_dir}"
    fi
done
export LD_LIBRARY_PATH

missing_libraries="$(ldd "${APP}" | grep "not found" || true)"
if [[ -n "${missing_libraries}" ]]; then
    echo "RTI Collector Service has unresolved shared libraries:" >&2
    echo "${missing_libraries}" >&2
    echo "Add the required RTI third-party library directories to LD_LIBRARY_PATH." >&2
    exit 1
fi

exec "${APP}" -cfgName NonSecureRemoteDebuggingLAN -verbosity 5
