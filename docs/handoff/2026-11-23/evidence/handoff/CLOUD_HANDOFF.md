# Cloud handoff (C-line) — apply only after identity match

Time: 2026-11-17T09:42:29+08:00 (Asia/Shanghai (UTC+8))

Remote found (parent search; unique application repo):
https://github.com/dehai168/ax630c-plate-face-demo

**ref/commit are not verified here.** Status = remote_found, pending cloud identity check.
- `ready_to_investigate` = yes
- `ready_to_apply` = no, until required v2.4 base file SHAs match this archive (or an equivalent recorded commit). Mismatch → STOP, do not apply.

Do not train, compile, infer, rewrite patches, merge to production, or touch the board in this step.

## Files (exact original bytes)

| file | bytes | SHA256 |
|---|---:|---|
| `C/HANDOFF/R40_INTEGRATION_EXACT.diff` | 12871 | `4eaac6ac5248029b14853ac84fc84400a6550dcd6a820bb43bba5ebda328970a` |
| `C/HANDOFF/src_color5_crop_policy.inc` | 728 | `49508bca5c1f6abc323695f452d0a49205103d73dad57c09e2e7a4bd5858c1b6` |
| `C/HANDOFF/R46_MINIMAL_DERIVED_FORMAT_FIX.diff` | 3364 | `2b6d511bec7d5b46e1af7415fa973de9347d1e74248cdd8a7bcd25602766b470` |
| `C/HANDOFF/src_approved_special_format.inc` | 1778 | `6cf14ec7b6f74e2c178816d3d35fd7fb244dada418fac8393165a0bb560f7c5b` |

Original paths (unchanged SHA):
- `/workspace/cn_val_su/v25/optimization_round40/INTEGRATION_EXACT.diff`
- `/workspace/cn_val_su/v25/optimization_round46/MINIMAL_DERIVED_FORMAT_FIX.diff`

Both diffs are already clean unified patches (no build logs). They were copied, not edited.
R40 paths are `gold/` / `integrated/`; R46 paths are `baseline/` / `fixed/`. From repo root use `git apply -p1` or `patch -p1`.

**R40 gap:** `INTEGRATION_EXACT.diff` changes `src/plate_rec.h`, `src/plate_rec.cpp`, `src/main.cpp` only. It `#include`s `color5_crop_policy.inc` but does not create that file. After a successful R40 apply, install `src_color5_crop_policy.inc` as `src/color5_crop_policy.inc`. Do not modify the original R40 diff. R46 already contains the new `approved_special_format.inc`; the companion copy is for inspection only.

## Base identity (v2.4 archive, prefix `ax630c_plate_face_demo/`)

Archive `/workspace/ax630c_plate_face_demo_v2.4_src.tar.gz` recorded SHA `e04ff400ad5f7709b3dbfe518262baa7f8b9bd3989f4dbfe29135e98935074ba`. Cloud cannot read this archive; use the member SHAs below. R40 gold sources were verified equal to these members.

Required before apply:
- `src/plate_rec.h` 4695 `1310998b156f6449a62e40cfbd5ec79a9221948cc773ce2657ba91a323e4e7f9`
- `src/plate_rec.cpp` 13416 `d34d37015580cecd36063573bc5ef2938ba4d4cd305f8dccea3c4adbf20a8b71`
- `src/main.cpp` 57128 `3bbd4a3db62a8647b1896003ec42c63ef4840631f31747ca7f225dc747116cf5`
- `src/filters.cpp` 13783 `7a3a86c960c55c94642c3cdfbfd8d62821ad7b75c99aa5c1e800097976eb2188` (R46 base; unchanged by R40)

Recommended unchanged guards: `src/filters.h` `d2dd740053e350ef…ba031c`, `CMakeLists.txt` `107061f667083db6…196d29` (3259 B, not patched), `src/version.h` `e212f05214379dca…ce7459e`, `src/infer/engine_ort.cpp` `ecf7d5df344814e8…911f992`, `src/infer/engine_ax.cpp` `3c901f285481f54e…7b977b04`, `src/detector.cpp` `010ea14b6e031a61…076af0`, `src/bytetrack.cpp` `0d241e4cc6028996…dd82c2`.

## Apply order and stop rules

1. Record remote + candidate base ref/commit. Hash required base files.
2. If any required SHA mismatches: **STOP**. Report the commit and hashes. Do not apply.
3. `git apply -p1 --check` then `git apply -p1` on `R40_INTEGRATION_EXACT.diff`. Check/conflict → **STOP**.
4. Copy inc to `src/color5_crop_policy.inc` (exact 728 bytes).
5. Verify post-R40: `plate_rec.h` `ce144d54b501e0fa0386356bc452958bf2ad891b3004d9dd7e181626721cd185` (5362), `plate_rec.cpp` `fd653109a249d2ef123dab8b58904af1af52702e29b1f0243fb53df1e14310d6` (16676), `main.cpp` `39db22bb0d7d7aa2fb4466f33320a6111baebb4219076e7948ddae3789cd23c8` (58093), inc `49508bca…c1b6`. Mismatch → **STOP**.
6. Only then `git apply -p1 --check` / `git apply -p1` on `R46_MINIMAL_DERIVED_FORMAT_FIX.diff`. Do not apply R46 to vanilla v2.4. Conflict → **STOP**. Do not hand-edit hunks or whitespace to force a fit.
7. Verify final: `plate_rec.cpp` `44fed46dc546443e7b24cc7bb171884c6eaa22d287a962a927d34e03d33af3da` (16808), `filters.cpp` `f69c8649919f5c9d2ce915a4def6d061b9ecb1798d412d027c059324ada845d8` (13928), `approved_special_format.inc` `6cf14ec7b6f74e2c178816d3d35fd7fb244dada418fac8393165a0bb560f7c5b` (1778). `plate_rec.h` / `main.cpp` / color5 inc stay at post-R40 SHAs.
8. Independent non-production branch only. No production merge, compile, inference, or deploy in this step.

## Do not include

Models (axmodel/onnx/pt), NPU/ELF binaries, test videos/GT, protected/held/val sets, build logs, test harnesses, synthetic AVI, board backups, credentials, quantization artifacts, the pre-correction R46 helper, or the uninvestigated live box checkout. CMake is not patched. This apply is **not** an NPU, board-video, or production pass. R50 color5 numeric fail and R31 rejection remain.

Full machine-readable detail: `APPLY_MANIFEST.json`.
