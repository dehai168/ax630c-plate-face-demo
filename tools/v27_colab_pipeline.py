#!/usr/bin/env python3
"""
v2.7 Colab Pipeline:
1. Mount Drive, locate Pulsar2 8.0 Lite and v26_color5_pack.tar + v27_synth.tar.
2. Fine-tune plate_rec_finetune.onnx -> plate_rec_v27.onnx (31-province balanced).
3. Quantize plate_rec_v27.onnx -> plate_rec_v27_npu1/2.axmodel (Pulsar2 8.0, U16 activation, compiler.check=1).
4. Persist models & report to /content/drive/MyDrive/ax630c_v25_rc_persist/v27/.
"""
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import time

PERSIST_DIR = "/content/drive/MyDrive/ax630c_v25_rc_persist"
V27_DRIVE = os.path.join(PERSIST_DIR, "v27")
WORK_DIR = "/content/v27_work"
REPO_DIR = "/content/ax630c-plate-face-demo"
os.makedirs(V27_DRIVE, exist_ok=True)
os.makedirs(WORK_DIR, exist_ok=True)


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


def find_file(name):
    cands = glob.glob(f"/content/drive/MyDrive/**/{name}", recursive=True)
    if not cands:
        raise FileNotFoundError(f"Missing {name} on Drive")
    return cands[0]


def main():
    t0 = time.time()
    run("pip -q install onnx onnxruntime onnx2torch opencv-python-headless")
    if not os.path.exists(REPO_DIR):
        run(f"git clone -b v2.7-dev https://github.com/dehai168/ax630c-plate-face-demo.git {REPO_DIR}")
    else:
        run("git fetch --all --tags && git checkout -B v2.7-dev origin/v2.7-dev", cwd=REPO_DIR)

    # 1. Extract real crops (v26_color5_pack.tar) and synth crops (v27_synth.tar)
    real_dir = os.path.join(WORK_DIR, "real")
    synth_dir = os.path.join(WORK_DIR, "synth")
    os.makedirs(real_dir, exist_ok=True)
    os.makedirs(synth_dir, exist_ok=True)
    if not os.path.exists(os.path.join(real_dir, "manifest.json")):
        real_tar = find_file("v26_color5_pack.tar")
        run(f'tar -xf "{real_tar}" -C "{real_dir}"')
    if not os.path.exists(os.path.join(synth_dir, "synth_manifest.json")):
        syn_tar = find_file("v27_synth.tar")
        run(f'tar -xf "{syn_tar}" -C "{synth_dir}"')

    # 2. Train plate_rec_v27.onnx
    out_onnx = os.path.join(WORK_DIR, "plate_rec_v27.onnx")
    run(
        f'python3 "{REPO_DIR}/tools/train_rec_v27.py" '
        f'--base_onnx "{REPO_DIR}/models/onnx/plate_rec_finetune.onnx" '
        f'--real_dir "{real_dir}" --synth_dir "{synth_dir}" '
        f'--out_onnx "{out_onnx}" --epochs 15 --batch_size 128 --lr 2e-4'
    )
    shutil.copy2(out_onnx, os.path.join(V27_DRIVE, "plate_rec_v27.onnx"))

    # 3. Prepare 64-image calibration tar (160x48 BGR) for Pulsar2 8.0
    calib_tar = os.path.join(WORK_DIR, "rec_calib64.tar")
    syn_imgs = sorted(glob.glob(os.path.join(synth_dir, "imgs", "*.jpg")))[:64]
    with tarfile.open(calib_tar, "w") as tf:
        for p in syn_imgs:
            tf.add(p, arcname=os.path.basename(p))

    # 4. Unpack Pulsar2 8.0 Lite and patch bin/pulsar2 if needed
    p2_dir = os.path.join(WORK_DIR, "pulsar2")
    if not os.path.exists(p2_dir):
        os.makedirs(p2_dir, exist_ok=True)
        lite_tar = os.path.join(WORK_DIR, "pulsar2_lite.tar.gz")
        parts = sorted(glob.glob("/content/drive/MyDrive/**/ax_pulsar2_8.0_lite_package.tar.gz.part*", recursive=True))
        if parts:
            with open(lite_tar, "wb") as out_f:
                for pt in parts:
                    with open(pt, "rb") as in_f:
                        shutil.copyfileobj(in_f, out_f, 1 << 20)
        else:
            lite_tar = find_file("ax_pulsar2_8.0_lite_package.tar.gz")
        run(f'tar -xzf "{lite_tar}" -C "{p2_dir}"')
    p2_bins = glob.glob(os.path.join(p2_dir, "**", "bin", "pulsar2"), recursive=True)
    p2_bin = p2_bins[0]
    txt = open(p2_bin, encoding="utf-8", errors="ignore").read()
    if "/axnn/axnn/yamain/main.py" in txt:
        open(p2_bin, "w", encoding="utf-8").write(txt.replace("/axnn/axnn/yamain/main.py", "/axnn/yamain/main.py"))

    # 5. Quantize NPU1 and NPU2 with U16 activation
    report = {"onnx_sha256": sha256(out_onnx), "elapsed_sec": 0}
    for npu in ("NPU1", "NPU2"):
        cfg_path = os.path.join(WORK_DIR, f"rec_{npu.lower()}.json")
        out_dir = os.path.join(WORK_DIR, f"out_{npu.lower()}")
        out_ax = os.path.join(WORK_DIR, f"plate_rec_v27_{npu.lower()}.axmodel")
        cfg = {
            "model_type": "ONNX",
            "npu_mode": npu,
            "quant": {
                "input_configs": [{
                    "tensor_name": "data",
                    "calibration_dataset": calib_tar,
                    "calibration_format": "Image",
                    "calibration_size": -1,
                    "calibration_mean": [127.5, 127.5, 127.5],
                    "calibration_std": [127.5, 127.5, 127.5],
                }],
                "calibration_method": "MinMax",
                "precision_analysis": True,
                "precision_analysis_method": "EndToEnd",
                "layer_configs": [{"start_tensor_names": ["DEFAULT"], "end_tensor_names": ["DEFAULT"], "data_type": "U16"}],
            },
            "input_processors": [{
                "tensor_name": "data",
                "tensor_format": "BGR",
                "src_format": "BGR",
                "src_dtype": "U8",
                "src_layout": "NHWC",
            }],
            "compiler": {"check": 1},
        }
        json.dump(cfg, open(cfg_path, "w"), indent=2)
        run(f'"{p2_bin}" build --input "{out_onnx}" --config "{cfg_path}" --output_dir "{out_dir}" --output_name "plate_rec_v27_{npu.lower()}.axmodel"')
        shutil.copy2(out_ax, os.path.join(V27_DRIVE, os.path.basename(out_ax)))
        report[f"{npu.lower()}_bytes"] = os.path.getsize(out_ax)
        report[f"{npu.lower()}_sha256"] = sha256(out_ax)

    report["elapsed_sec"] = round(time.time() - t0, 1)
    json.dump(report, open(os.path.join(V27_DRIVE, "v27_ocr_report.json"), "w"), indent=2)
    print("V27 COLAB PIPELINE DONE!", json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
