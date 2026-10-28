#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IDL_FILE="${ROOT_DIR}/idl/ShipThreat.idl"
GENERATED_DIR="${RTIDDSGEN_OUTPUT_DIR:-${ROOT_DIR}/idl/generated}"
RTI_HOME="${RTI_CONNEXTDDS_DIR:-${NDDSHOME:-}}"

if [[ ! -f "${IDL_FILE}" ]]; then
    echo "IDL file not found: ${IDL_FILE}" >&2
    exit 1
fi

if [[ -n "${RTI_HOME}" ]]; then
    RTIDDSGEN="${RTI_HOME}/bin/rtiddsgen"
else
    RTIDDSGEN="$(command -v rtiddsgen || true)"
fi
if [[ -z "${RTIDDSGEN}" || ! -x "${RTIDDSGEN}" ]]; then
    echo "rtiddsgen was not found. Add RTI Connext DDS bin to PATH or set NDDSHOME." >&2
    exit 1
fi

mkdir -p "${GENERATED_DIR}"
"${RTIDDSGEN}" -ppDisable -replace -d "${GENERATED_DIR}" -language C++ -namespace -create typefiles "${IDL_FILE}"
