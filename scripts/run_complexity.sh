#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

# Change into root and run tests without printing directory stack noise
pushd "${g_ROOT_DIR}" > /dev/null
trap 'popd > /dev/null' EXIT

# Function to check if lizard is installed
function check_lizard_installed() {
    if ! command -v lizard &> /dev/null; then
        echo "Lizard is not installed. Attempting to install with pip..."
        install_lizard
    fi
}

# Function to install lizard using pip
function install_lizard() {
    if command -v pip &> /dev/null; then
        pip install lizard
        if [ $? -ne 0 ]; then
            echo "❌ Failed to install Lizard with pip."
            exit 1
        fi
    else
        echo "❌ pip is not installed. Please install pip first."
        exit 1
    fi
}

# Function to run Lizard and check metrics
function run_lizard() {
    # Run lizard with specific thresholds
    lizard_output=$(lizard ${g_PROJECT_DIR}/${g_DEFAULT_BUILD_TARGET}/src -x "${g_PROJECT_DIR}/${g_DEFAULT_BUILD_TARGET}/src/jsmn/*")

    # Check if thresholds were exceeded
    if echo "$lizard_output" | grep -q "No thresholds exceeded"; then
        # Only show the summary line if no issues
        echo "$lizard_output" | grep "No thresholds exceeded"
        echo "$lizard_output" | tail -4 | head -3  # Show the summary table
    else
        # Only show the warnings section if there are issues
        echo "$lizard_output" | grep -A 20 "!!!! Warnings"
        echo "$lizard_output" | tail -4 | head -3  # Show the summary table
    fi
}

# Main script execution
check_lizard_installed
run_lizard
