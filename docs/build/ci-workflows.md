# GitHub Actions CI & Release Workflows

## 1. Pattern & Key Files

- [`.github/workflows/build.yml`](../../.github/workflows/build.yml): Linux (`ubuntu-26.04`) and Windows (`windows-2025`) builds, `ctest`, Linux `ldd` check, alpha packaging, and `workflow_dispatch` release retry via `run_id`.
- [`.github/workflows/opencode-review.yml`](../../.github/workflows/opencode-review.yml): automated PR reviews via `dceoy/opencode-action` (`runtime: v2`, `review-only: true`).
- [`.github/workflows/triage.yml`](../../.github/workflows/triage.yml) and [`.github/dependabot.yml`](../../.github/dependabot.yml): issue/PR triage and weekly `github-actions` dependency updates.

## 2. When to Use It (Do's & Don'ts)

- **Do** pin all third-party actions by full 40-character commit SHA with a `# vX.Y.Z` comment, and let Dependabot keep them current.
- **Do** pass `GITHUB_TOKEN` in the `Configure CMake` step environment so `vcpkg` manifest installs avoid anonymous GitHub API rate limits.
- **Do** re-publish an alpha release from an existing green `Build RME` run by triggering `workflow_dispatch` with its `run_id` instead of rebuilding both platforms.
- **Don't** check out untrusted PR head code inside `pull_request_target` workflows with write tokens; `opencode-review.yml` uses `review-only: true` and reads frozen commit blobs via GitHub API.

## 3. Minimal Usage

```yaml
# Retry alpha release publishing from an existing green Build RME run:
# GitHub UI -> Actions -> Build RME -> Run workflow -> run_id: <RUN_ID>
# Or via CLI:
# gh workflow run build.yml -f run_id=45497873668
```

## 4. Invariants & Pitfalls

- `Build Linux` and `Build Windows` job names must match the required status checks in `master` ruleset `12110183`—do not rename those job `name:` fields without updating the ruleset.
- In `build.yml`, `release` uses `!cancelled()` in its `if:` condition because `needs: [build-linux, build-windows]` would otherwise skip `release` when `run_id` skips the build jobs.
- `build-windows` sets up MSVC via `vswhere.exe` + `vcvars64.bat` (preserving `PATH` precedence by reversing `$addedPaths` into `GITHUB_PATH` and pinning `CC=cl`/`CXX=cl` + `-DCMAKE_{C,CXX}_COMPILER=cl`) and relies on preinstalled Ninja on `windows-2025` so CMake does not pick `Llvm/x64/bin/clang++.exe`.
