#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common_config.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

${g_TOOLCHAIN_SHELL}
