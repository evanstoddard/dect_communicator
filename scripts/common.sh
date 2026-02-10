#!/bin/bash

# Prevents this script from being run directly
if [ ${0##*/} == ${BASH_SOURCE[0]##*/} ]; then 
    echo "WARNING"
    echo "This script is not meant to be executed directly!"
    echo "Use this script only by sourcing it."
    echo
    exit 1
fi

# Brief: Common variables and functions used by scripts in this directory

# Source project config
source ${g_SCRIPT_DIR}/project.conf

# Paths/Directories
g_NRFUTIL_PATH="nrfutil"
g_SCRIPT_BIN_DIR=${g_SCRIPT_DIR}/bin
g_ROOT_DIR=${g_SCRIPT_DIR}/..
g_PROJECT_DIR=${g_ROOT_DIR}/${g_PROJECT_NAME}

g_TOOLCHAIN_SHELL="${g_NRFUTIL_PATH} toolchain-manager launch --ncs-version ${g_NORDIC_TOOLCHAIN_VERSION} --shell"
g_WEST="${g_TOOLCHAIN_SHELL} -- west"

# Variable to store the OS type
g_MACHINE="unsupported"

# Function to determine the OS of the current system
function determine_os() {
    uname_out="$(uname -s)"
    case "${uname_out}" in
        Linux*)     g_MACHINE=linux;;
        Darwin*)    g_MACHINE=mac;;
        *)          g_MACHINE="unsupported"
    esac
}

# Function to determine if nrfutil is installed and in a location accessible via $PATH
function check_for_nrfutil_in_path() {
    # Check if nrfutil exists in PATH
    if command -v nrfutil &> /dev/null; then
        g_NRFUTIL_PATH="nrfutil"
        return
    fi

    # Check if a local copy of nrfutil has already been installed
    if [[ -x "${g_SCRIPT_BIN_DIR}/nrfutil" ]]; then
        g_NRFUTIL_PATH="${g_SCRIPT_BIN_DIR}/nrfutil"
        return
    fi

    echo "Could not find \"nrfutil\". Installing locally."
    install_nrfutil_locally
}

# Install nrfutil to scripts/bin
function install_nrfutil_locally() {
    if [ ! -d ${g_SCRIPT_BIN_DIR} ]; then
        mkdir ${g_SCRIPT_BIN_DIR}
    fi
    
    g_NRFUTIL_PATH=${g_SCRIPT_BIN_DIR}/nrfutil
    
    # Detrmine the OS to know which version of nrfutil to install
    determine_os
    
    NRFUTIL_DOWNLOAD_URL=""
    if [ ${g_MACHINE} == "linux" ]; then
        NRFUTIL_DOWNLOAD_URL="https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/x86_64-unknown-linux-gnu/nrfutil"
    elif [ ${g_MACHINE} == "mac" ]; then
        NRFUTIL_DOWNLOAD_URL="https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/universal-apple-darwin/nrfutil"
    else
        echo "Unsupported platform.  Please use Linux or Mac OS."
        exit 1
    fi;
    
    curl ${NRFUTIL_DOWNLOAD_URL} -o ${g_NRFUTIL_PATH}
    chmod +x ${g_NRFUTIL_PATH}
}

# Checks and installs "device" & "toolchain-manager" commands
function check_for_nrfutil_commands() {
    # Check for and install toolchain-manager command if not already installed
    if ! ${g_NRFUTIL_PATH} list | grep -q "toolchain-manager"; then
        echo "Toolchain manager command not found. Installing now..."
        ${g_NRFUTIL_PATH} install toolchain-manager
    fi

    # Check for and install device command if not already installed
    if ! ${g_NRFUTIL_PATH} list | grep -q "device"; then
        echo "Device command not found. Installing now..."
        ${g_NRFUTIL_PATH} install device
    fi
}

# Check and install toolchain if not found
function check_for_toolchain() {
    if ! ${g_NRFUTIL_PATH} toolchain-manager list | grep -q "${g_NORDIC_TOOLCHAIN_VERSION}"; then
        echo "Toolchain version not installed. Installing now..."
        ${g_NRFUTIL_PATH} toolchain-manager install --ncs-version ${g_NORDIC_TOOLCHAIN_VERSION}
    fi
}

function determine_toolchain_path() {
    g_TOOLCHAIN_SHELL="${g_NRFUTIL_PATH} toolchain-manager launch --ncs-version ${g_NORDIC_TOOLCHAIN_VERSION} --shell"
    g_WEST="${g_TOOLCHAIN_SHELL} -- west"
}

check_for_nrfutil_in_path
check_for_nrfutil_commands
check_for_toolchain
determine_toolchain_path