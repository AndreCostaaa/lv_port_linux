# LVGL test coverage: v9.5.0 vs master

**Date of measurement:** 2026-09-14
**Baseline ref:** `v9.5.0` = `85aa60d18` (2026-02-18, "chore: release v9.5.0 (#9753)")
**Head ref:** `master` = `99c76d715` (2026-09-14 14:02 CEST, "fix(frogfs): do not typedef ssize_t when the libc has it (#10694)")
**Span:** 743 commits, ~7 months

---

## 1. Headline

| metric | v9.5.0 | master | delta |
|---|---:|---:|---:|
| **line coverage** | **78.66%** | **82.23%** | **+3.57 pp** |
| covered lines | 46,756 | 56,021 | +9,265 |
| coverable lines | 59,442 | 68,129 | +8,687 |
| uncovered lines | 12,686 | 12,108 | −578 |
| branch coverage | 61.40% | 62.59% | +1.19 pp |
| covered branches | 22,874 / 37,252 | 28,410 / 45,392 | +5,536 |
| function coverage | 82.1% | 83.7% | +1.6 pp |
| files measured | 241 | 266 | +25 |

Absolute uncovered lines **fell by 578** even though the measured codebase grew by 8,687
coverable lines.

---

## 2. Coverable lines vs. new lines

Scope for all line counts below is the gcovr scope (`filter = src/(?:.*/)?lv_.*\.c`, minus the
six hardware-only excludes in `gcovr.cfg`).

| | added | removed | net |
|---|---:|---:|---:|
| in-scope `.c` text lines | 155,641 | 136,858 | **+18,783** |
| ├ generated font tables (`src/font/lv_font_*.c`) | 128,175 | 127,021 | +1,154 |
| └ real code | **27,466** | 9,837 | +17,629 |
| **coverable lines (gcovr)** | — | — | **+8,687** |

Of the 27,466 added lines of real code:

| | lines | share |
|---|---:|---:|
| coverable per gcovr | 11,984 | 43.6% of added |
| covered by tests | 9,697 | **80.9% of new coverable** |
| new coverable, never hit | 2,287 | 19.1% |
| in files no test config compiles | 7,875 | — |

**Interpretation:** ~436 coverable lines per 1,000 lines of new real code, landing at ~81%
covered. That is just below the 82.2% project average but well above the 78.7% starting
point, which is why the overall rate rose instead of being diluted.

The 7,875 added lines that never enter the denominator are all vendor GPU / hardware
backends, 136 files, led by:

| file | added lines |
|---|---:|
| `src/draw/sw/blend/sve2/lv_draw_sw_blend_sve2_to_rgb888.c` | 4,069 |
| `src/draw/sifli/epic/*` (10 files) | ~2,350 |
| `src/draw/dma2d/lv_draw_dma2d.c` | 178 |
| `src/drivers/evdev/lv_evdev.c` | 99 |
| `src/draw/nema_gfx/*`, `src/draw/nxp/g2d/*`, `src/stdlib/rtthread/*` | ~280 |

---

## 3. Where the +3.57 pp came from

File matching is rename-aware (several modules moved: `src/libs/svg` -> `src/image/svg`,
`src/libs/freetype` -> `src/font/freetype`, `src/libs/bin_decoder` -> `src/image`,
`src/misc/lv_fs.c` -> `src/fs/`).

| | coverable | covered | % |
|---|---:|---:|---:|
| files in both reports @ v9.5.0 | 59,163 | 46,522 | 78.6% |
| files in both reports @ master | 63,499 | 52,120 | 82.1% |
| **like-for-like change** | **+4,336** | **+5,598** | **+3.4 pp** |
| files new to the report (28) | 4,630 | 3,901 | 84.3% |
| files dropped from the report (3) | 279 | 234 | 83.9% |

Most of the gain is real improvement on the same files, concentrated in modules that were at
**0%** at v9.5.0 and are exercised now:

