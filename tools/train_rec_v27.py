#!/usr/bin/env python3
"""
v2.7 Province-Balanced OCR Fine-Tuning Script (Colab / GPU).
Loads models/onnx/plate_rec_finetune.onnx via onnx2torch, fine-tunes with CTCLoss
using real Label Studio crops (with 京 capped) + balanced synthetic 31-province crops,
and exports models/onnx/plate_rec_v27.onnx (fixed 1x3x48x160 for Pulsar2 8.0 U16).
"""
import argparse
import json
import os
import random
from collections import Counter

import cv2
import numpy as np
import onnx
import onnxruntime as ort
import torch
import torch.nn as nn
import torch.nn.functional as F
from onnx2torch import convert
from torch.utils.data import DataLoader, Dataset, WeightedRandomSampler

TOKENS = [
    "", "'", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "H", "J", "K", "L", "M",
    "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "云", "京", "冀", "吉", "学", "宁", "川", "挂", "新", "晋", "桂", "民",
    "沪", "津", "浙", "渝", "港", "湘", "琼", "甘", "皖", "粤", "航", "苏",
    "蒙", "藏", "警", "豫", "贵", "赣", "辽", "鄂", "闽", "陕", "青", "鲁",
    "黑", "领", "使", "澳",
]
TOK2ID = {t: i for i, t in enumerate(TOKENS) if t}


def encode_text(text):
    ids = []
    for ch in text:
        if ch in TOK2ID:
            ids.append(TOK2ID[ch])
        else:
            return None
    return ids if 5 <= len(ids) <= 9 else None


def greedy_decode(logits):
    # logits: [T, C]
    idx = np.argmax(logits, axis=-1)
    out = []
    prev = -1
    for i in idx:
        if i != 0 and i != prev and i < len(TOKENS):
            out.append(TOKENS[i])
        prev = i
    return "".join(out)


