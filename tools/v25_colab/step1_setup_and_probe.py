#!/usr/bin/env python3
"""
Step 1 on Colab:
1. Locate mounted Google Drive assets ('grok bot 文件内容').
2. Clone repo and align v2.5-rc canonical ONNX models (verify SHA256).
3. Extract A_OCR_REQUIRED, B_COLOR_SCRIPTS_MODELS_CAL64REF, and C_OPTIONAL 3rdparty.
4. Reassemble & unpack Pulsar2 8.0 (try LITE first; verify version & commands).
5. Persist full report and extracted R84 control.json / run_config.json to Drive.
"""
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

PERSIST_DIR = "/content/drive/MyDrive/ax630c_v25_rc_persist"
WORK_DIR = "/content/v25_work"
REPO_DIR = "/content/ax630c-plate-face-demo"
LOG_DIR = os.path.join(PERSIST_DIR, "logs")
os.makedirs(WORK_DIR, exist_ok=True)
os.makedirs(LOG_DIR, exist_ok=True)


def run(cmd, cwd=None, check=True):
    print(f"+ {cmd}", flush=True)
    p = subprocess.run(cmd, shell=True, cwd=cwd, text=True, capture_output=True)
    if p.stdout:
        print(p.stdout[-4000:], flush=True)
    if p.stderr:
        print(p.stderr[-2000:], file=sys.stderr, flush=True)
    if check and p.returncode != 0:
        raise RuntimeError(f"Command failed (rc={p.returncode}): {cmd}")
    return p


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def find_assets_dir():
    for cand in glob.glob("/content/drive/MyDrive/**/SHA256SUMS.txt", recursive=True):
        parent = os.path.dirname(cand)
        if os.path.isdir(os.path.join(parent, "PULSAR2")) and os.path.isdir(os.path.join(parent, "B_COLOR")):
            return parent
    raise RuntimeError("Could not find Drive folder containing PULSAR2/ and B_COLOR/ and SHA256SUMS.txt")