| module | covered / coverable @ master | % |
|---|---:|---:|
| `src/libs/gltf` | 2,917 / 3,430 | 85% |
| `src/draw/nanovg` | 1,492 / 1,716 | 87% |
| `src/widgets/gstreamer` | 328 / 461 | 71% |
| `src/widgets/barcode` | 290 / 318 | 91% |
| `src/widgets/qrcode` | 208 / 217 | 96% |

Caveat: a substantial part of this is the **new `OPTIONS_TEST_NANOVG` test config existing at
all** (it unlocks nanovg + glTF), not new test cases written against previously untested code.

### Largest blocks of untested new code

| file | uncovered / new coverable |
|---|---:|
| `src/core/lv_obj_style_gen.c` | 356 / 557 |
| `src/draw/sw/lv_draw_sw_transform.c` | 101 / 660 |
| `src/core/lv_observer.c` | 96 / 449 |
| `src/display/lv_display.c` | 81 / 412 |
| `src/widgets/chart/lv_chart.c` | 73 / 157 |
| `src/misc/lv_style_gen.c` | 72 / 134 |
| `src/draw/sw/blend/lv_draw_sw_blend_to_rgb565_swapped.c` | 67 / 236 |
| `src/core/lv_obj_style.c` | 65 / 336 |
| `src/font/binfont_loader/lv_binfont_loader.c` | 64 / 230 |
| `src/widgets/gstreamer/lv_gstreamer.c` | 59 / 171 |

---

## 4. Per-config contribution (master, 64-bit)

| config | covered / coverable | % | tests |
|---|---:|---:|---:|
| `OPTIONS_TEST_SYSHEAP` | 46,205 / 56,875 | 81.2 | 183 |
| `OPTIONS_TEST_RISCV_V` | 46,686 / 57,659 | 81.0 | 183 |
| `OPTIONS_TEST_DEFHEAP` | 46,699 / 57,943 | 80.6 | 183 |
| `OPTIONS_TEST_VG_LITE` | 42,567 / 60,703 | 70.1 | 183 |
| `OPTIONS_TEST_NANOVG` | 42,693 / 62,140 | 68.7 | 183 |
| `OPTIONS_TEST_PUBLIC_API` | 7,777 / 56,875 | 13.7 | 28 |
| **merged** | **56,019 / 68,129** | **82.2** | — |

The merged total exceeds every individual config, so the configs cover genuinely different
code: VG_LITE and NANOVG each add ~4–5k coverable lines nothing else compiles.

---

## 5. Methodology and provenance

**master numbers are official CI output.** Downloaded from GitHub Actions run `34841294966`
(`lvgl/lvgl`, push of `99c76d715`), artifacts `coverage-baseline` (56021/68129/82.2278%) and
`coverage-reports-64bit build` (`coverage.xml`, gcovr 7.0). Per-file and per-line data in this
report comes from that `coverage.xml`.

**v9.5.0 numbers were measured locally**, because CI artifacts expire after 90 days and
v9.5.0 is from February. Procedure:

1. `git worktree add <path> v9.5.0` (isolated tree; never the live working tree).
2. Ran in the CI-matching container (`tests/Dockerfile`, Ubuntu 24.04, gcc 13, gcovr 7.0)
   via `scripts/run_tests_docker.sh`'s image.
3. Ran all four test configs that exist at v9.5.0 (`SYSHEAP`, `DEFHEAP`, `VG_LITE`,
   `RISCV_V`), each in its own `tests/main.py --build-options=<OPT> test` invocation.
   **All four passed 141/141 tests, zero failures.**
4. **Copied master's `gcovr.cfg` into the v9.5.0 tree.** This is essential: v9.5.0's
   `tests/main.py` hard-codes a *different* gcovr filter (`--filter src/.*/lv_.*\.c`, no
   exclude list), so its native report is not comparable to master's. Both sides therefore
   use `filter = src/(?:.*/)?lv_.*\.c` plus the excludes for
   `src/draw/opengles/`, `src/drivers/opengles/`, `src/drivers/wayland/`,
   `src/drivers/display/drm/`, `src/drivers/display/fb/`, `src/drivers/libinput/`.
