#!/bin/bash

# Paths/Directories
g_SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Source common.sh for common variables
source "${g_SCRIPT_DIR}/common.sh"

ROOT_DIR="${g_SCRIPT_DIR}/.."
BUILD_DIR="${g_SCRIPT_DIR}/../build/"
ANALYSIS_DIR="${BUILD_DIR}/analysis"
TEST_RESULT_DIR="${ROOT_DIR}/twister-out"
OUTPUT_MARKDOWN_FILE="${ANALYSIS_DIR}/analysis.md"

function write_output_header() {
    echo "# Analysis" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "" >> "${OUTPUT_MARKDOWN_FILE}"
}

function parse_test_results() {
    # Run tests
    test_output=$("${g_SCRIPT_DIR}/run_tests.sh")
    test_exit_code=$?
    twister_xml="${TEST_RESULT_DIR}/twister.xml"

    if [ -f "${twister_xml}" ]; then
        twister_output=$(cat "${twister_xml}")

        if echo "$test_output" | grep -q 'fail = [1-9]' || [ $test_exit_code -ne 0 ]; then
            header_str="Unit tests failed ❌"
        else
            header_str="Unit tests passed ✅"
        fi

        cat >> "${OUTPUT_MARKDOWN_FILE}" <<EOF
<details>
<summary>

## ${header_str}

</summary>

## Unit Test Results

\`\`\`xml
${twister_output}
\`\`\`

<details>
<summary>Twister Log</summary>

\`\`\`
${test_output}
\`\`\`

</details>

</details>
EOF
    else
        if [ $test_exit_code -eq 0 ]; then
            header_str="Unit tests skipped ℹ️"
        else
            header_str="Unit tests failed ❌"
        fi

        cat >> "${OUTPUT_MARKDOWN_FILE}" <<EOF
<details>
<summary>

## ${header_str}

</summary>

\`\`\`
${test_output}
\`\`\`

</details>
EOF
    fi
}

function parse_code_coverage_results() {
    coverage_file="${TEST_RESULT_DIR}/coverage/coverage.txt"

    if [ ! -f "${coverage_file}" ]; then
        cat >> "${OUTPUT_MARKDOWN_FILE}" <<EOF
<details>
<summary>

## Code Coverage Skipped ℹ️

</summary>

No code coverage report generated.

</details>
EOF
        return
    fi

    coverage_output=$(cat "${coverage_file}")

    cat >> "${OUTPUT_MARKDOWN_FILE}" <<EOF
<details>
<summary>

## Code Coverage Report

</summary>

\`\`\`
${coverage_output}
\`\`\`

</details>
EOF

}

function parse_format_check_results() {
    # Write header

    echo "## Code Format Results" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "" >> "${OUTPUT_MARKDOWN_FILE}"

    # Run format check
    format_results=$("${g_SCRIPT_DIR}/check_formatting.sh")

    # Generate report
    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"

    if [ -z "${format_results}" ]; then
        echo "Code Formatting Passed ✅" >> "${OUTPUT_MARKDOWN_FILE}"
    else
        echo "Code Formatting Failed ❌" >> "${OUTPUT_MARKDOWN_FILE}"
        echo "${format_results}" >> "${OUTPUT_MARKDOWN_FILE}"
    fi
    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"
}

function parse_codechecker_results() {
    set -o pipefail
    "${g_SCRIPT_DIR}/run_codechecker.sh" 2>&1 | tee /dev/stderr
    codechecker_exit_code=$?
    set +o pipefail
    summary_file="${ANALYSIS_DIR}/codechecker/summary.txt"
    if [ -f "${summary_file}" ]; then
        summary_output=$(cat "${summary_file}")
    else
        summary_output="Summary not available."
    fi
    if [ $codechecker_exit_code -eq 0 ]; then
        header_str="Static Analysis (CodeChecker) passed ✅"
    else
        header_str="Static Analysis (CodeChecker) failed ⚠️"
    fi

    cat >> "${OUTPUT_MARKDOWN_FILE}" <<EOF
<details>
<summary>

## ${header_str}

</summary>

\`\`\`
${summary_output}
\`\`\`

</details>
EOF
}

function parse_cyc_complexity_results() {
    # Write header
    echo "## Cyclomatic Complexity Results" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "" >> "${OUTPUT_MARKDOWN_FILE}"

    # Run the script and capture its output
    complexity_results=$("${g_SCRIPT_DIR}/run_complexity.sh")
    # Capture the exit code
    exit_code=$?
    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"

    # Check the exit code and perform actions based on it
    if [ $exit_code -eq 0 ]; then
        if echo "$complexity_results" | grep -q "No thresholds exceeded"; then
            echo "Cyclomatic Complexity Passed ✅" >> "${OUTPUT_MARKDOWN_FILE}"
        else
            echo "Cyclomatic Complexity Failed ❌" >> "${OUTPUT_MARKDOWN_FILE}"

        fi
    else
        echo "Cyclomatic Complexity Failed To Run ❌" >> "${OUTPUT_MARKDOWN_FILE}"
    fi

    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "$complexity_results" >> "${OUTPUT_MARKDOWN_FILE}"
    echo "\`\`\`" >> "${OUTPUT_MARKDOWN_FILE}"
}

function main() {
    # Delete analysis directory if already exists
    if [ -d "${ANALYSIS_DIR}" ]; then
        rm -rf "${ANALYSIS_DIR}" &> /dev/null
    fi

    # Create analysis directory
    mkdir -p "${ANALYSIS_DIR}"
    write_output_header

    # Parse cyclomatic complexity
    parse_cyc_complexity_results

    # Parse code formatting results
    parse_format_check_results

    # Parse static analysis results
    parse_codechecker_results

    # Parse testing results
    parse_test_results
    parse_code_coverage_results
}

main

# Exit with error code if any analysis failed
if grep -q "❌" "${OUTPUT_MARKDOWN_FILE}"; then
    echo "Analysis failed - check the PR comment for details"
    exit 1
fi