def main():
    t0 = time.time()
    report = {"started_at": time.strftime("%Y-%m-%d %H:%M:%S")}

    assets_dir = find_assets_dir()
    report["assets_dir"] = assets_dir
    print("ASSETS_DIR =", assets_dir, flush=True)

    # 1. Install python deps
    run("pip -q install onnx onnxruntime numpy opencv-python-headless")

    # 2. Clone & align repo
    if not os.path.exists(REPO_DIR):
        run(f"git clone https://github.com/dehai168/ax630c-plate-face-demo.git {REPO_DIR}")
    run("git fetch --all --tags", cwd=REPO_DIR)
    run('git config user.name "v25-rc-builder"', cwd=REPO_DIR)
    run('git config user.email "v25-rc@local"', cwd=REPO_DIR)
    run("git checkout -B v2.5-rc origin/cursor/v25-c-integration-audit-50d6", cwd=REPO_DIR)
    run("git merge origin/cursor/mixed-finetune-onnx-b3c5 --no-edit", cwd=REPO_DIR)

    rec_sha = sha256(os.path.join(REPO_DIR, "models/onnx/plate_rec_finetune.onnx"))
    col_sha = sha256(os.path.join(REPO_DIR, "models/onnx/plate_color_finetune.onnx"))
    report["rec_onnx_sha256"] = rec_sha
    report["color_onnx_sha256"] = col_sha
    assert rec_sha == "45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1", f"Bad rec sha: {rec_sha}"
    assert col_sha == "ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360", f"Bad col sha: {col_sha}"

    # 3. Extract A_OCR, B_COLOR, C_OPTIONAL into /content/v25_work/extracted
    ext_dir = os.path.join(WORK_DIR, "extracted")
    os.makedirs(ext_dir, exist_ok=True)
    a_tar = os.path.join(assets_dir, "A_OCR/A_OCR_REQUIRED.tar.gz")
    b_tar = os.path.join(assets_dir, "B_COLOR/B_COLOR_SCRIPTS_MODELS_CAL64REF.tar.gz")
    c_tar = os.path.join(assets_dir, "C_OPTIONAL/3rdparty_aarch64_opencv_ffmpeg.tar.gz")

    os.makedirs(os.path.join(ext_dir, "A_OCR"), exist_ok=True)
    os.makedirs(os.path.join(ext_dir, "B_COLOR"), exist_ok=True)
    if not os.listdir(os.path.join(ext_dir, "A_OCR")):
        run(f'tar -xzf "{a_tar}" -C "{ext_dir}/A_OCR"')
    if not os.listdir(os.path.join(ext_dir, "B_COLOR")):
        run(f'tar -xzf "{b_tar}" -C "{ext_dir}/B_COLOR"')
    if not os.path.exists(os.path.join(REPO_DIR, "3rdparty/aarch64")):
        run(f'tar -xzf "{c_tar}" -C "{REPO_DIR}"')

    # Inspect extracted B_COLOR configs
    b_files = []
    for r, ds, fs in os.walk(os.path.join(ext_dir, "B_COLOR")):
        for f in sorted(fs):
            rel = os.path.relpath(os.path.join(r, f), os.path.join(ext_dir, "B_COLOR"))
            if not rel.endswith(".bin"):
                b_files.append(rel)
    report["b_color_files"] = b_files

    for cfg_name in ["control.json", "run_config.json", "list.txt"]:
        hits = glob.glob(os.path.join(ext_dir, "B_COLOR", "**", cfg_name), recursive=True)
        if hits:
            txt = open(hits[0], encoding="utf-8", errors="replace").read()
            report[f"b_cfg_{cfg_name}"] = txt[:4000]
            shutil.copy2(hits[0], os.path.join(LOG_DIR, f"r84_{cfg_name}"))

    # 4. Reassemble & unpack Pulsar2 8.0 LITE first (fast: 830MB)
    p2_tar_dir = os.path.join(WORK_DIR, "pulsar2_tars")
    p2_lite_dir = os.path.join(WORK_DIR, "pulsar2_lite")
    os.makedirs(p2_tar_dir, exist_ok=True)
    os.makedirs(p2_lite_dir, exist_ok=True)
    reassemble = os.path.join(assets_dir, "PULSAR2/reassemble_pulsar2.py")
    lite_tar = os.path.join(p2_tar_dir, "ax_pulsar2_8.0_lite_package.tar.gz")
    if not os.path.exists(lite_tar):
        run(f'python3 "{reassemble}" --parts-dir "{assets_dir}/PULSAR2/LITE_PARTS" --out "{lite_tar}"', check=False)
        if not os.path.exists(lite_tar):
            # Fallback: inspect script args or simple cat
            parts = sorted(glob.glob(os.path.join(assets_dir, "PULSAR2/LITE_PARTS/ax_pulsar2_8.0_lite_package.tar.gz.part*")))
            print(f"Concatenating {len(parts)} lite parts -> {lite_tar}", flush=True)
            with open(lite_tar, "wb") as out_f:
                for pt in parts:
                    with open(pt, "rb") as in_f:
                        shutil.copyfileobj(in_f, out_f, 1 << 20)
    lite_sha = sha256(lite_tar)
    report["pulsar2_lite_tar_sha256"] = lite_sha
    assert lite_sha == "0c4409aa4f6433e830281b5cb80cf75a31bec9c755c5c4ed47cd57d3fe10f11c", f"Bad lite sha: {lite_sha}"

    if not os.listdir(p2_lite_dir):
        run(f'tar -xzf "{lite_tar}" -C "{p2_lite_dir}"')

    bins = [b for b in glob.glob(os.path.join(p2_lite_dir, "**", "pulsar2"), recursive=True) if os.path.isfile(b) and os.access(b, os.X_OK)]
    report["pulsar2_lite_bins"] = bins
    if bins:
        p = run(f'"{bins[0]}" version', check=False)
        report["pulsar2_version_out"] = (p.stdout + "\n" + p.stderr).strip()
        p2 = run(f'"{bins[0]}" build --help', check=False)
        report["pulsar2_build_help_head"] = (p2.stdout + "\n" + p2.stderr)[:1500]

    report["elapsed_sec"] = round(time.time() - t0, 1)
    out_path = os.path.join(LOG_DIR, "step1_report.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print("STEP 1 DONE! Report saved to:", out_path, flush=True)


if __name__ == "__main__":
    main()
