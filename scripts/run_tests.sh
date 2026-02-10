#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

# Ensure project initialized
if [ ! -d ${g_ROOT_DIR}/.west ]; then
    echo "Project doesn't exist..."
    ${g_SCRIPT_DIR}/init_project.sh
fi

pushd "${g_ROOT_DIR}" > /dev/null
trap 'popd > /dev/null' EXIT

# Remove previous twister-out directory if it exists
GCOV_DIR=$(realpath "${g_ROOT_DIR}/twister-out")

if [ -d "$GCOV_DIR" ]; then
    rm -rf "$GCOV_DIR"
fi

rm -rf ${g_ROOT_DIR}/twister-out*

TEST_ROOT_REL="project/tests"
TEST_ROOT="${g_ROOT_DIR}/${TEST_ROOT_REL}"

if [ ! -d "${TEST_ROOT}" ]; then
    echo "No Zephyr unit tests found in ${TEST_ROOT_REL}, skipping twister run."
    exit 0
fi

# Skip running twister if no Zephyr unit test definitions are present
if ! find "${TEST_ROOT}" \( -name 'testcase.yaml' -o -name 'testcase.yml' \) -print -quit | grep -q .; then
    echo "No Zephyr unit test cases found under ${TEST_ROOT_REL}, skipping twister run."
    exit 0
fi

# Run twister
${g_WEST} twister -p native_sim/native/64 --coverage --enable-coverage --coverage-formats html,txt --coverage-basedir ${g_ROOT_DIR}/project -T project/tests 
