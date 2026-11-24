#!/usr/bin/env python3
"""
End-to-end self-healing v2.6 Colab pipeline:
1. Restore Pulsar2 8.0 Lite, gcc-arm-9.2, ax620e_bsp_sdk, 3rdparty/aarch64 if needed.
2. Train ColorV3-ReLU6 (fast 4x-downsampled ReLU6 CNN + BN-folded S8 QAT, ~8-10 min on CPU or ~40s on GPU).
3. Quantize ColorV3-ReLU6 with Pulsar2 8.0 (U16+S8, compiler.check=1) for NPU1 & NPU2.
4. Verify Holdout-64 numerical & accuracy gate against untouched float ONNX.
5. Cross-compile v2.6-rc1 (C++ Prefix Beam + default ColorV3-ReLU6) and package board tarball to Drive.
"""
import glob
import hashlib
import json
import os
import shutil
import subprocess
import tarfile

import cv2
import numpy as np
import onnxruntime as ort

P = "/content/drive/MyDrive/ax630c_v25_rc_persist"
V26 = f"{P}/v26"
ASSETS = "/content/drive/MyDrive/文件中转站/grok bot 文件内容"
PACK = f"{P}/v26_color5_pack.tar"
REPO = "/content/ax630c-plate-face-demo"
WORK = "/content/v25_work"
OUT = "/content/v26_work/color_v3"


