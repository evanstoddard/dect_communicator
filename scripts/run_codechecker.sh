#!/bin/bash

set -euo pipefail

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Ensure pip-installed binaries and local tools are in PATH (needed for CI)
export PATH="$HOME/.local/bin:${g_SCRIPT_DIR}/bin:$PATH"

# Source common.sh for common variables
source "${g_SCRIPT_DIR}/common.sh"

ROOT_DIR="$(cd "${g_ROOT_DIR}" && pwd)"
export CODECHECKER_ROOT="${ROOT_DIR}"
BUILD_DIR="${ROOT_DIR}/build"
ANALYSIS_DIR="${BUILD_DIR}/analysis"
CODECHECKER_DIR="${ANALYSIS_DIR}/codechecker"
REPORT_DIR="${CODECHECKER_DIR}/reports"
HTML_DIR="${CODECHECKER_DIR}/html"
ZIP_OUT="${CODECHECKER_DIR}/codechecker-html.zip"
FILTERED_COMPDB="${CODECHECKER_DIR}/compile_commands.filtered.json"
SKIP_FILE="${CODECHECKER_DIR}/codechecker.skip"
SUMMARY_OUT="${CODECHECKER_DIR}/summary.txt"
DEFAULT_CONFIG_FILE="${ROOT_DIR}/.codechecker.yml"

SKIP_DIRS=(
    "${ROOT_DIR}/zephyr"
    "${ROOT_DIR}/modules"
    "${ROOT_DIR}/nrfxlib"
    "${ROOT_DIR}/nrf"
    "${ROOT_DIR}/bootloader"
    "${ROOT_DIR}/example"
    "${ROOT_DIR}/build"
    "${ROOT_DIR}/project/tests"
)

skip_build=0
pristine=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-build)
            skip_build=1
            shift
            ;;
        --no-pristine)
            pristine=0
            shift
            ;;
        *)
            shift
            ;;
    esac
done

CODECHECKER_CMD=""
if command -v CodeChecker &> /dev/null; then
    CODECHECKER_CMD="CodeChecker"
elif command -v codechecker &> /dev/null; then
    CODECHECKER_CMD="codechecker"
else
    echo "CodeChecker not found. Install with: pip install codechecker"
    exit 2
fi

PYTHON_BIN="${PYTHON_BIN:-python3}"
if ! command -v "${PYTHON_BIN}" &> /dev/null; then
    PYTHON_BIN="python"
fi
if ! command -v "${PYTHON_BIN}" &> /dev/null; then
    echo "Python is required to filter compile_commands.json"
    exit 2
fi

config_args=()
if [[ -n "${CODECHECKER_CONFIG_FILE:-}" ]]; then
    if [[ -f "${CODECHECKER_CONFIG_FILE}" ]]; then
        config_args+=(--config "${CODECHECKER_CONFIG_FILE}")
    else
        echo "CodeChecker config not found: ${CODECHECKER_CONFIG_FILE}"
        exit 2
    fi
elif [[ -f "${DEFAULT_CONFIG_FILE}" ]]; then
    config_args+=(--config "${DEFAULT_CONFIG_FILE}")
fi