5. gcovr invoked identically on both sides, 64-bit only (matches the CI baseline job).

**Counting method verified equivalent.** Summing the gcovr detail JSON the way
`scripts/check_gcov_coverage.py` does yields exactly the same totals as gcovr's
`--json-summary` (46,756 / 59,442 for v9.5.0), so the two sides are counted the same way.

**Pipeline cross-check.** The same local pipeline was run against master (`1da53eaee`, six
configs, all green: 183/183 x5 plus 28/28) and produced **56,019 / 68,129 = 82.22%** versus
CI's **56,021 / 68,129 = 82.23%** at `99c76d715` — identical coverable-line count, two
covered lines apart. The local v9.5.0 measurement is therefore directly comparable to CI's
master number.

### Known limitations

- Coverage is computed only over files that at least one test config actually compiles.
  Files never built (disabled features, vendor backends) do not appear as 0%; they are
  absent from the denominator entirely. 431 in-scope `.c` files exist at master, 266 are
  measured.
- "Added lines" are raw diff insertions with rename detection (`-M`,
  `diff.renameLimit=20000`); a heavily rewritten line counts as both an insertion and a
  deletion, which is why insertions (27,466 real-code) exceed net growth (+17,629).
- 32-bit configs were not measured on either side (the CI baseline is 64-bit only).

---

## 6. Incidental findings (worth fixing)

1. **`tests/main.py` passes a bare `--gcov-ignore-parse-errors`.** gcovr 7.0 aborts on
   `NegativeHits: Got negative hit value in gcov line 'branch 1 taken -1'` (gcc bug 68080)
   unless a value is given. Use `--gcov-ignore-parse-errors=all` (or
   `negative_hits.warn_once_per_file`). This bit the v9.5.0 run and would bite CI whenever a
   negative-hit branch appears.
2. **`tests/main.py --report` silently produces no report when any test fails**, because
   `run_tests()` raises and `main.py` exits before `generate_code_coverage_report()`. A
   failing test in the first config also aborts the remaining configs, so a partial matrix
   can silently truncate coverage.
3. In this repo, `tests/report-64bit/` was stale (2026-08-18, 15.8% line rate) — the output
   of a partial gltf-only run, not a usable master baseline. Worth deleting or regenerating
   so it is not mistaken for current data.

---

## 7. Reproduction

```bash
# master (official): download CI artifacts
gh run download 34841294966 --repo lvgl/lvgl --name coverage-baseline
gh run download 34841294966 --repo lvgl/lvgl --name "coverage-reports-64bit build"

# v9.5.0 (local), in the CI-matching container:
git worktree add /path/lvgl-v95 v9.5.0
cp gcovr.cfg /path/lvgl-v95/gcovr.cfg          # equalise the measurement scope
docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" \
    -e HOME=/tmp -v /path/lvgl-v95:/work -w /work lvgl-tests:local bash -c '
      export GCOV=gcov-13
      for o in OPTIONS_TEST_SYSHEAP OPTIONS_TEST_DEFHEAP OPTIONS_TEST_VG_LITE OPTIONS_TEST_RISCV_V; do
          ./tests/main.py --build-options=$o test || true
      done
      gcovr --gcov-ignore-parse-errors=all --gcov-ignore-errors=no_working_dir_found \
            --root /work -j $(nproc) --merge-mode-functions=merge-use-line-min \
            --json detail.json --json-summary summary.json --print-summary'
```

Note: each build directory is ~9 GB; six configs will not fit on a small disk. Measure each
config with gcovr and delete its build dir before the next, then merge the per-config
tracefiles with `gcovr --add-tracefile`.
