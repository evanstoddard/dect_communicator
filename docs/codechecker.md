# CodeChecker Static Analysis

## Run locally

- Full run (build + analyze + report):
  `./scripts/run_codechecker.sh`
- Analyze only (reuse existing build output):
  `./scripts/run_codechecker.sh --no-build`

Outputs:
- HTML report: `build/analysis/codechecker/html/index.html`
- Zipped report: `build/analysis/codechecker/codechecker-html.zip`
- Raw report data: `build/analysis/codechecker/reports/`

## Configuration files

- `.clang-tidy`: clang-tidy checks and options.
- `.codechecker.yml`: CodeChecker analyze config (used automatically).
- `.codechecker.tidyargs`: extra clang-tidy arguments (loaded via config).

## Mark findings as reviewed

Use in-source annotations:

Add a comment above the flagged line:

```c
// codechecker_false_positive [checker-name] Your justification here
```

Other supported statuses:
- `codechecker_suppress`
- `codechecker_intentional`
- `codechecker_confirmed`

These comments are picked up during `parse` and reflected in the HTML report.

## Suppress specific violations

Choose one of these, based on scope:

1) **Single line**: use in-source annotations (above).
2) **Checker-wide**: add `-Wno-...` to `.codechecker.tidyargs` (clang-tidy only).
3) **Exclude paths**: update `scripts/run_codechecker.sh` `SKIP_DIRS` list.

## Notes

- The runner filters analysis to `project/app` and `project/drivers`.
- The HTML report shows a Review Status column when in-source review comments are present.