def sha256(fp: str) -> str:
    h = hashlib.sha256()
    with open(fp, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def ensure_pulsar2() -> str:
    p2 = f"{WORK}/pulsar2_lite/ax_pulsar2_8.0_lite_package/bin/pulsar2"
    if not os.path.exists(p2):
        os.makedirs(f"{WORK}/pulsar2_tars", exist_ok=True)
        os.makedirs(f"{WORK}/pulsar2_lite", exist_ok=True)
        ltar = f"{WORK}/pulsar2_tars/ax_pulsar2_8.0_lite_package.tar.gz"
        if not os.path.exists(ltar):
            parts = sorted(glob.glob(f"{ASSETS}/PULSAR2/LITE_PARTS/ax_pulsar2_8.0_lite_package.tar.gz.part*"))
            assert parts, f"Missing Pulsar2 LITE_PARTS under {ASSETS}"
            with open(ltar, "wb") as out_f:
                for pt in parts:
                    with open(pt, "rb") as in_f:
                        shutil.copyfileobj(in_f, out_f, 1 << 20)
        subprocess.run(f'tar -xzf "{ltar}" -C "{WORK}/pulsar2_lite"', shell=True, check=True)
        txt = (
            open(p2)
            .read()
            .replace("/pulsar2/axnn/axnn/yamain/main.py", "/pulsar2/axnn/yamain/main.py")
            .replace("/pulsar2/axnn/axnn/tools", "/pulsar2/axnn/tools")
        )
        open(p2, "w").write(txt)
        os.chmod(p2, 0o755)
    return p2


def main():
    os.makedirs(V26, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    assert os.path.exists(PACK), f"Missing {PACK}"

    p2 = ensure_pulsar2()

    # 1. Train ColorV3-ReLU6 + S8 QAT
    pt = subprocess.run(
        [
            "python3",
            f"{REPO}/tools/train_color_v3_relu6.py",
            "--pack",
            PACK,
            "--out-dir",
            OUT,
            "--epochs",
            "40",
            "--qat-epochs",
            "10",
            "--batch",
            "64",
        ],
        capture_output=True,
        text=True,
    )
    open(f"{V26}/train_color_v3.log", "w").write(pt.stdout + "\n" + pt.stderr)
    print(pt.stdout, flush=True)
    if pt.returncode != 0:
        print("TRAIN ERR:", pt.stderr[-1500:], flush=True)
        raise RuntimeError("Training failed")

    onnx_p = f"{OUT}/plate_color_v3_relu6.onnx"
    shutil.copy2(onnx_p, f"{V26}/plate_color_v3_relu6.onnx")
    shutil.copy2(f"{OUT}/color_v3_metrics.json", f"{V26}/color_v3_metrics.json")

    # 2. Pulsar2 8.0 U16+S8 NPU1 & NPU2
    with tarfile.open(PACK, "r") as tf:
        tf.extract("calib_v26_color5.tar", OUT)
    calib_tar = f"{OUT}/calib_v26_color5.tar"

    for npu in ("NPU1", "NPU2"):
        bdir = f"{OUT}/build_{npu.lower()}"
        if os.path.exists(bdir):
            shutil.rmtree(bdir)
        os.makedirs(bdir, exist_ok=True)
        cfg = {
            "model_type": "ONNX",
            "npu_mode": npu,
            "quant": {
                "input_configs": [
                    {
                        "tensor_name": "data",
                        "calibration_dataset": calib_tar,
                        "calibration_format": "Image",
                        "calibration_size": 400,
                        "calibration_mean": [0, 0, 0],
                        "calibration_std": [255, 255, 255],
                    }
                ],
                "calibration_method": "MinMax",
                "precision_analysis": True,
                "precision_analysis_method": "EndToEnd",
                "layer_configs": [
                    {"start_tensor_names": ["DEFAULT"], "end_tensor_names": ["DEFAULT"], "data_type": "U16"}
                ],
            },
            "input_processors": [
                {
                    "tensor_name": "data",
                    "tensor_format": "BGR",
                    "src_format": "BGR",
                    "src_dtype": "U8",
                    "src_layout": "NHWC",
                }
            ],
            "compiler": {"check": 1},
        }
        json.dump(cfg, open(f"{bdir}/control.json", "w"), indent=2)
        pb = subprocess.run(
            [p2, "build", "--target_hardware", "AX620E", "--input", onnx_p, "--output_dir", f"{bdir}/out", "--config", f"{bdir}/control.json"],
            capture_output=True,
            text=True,
        )
        open(f"{V26}/color_v3_{npu.lower()}_build.log", "w").write(pb.stdout + "\n" + pb.stderr)
        assert pb.returncode == 0, (pb.stdout + pb.stderr)[-1200:]
        dst = f"{V26}/plate_color5_v26_{npu.lower()}.axmodel"
        shutil.copy2(f"{bdir}/out/compiled.axmodel", dst)
        print(f"PERSISTED {npu}: {sha256(dst)} ({os.path.getsize(dst):,} B)", flush=True)

    # 3. Holdout-64 numerical & accuracy gate
    sess = ort.InferenceSession(onnx_p, providers=["CPUExecutionProvider"])
    in_n = sess.get_inputs()[0].name
    sim_in, sim_out = f"{OUT}/hold64_in", f"{OUT}/hold64_out"
    os.makedirs(sim_in, exist_ok=True)
    os.makedirs(sim_out, exist_ok=True)
    golden, labels, names = {}, {}, []
    with tarfile.open(PACK, "r") as tf:
        manifest = json.loads(tf.extractfile("manifest.json").read().decode("utf-8"))
        by_c = {c: [] for c in range(5)}
        for s in manifest["samples"]:
            if s["split"] == "holdout":
                by_c[int(s["label"])].append(s)
        chosen = [x for c in range(5) for x in by_c[c][:13]][:64]
        for i, s in enumerate(chosen):
            cid = f"H_{i:04d}"
            im = cv2.imdecode(np.frombuffer(tf.extractfile(s["path"]).read(), np.uint8), cv2.IMREAD_COLOR)
            os.makedirs(f"{sim_in}/{cid}", exist_ok=True)
            im.astype(np.uint8).tofile(f"{sim_in}/{cid}/data.bin")
            golden[cid] = sess.run(None, {in_n: im.transpose(2, 0, 1).astype(np.float32)[None] / 255.0})[0].reshape(-1)
            labels[cid] = int(s["label"])
            names.append(cid)
    open(f"{OUT}/hold64_list.txt", "w").write("\n".join(names) + "\n")
    pa = subprocess.run(
        [p2, "run", "--model", f"{V26}/plate_color5_v26_npu1.axmodel", "--input_dir", sim_in, "--output_dir", sim_out, "--list", f"{OUT}/hold64_list.txt"],
        capture_output=True,
        text=True,
    )
    assert pa.returncode == 0, (pa.stdout + pa.stderr)[-1000:]
    max_err, mae_sum, am_match, gt_f, gt_q = 0.0, 0.0, 0, 0, 0
    for cid in names:
        b = [x for x in glob.glob(f"{sim_out}/{cid}/*.bin") if os.path.getsize(x) == 20][0]
        y = np.fromfile(b, dtype=np.float32).reshape(-1)
        g = golden[cid]
        d = np.abs(y - g)
        max_err = max(max_err, float(d.max()))
        mae_sum += float(d.mean())
        am_match += int(np.argmax(y) == np.argmax(g))
        gt_f += int(np.argmax(g) == labels[cid])
        gt_q += int(np.argmax(y) == labels[cid])
    mae = mae_sum / len(names)
    gate = {
        "n": len(names),
        "max": max_err,
        "mae": mae,
        "argmax_parity": f"{am_match}/{len(names)}",
        "gt_float": f"{gt_f}/{len(names)}",
        "gt_axm": f"{gt_q}/{len(names)}",
        "PASS_GATE": (max_err <= 0.03 and mae <= 0.005 and am_match == len(names)),
    }
    json.dump(gate, open(f"{V26}/color_v3_holdout64_gate.json", "w"), indent=2)
    print("=== HOLDOUT-64 GATE ===\n", json.dumps(gate, indent=2), flush=True)

    # 4. Cross-compile v2.6-rc1 & package
    tc = "/content/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu"
    bsp = "/content/ax620e_bsp_sdk"
    if not os.path.exists(tc):
        subprocess.run(
            "wget -q https://developer.arm.com/-/media/Files/downloads/gnu-a/9.2-2019.12/binrel/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu.tar.xz -O /content/gcc92.tar.xz && tar -xf /content/gcc92.tar.xz -C /content",
            shell=True,
            check=True,
        )
    if not os.path.exists(bsp):
        subprocess.run(f"git clone --depth 1 https://github.com/AXERA-TECH/ax620e_bsp_sdk.git {bsp}", shell=True, check=True)
    if not os.path.exists(f"{REPO}/3rdparty/aarch64"):
        subprocess.run(f'tar -xzf "{ASSETS}/C_OPTIONAL/3rdparty_aarch64_opencv_ffmpeg.tar.gz" -C "{REPO}"', shell=True, check=True)
    os.makedirs("/workspace", exist_ok=True)
    if not os.path.exists("/workspace/ax630c_plate_face_demo"):
        os.symlink(REPO, "/workspace/ax630c_plate_face_demo")
    shutil.copy2(f"{P}/models/plate_rec_v25_npu1.axmodel", f"{REPO}/models/axmodel/plate_rec_npu1.axmodel")
    shutil.copy2(f"{P}/models/plate_rec_v25_npu2.axmodel", f"{REPO}/models/axmodel/plate_rec_npu2.axmodel")
    shutil.copy2(f"{V26}/plate_color5_v26_npu1.axmodel", f"{REPO}/models/axmodel/plate_color5_npu1.axmodel")
    shutil.copy2(f"{V26}/plate_color5_v26_npu2.axmodel", f"{REPO}/models/axmodel/plate_color5_npu2.axmodel")
    env = os.environ.copy()
    env["TOOLCHAIN_DIR"] = tc
    env["BSP_MSP_DIR"] = f"{bsp}/msp/out/arm64_glibc"
    env["OUTDIR"] = V26
    subprocess.run(["bash", "build_aarch64.sh"], cwd=REPO, env=env, check=True)
    subprocess.run(["bash", "scripts/package.sh"], cwd=REPO, env=env, check=True)
    print("=== v2.6-rc1 SHA256SUMS ===\n" + open(f"{V26}/ax630c_plate_face_demo_v2.6-rc1_SHA256SUMS.txt").read(), flush=True)


if __name__ == "__main__":
    main()
