#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

# Initialize project
function initialize_project() {
    echo ${g_WEST}
    if [ ! -d ${g_ROOT_DIR}/.west ]; then
        echo "Initializing project..."
        ${g_WEST} init -l ${g_PROJECT_DIR}
    fi
}

function run_west_update() {
    # Change into repo root
    pushd ${g_ROOT_DIR}
    
    # Run the following west update command
    ${g_WEST} update -f smart -n -o=--depth=1
    
    popd
}

initialize_project
run_west_update