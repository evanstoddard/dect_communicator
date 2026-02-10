#!/bin/bash

set -e

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

# Ensure project initialized
if [ ! -d ${g_ROOT_DIR}/.west ]; then
    echo "Project doesn't exist..."
    ${g_SCRIPT_DIR}/init_project.sh
fi

# Build default target with default board
# TODO: Update this script to accept arguments for targets and boards

pushd ${g_ROOT_DIR}
${g_WEST} build --board=${g_DEFAULT_BUILD_BOARD} ${g_PROJECT_NAME}/${g_DEFAULT_BUILD_TARGET} "$@"
popd