if [[ ${skip_build} -eq 0 ]]; then
    build_args=()
    if [[ ${pristine} -eq 1 ]]; then
        build_args+=(--pristine)
    fi
    build_args+=(-- "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
    if [[ -n "${CODECHECKER_BUILD_ARGS:-}" ]]; then
        read -r -a extra_build_args <<< "${CODECHECKER_BUILD_ARGS}"
        build_args+=("${extra_build_args[@]}")
    fi
    "${g_SCRIPT_DIR}/build.sh" "${build_args[@]}"
fi

COMPDB_CANDIDATES=()
if [[ -n "${CODECHECKER_COMPDB:-}" ]]; then
    COMPDB_CANDIDATES+=("${CODECHECKER_COMPDB}")
fi
COMPDB_CANDIDATES+=(
    "${BUILD_DIR}/compile_commands.json"
    "${BUILD_DIR}/app/compile_commands.json"
)

COMPDB=""
for candidate in "${COMPDB_CANDIDATES[@]}"; do
    if [[ -f "${candidate}" ]]; then
        COMPDB="${candidate}"
        break
    fi
done

if [[ -z "${COMPDB}" ]]; then
    if command -v find &> /dev/null; then
        found_compdb=$(find "${BUILD_DIR}" -maxdepth 3 -name compile_commands.json -print -quit 2>/dev/null || true)
        if [[ -n "${found_compdb}" ]]; then
            COMPDB="${found_compdb}"
        fi
    fi
fi

if [[ -z "${COMPDB}" ]]; then
    echo "Missing compile_commands.json. Re-run build with CMAKE_EXPORT_COMPILE_COMMANDS=ON."
    exit 2
fi

rm -rf "${CODECHECKER_DIR}"
mkdir -p "${CODECHECKER_DIR}"

export ROOT_DIR
export FILTERED_COMPDB
export COMPDB

filtered_count=$("${PYTHON_BIN}" - <<'PY'
import json
import os
from pathlib import Path
root = Path(os.environ["ROOT_DIR"]).resolve()
compdb = Path(os.environ["COMPDB"]).resolve()
out = Path(os.environ["FILTERED_COMPDB"]).resolve()
allowed = [
    (root / "project" / "app").resolve(),
    (root / "project" / "drivers").resolve(),
]
def is_allowed(path: Path) -> bool:
    for base in allowed:
        try:
            path.relative_to(base)
            return True
        except ValueError:
            continue
    return False
with compdb.open("r", encoding="utf-8") as handle:
    data = json.load(handle)
filtered = []
for entry in data:
    directory = entry.get("directory", "")
    file_path = Path(entry.get("file", ""))
    if not file_path.is_absolute():
        file_path = (Path(directory) / file_path).resolve()
    else:
        file_path = file_path.resolve()
    if is_allowed(file_path):
        filtered.append(entry)
with out.open("w", encoding="utf-8") as handle:
    json.dump(filtered, handle, indent=2)
print(len(filtered))
PY
)

if [[ "${filtered_count}" == "0" ]]; then
    echo "No source files found in project/app or project/drivers for analysis."
    exit 2
fi

analyzer_list=()
if [[ -n "${CODECHECKER_ANALYZERS:-}" ]]; then
    read -r -a analyzer_list <<< "${CODECHECKER_ANALYZERS}"
else
    analyzer_list=("clangsa")
    if command -v clang-tidy &> /dev/null; then
        analyzer_list+=("clang-tidy")
    else
        echo "clang-tidy not found, running clangsa only."
    fi
fi

analyze_skip_args=()
parse_skip_args=()
if "${CODECHECKER_CMD}" analyze --help 2>&1 | grep -q -- "--skip"; then
    printf -- "-%s\n" "${SKIP_DIRS[@]}" > "${SKIP_FILE}"
    analyze_skip_args+=(--skip "${SKIP_FILE}")
fi
if "${CODECHECKER_CMD}" parse --help 2>&1 | grep -q -- "--skip"; then
    if [[ ! -f "${SKIP_FILE}" ]]; then
        printf -- "-%s\n" "${SKIP_DIRS[@]}" > "${SKIP_FILE}"
    fi
    parse_skip_args+=(--skip "${SKIP_FILE}")
fi

extra_analyze_opts=()
if [[ -n "${CODECHECKER_ANALYZE_OPTS:-}" ]]; then
    read -r -a extra_analyze_opts <<< "${CODECHECKER_ANALYZE_OPTS}"
fi

extra_parse_opts=()
if [[ -n "${CODECHECKER_PARSE_OPTS:-}" ]]; then
    read -r -a extra_parse_opts <<< "${CODECHECKER_PARSE_OPTS}"
fi

"${CODECHECKER_CMD}" analyze \
    --output "${REPORT_DIR}" \
    --analyzers "${analyzer_list[@]}" \
    --jobs "${CODECHECKER_JOBS:-$(getconf _NPROCESSORS_ONLN)}" \
    "${analyze_skip_args[@]}" \
    "${config_args[@]}" \
    "${extra_analyze_opts[@]}" \
    "${FILTERED_COMPDB}"

parse_exit=0
"${CODECHECKER_CMD}" parse \
    "${REPORT_DIR}" \
    --export html \
    --output "${HTML_DIR}" \
    "${parse_skip_args[@]}" \
    "${config_args[@]}" \
    "${extra_parse_opts[@]}" || parse_exit=$?
if [[ ${parse_exit} -ne 0 && ${parse_exit} -ne 2 ]]; then
    echo "CodeChecker parse failed with exit code ${parse_exit}"
    exit ${parse_exit}
fi

parse_status=${parse_exit}

summary_raw=$("${CODECHECKER_CMD}" parse \
    "${REPORT_DIR}" \
    "${parse_skip_args[@]}" \
    "${extra_parse_opts[@]}" 2>/dev/null) || true

# Extract summary block - try Severity Statistics first, fall back to Summary
summary_block=$(echo "${summary_raw}" | awk '
    BEGIN {inside=0; end=0}
    /^----==== Severity Statistics ====----/ {inside=1}
    {if(inside) print}
    /^----=================----/ {
        if(inside){
            end++;
            if(end==4){exit}
        }
    }
')

# If no severity statistics (no violations), extract the Summary block instead
if [[ -z "${summary_block}" ]]; then
    summary_block=$(echo "${summary_raw}" | awk '
        BEGIN {inside=0}
        /^----======== Summary ========----/ {inside=1}
        {if(inside) print}
        /^----=================----/ {if(inside) exit}
    ')
fi

if [[ -z "${summary_block}" ]]; then
    summary_block="Summary not available."
fi

echo "${summary_block}" > "${SUMMARY_OUT}"

archive_path="${ZIP_OUT}"
if ! command -v zip &> /dev/null; then
    echo "zip is required to archive the HTML report."
    exit 2
fi

if [[ -d "${HTML_DIR}" ]]; then
    (cd "${HTML_DIR}" && zip -qr "${ZIP_OUT}" .)
else
    zip -jq "${ZIP_OUT}" "${HTML_DIR}"
fi

echo "CodeChecker parse exit code: ${parse_status}"
echo "CodeChecker HTML report archive: ${archive_path}"
echo "CodeChecker report data: ${REPORT_DIR}"

if [[ "${parse_status}" -eq 2 ]]; then
    exit 1
fi