def prep_160x48(bgr, is_double=False):
    if is_double:
        h, w = bgr.shape[:2]
        up = bgr[0 : max(1, h * 5 // 12), :]
        down = bgr[h // 3 :, :]
        up = cv2.resize(up, (down.shape[1], down.shape[0]))
        bgr = cv2.hconcat([up, down])
    h, w = bgr.shape[:2]
    if (h, w) == (48, 160):
        canvas = bgr
    else:
        rw = min(160, max(1, int(np.ceil(48 * (w / max(1, h))))))
        canvas = np.full((48, 160, 3), 128, dtype=np.uint8)
        canvas[:, :rw] = cv2.resize(bgr, (rw, 48))
    return (canvas.astype(np.float32) - 127.5) / 127.5


class PlateOCRDataset(Dataset):
    def __init__(self, samples, augment=False):
        self.samples = samples
        self.augment = augment

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        s = self.samples[idx]
        bgr = cv2.imread(s["abs_path"])
        if bgr is None:
            bgr = np.full((48, 160, 3), 128, dtype=np.uint8)
        if self.augment:
            alpha = random.uniform(0.8, 1.2)
            beta = random.uniform(-15, 15)
            bgr = np.clip(bgr.astype(np.float32) * alpha + beta, 0, 255).astype(np.uint8)
        x = prep_160x48(bgr, is_double=s.get("is_double", False))
        # HWC BGR -> CHW
        x = np.transpose(x, (2, 0, 1)).astype(np.float32)
        return torch.from_numpy(x), torch.tensor(s["ids"], dtype=torch.long), s["text"]


def collate_fn(batch):
    xs, ys, txts = zip(*batch)
    xs = torch.stack(xs, 0)
    lens = torch.tensor([len(y) for y in ys], dtype=torch.long)
    targets = torch.cat(ys, 0)
    return xs, targets, lens, list(txts)


def make_dynamic_batch_onnx(src_onnx, dst_onnx):
    m = onnx.load(src_onnx)
    # Set batch dim of inputs/outputs to dynamic "N"
    for inp in m.graph.input:
        if inp.type.tensor_type.shape.dim:
            inp.type.tensor_type.shape.dim[0].dim_param = "N"
    for out in m.graph.output:
        if out.type.tensor_type.shape.dim:
            out.type.tensor_type.shape.dim[0].dim_param = "N"
    # Find shape inputs of Reshape nodes and replace leading 1 with -1 (if no -1) or 0 (copy input dim0)
    reshape_shape_names = {n.input[1] for n in m.graph.node if n.op_type == "Reshape" and len(n.input) >= 2}
    def patch_arr(arr):
        if arr.ndim == 1 and len(arr) >= 2 and arr[0] == 1:
            arr[0] = 0 if (-1 in arr) else -1
            return True
        return False
    for init in m.graph.initializer:
        if init.name in reshape_shape_names and init.data_type == onnx.TensorProto.INT64:
            arr = onnx.numpy_helper.to_array(init).copy()
            if patch_arr(arr):
                init.CopyFrom(onnx.numpy_helper.from_array(arr, init.name))
    for n in m.graph.node:
        if n.op_type == "Constant" and n.output[0] in reshape_shape_names:
            for attr in n.attribute:
                if attr.name == "value" and attr.t.data_type == onnx.TensorProto.INT64:
                    arr = onnx.numpy_helper.to_array(attr.t).copy()
                    if patch_arr(arr):
                        attr.t.CopyFrom(onnx.numpy_helper.from_array(arr, attr.t.name))
    onnx.save(m, dst_onnx)
    sess = ort.InferenceSession(dst_onnx, providers=["CPUExecutionProvider"])
    y = sess.run(None, {"data": np.zeros((4, 3, 48, 160), dtype=np.float32)})[0]
    print("Dynamic-batch ONNX verified! batch=4 out shape:", y.shape, flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base_onnx", required=True)
    ap.add_argument("--real_dir", required=True)
    ap.add_argument("--synth_dir", required=True)
    ap.add_argument("--out_onnx", required=True)
    ap.add_argument("--epochs", type=int, default=15)
    ap.add_argument("--batch_size", type=int, default=128)
    ap.add_argument("--lr", type=float, default=2e-4)
    ap.add_argument("--max_jing_real", type=int, default=450)
    args = ap.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("Device:", device, flush=True)

    # Load real samples from manifest.json
    real_man = json.load(open(os.path.join(args.real_dir, "manifest.json"), encoding="utf-8"))["samples"]
    train_samples, val_samples = [], []
    jing_cnt = 0
    for s in real_man:
        txt = (s.get("text") or "").strip()
        ids = encode_text(txt)
        if not ids:
            continue
        p = os.path.join(args.real_dir, s["path"])
        if not os.path.exists(p):
            continue
        rec = {"abs_path": p, "text": txt, "ids": ids, "is_double": (s.get("layer") == "double"), "src": "real"}
        if s.get("split") in ("val", "holdout"):
            val_samples.append(rec)
        else:
            if txt[0] == "京":
                jing_cnt += 1
                if jing_cnt > args.max_jing_real:
                    continue
            train_samples.append(rec)

    # Load balanced synthetic samples
    syn_man = json.load(open(os.path.join(args.synth_dir, "synth_manifest.json"), encoding="utf-8"))
    for s in syn_man:
        txt = s["text"].strip()
        ids = encode_text(txt)
        if not ids:
            continue
        p = os.path.join(args.synth_dir, s["path"])
        # Synth images are already 160x48 (with double already merged)
        train_samples.append({"abs_path": p, "text": txt, "ids": ids, "is_double": False, "src": "synth"})

    # Compute province-balanced sampling weights (real gets 2x boost over synth)
    prov_counts = Counter(s["text"][0] for s in train_samples)
    weights = []
    for s in train_samples:
        w = 1.0 / max(1, prov_counts[s["text"][0]])
        if s["src"] == "real":
            w *= 2.0
        weights.append(w)

    print(f"Train={len(train_samples)} (provinces={len(prov_counts)}), Val/Holdout={len(val_samples)}", flush=True)

    tr_ds = PlateOCRDataset(train_samples, augment=True)
    va_ds = PlateOCRDataset(val_samples, augment=False)
    sampler = WeightedRandomSampler(weights, num_samples=len(train_samples), replacement=True)
    tr_dl = DataLoader(tr_ds, batch_size=args.batch_size, sampler=sampler, collate_fn=collate_fn, num_workers=2)
    va_dl = DataLoader(va_ds, batch_size=args.batch_size, shuffle=False, collate_fn=collate_fn, num_workers=2)

    dyn_onnx = os.path.join(os.path.dirname(args.out_onnx), "_dyn_base.onnx")
    make_dynamic_batch_onnx(args.base_onnx, dyn_onnx)
    model = convert(dyn_onnx).to(device)
    # Check output shape on dummy
    with torch.no_grad():
        dummy = torch.zeros(1, 3, 48, 160, device=device)
        out_d = model(dummy)
        print("Model output shape:", tuple(out_d.shape), flush=True)

    ctc = nn.CTCLoss(blank=0, zero_infinity=True)
    opt = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=1e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=args.epochs)

    best_acc = -1.0
    best_state = None
    for ep in range(1, args.epochs + 1):
        model.train()
        tot_loss = 0.0
        for xs, targets, lens, _ in tr_dl:
            xs, targets, lens = xs.to(device), targets.to(device), lens.to(device)
            logits = model(xs)  # [B, T, C]
            # If model already outputs softmax probs, convert safely to log_probs
            if logits.min() >= 0.0 and torch.allclose(logits.sum(dim=-1), torch.ones_like(logits[..., 0]), atol=1e-2):
                log_p = torch.log(logits.clamp(min=1e-7))
            else:
                log_p = F.log_softmax(logits, dim=-1)
            log_p_tbc = log_p.permute(1, 0, 2)  # [T, B, C]
            in_lens = torch.full((xs.size(0),), log_p.size(1), dtype=torch.long, device=device)
            loss = ctc(log_p_tbc, targets, in_lens, lens)
            opt.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(model.parameters(), 5.0)
            opt.step()
            tot_loss += loss.item()
        sched.step()

        model.eval()
        ok = n = 0
        with torch.no_grad():
            for xs, _, _, txts in va_dl:
                out = model(xs.to(device)).cpu().numpy()
                for i, gt in enumerate(txts):
                    pred = greedy_decode(out[i])
                    ok += int(pred == gt)
                    n += 1
        acc = ok / max(1, n)
        print(f"Epoch {ep:02d}/{args.epochs} loss={tot_loss/len(tr_dl):.4f} val_exact={ok}/{n} ({acc*100:.2f}%)", flush=True)
        if acc >= best_acc:
            best_acc = acc
            best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}

    if best_state is not None:
        model.load_state_dict(best_state)
    model.eval().cpu()
    dummy_cpu = torch.zeros(1, 3, 48, 160)
    torch.onnx.export(
        model, dummy_cpu, args.out_onnx,
        input_names=["data"], output_names=["output"],
        opset_version=13, do_constant_folding=True, dynamo=False,
    )
    sess = ort.InferenceSession(args.out_onnx, providers=["CPUExecutionProvider"])
    y = sess.run(None, {"data": dummy_cpu.numpy()})[0]
    print(f"Exported {args.out_onnx} OK! ORT out shape={y.shape}, best_val_acc={best_acc*100:.2f}%", flush=True)


if __name__ == "__main__":
    main()
