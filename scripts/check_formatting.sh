#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source ${g_SCRIPT_DIR}/common.sh

# Change into root and run tests
pushd "${g_ROOT_DIR}" > /dev/null

# Directories to start the search (relative paths)
DIRECTORY1="${g_PROJECT_NAME}/${g_DEFAULT_BUILD_TARGET}/src"


# Directory containing the .clang-format file (relative path)
FORMAT_DIRECTORY="${g_ROOT_DIR}"

# Variable to keep track of any unformatted files
unformatted_files=()

# Find C/C++ files in both directories and process them
while IFS= read -r file; do 
    # Run clang-format in dry-run mode
    output=$(clang-format --dry-run -style=file:"${FORMAT_DIRECTORY}/.clang-format" "${file}" 2>&1)

    # Check for warnings
    if [[ "${output}" == *"warning:"* ]]; then
        unformatted_files+=("${file}")
    fi
done < <(find "${DIRECTORY1}" \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \))

# Print the list of unformatted files 
for file in "${unformatted_files[@]}"; do 
    echo "${file}"
done
