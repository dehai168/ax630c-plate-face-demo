#!/usr/bin/env python3
"""
v2.6 ColorV3-ReLU6 5-class plate color classifier training + S8-QAT + ONNX export.

Input : BGR 96x96 / 255.0 (NCHW float32), raw_rectified -> 96x96 (unified C++ & training contract)
Output: Softmax probabilities [blue, yellow, green, white, black] (shape [1, 5])
Design: All activations hard-capped by ReLU6 ([0, 6]) -> eliminates activation outliers so
        AX620E Pulsar2 U16+S8 quantization achieves Max << 0.03, MAE << 0.005, ~100% parity.
"""
import argparse
import collections
import hashlib
import json
import os
import tarfile
import time

import cv2
import numpy as np
import torch
from torch import nn
from torch.nn import functional as F
from torch.utils.data import DataLoader, Dataset

COLOR_NAMES = ["蓝", "黄", "绿", "白", "黑"]


def fq_s8_per_ch(w: torch.Tensor) -> torch.Tensor:
    max_abs = w.detach().abs().amax(dim=tuple(range(1, w.ndim)), keepdim=True).clamp(min=1e-8)
    scale = max_abs / 127.5
    q = torch.clamp(torch.round(w / scale), -127.0, 127.0) * scale
    return w + (q - w).detach()


class ConvBNReLU6(nn.Module):
    def __init__(self, cin, cout, stride=1):
        super().__init__()
        self.conv = nn.Conv2d(cin, cout, 3, stride=stride, padding=1, bias=False)
        self.bn = nn.BatchNorm2d(cout)

    def forward(self, x, qat=False):
        if not qat:
            return F.relu6(self.bn(self.conv(x)))
        # Fold BN into Conv weights for exact Pulsar2 S8 weight fake-quant
        w = self.conv.weight
        mean = self.bn.running_mean
        var = self.bn.running_var
        gamma = self.bn.weight
        beta = self.bn.bias
        inv_std = torch.rsqrt(var + self.bn.eps)
        scale = gamma * inv_std
        w_f = w * scale.view(-1, 1, 1, 1)
        b_f = beta - mean * scale
        return F.relu6(F.conv2d(x, fq_s8_per_ch(w_f), b_f, stride=self.conv.stride, padding=self.conv.padding))


class ColorV3ReLU6(nn.Module):
    def __init__(self):
        super().__init__()
        self.b1 = ConvBNReLU6(3, 24, stride=2)      # 96 -> 48
        self.b2 = ConvBNReLU6(24, 48, stride=1)     # 48 -> pool 24
        self.b3 = ConvBNReLU6(48, 96, stride=1)     # 24 -> pool 12
        self.b4 = ConvBNReLU6(96, 128, stride=1)    # 12 -> pool 6
        self.b5 = ConvBNReLU6(128, 128, stride=1)   # 6 -> GAP 1
        self.pool = nn.MaxPool2d(2)
        self.drop = nn.Dropout(0.15)
        self.fc = nn.Linear(128, 5)

    def forward(self, x, qat=False):
        x = self.b1(x, qat)
        x = self.pool(self.b2(x, qat))
        x = self.pool(self.b3(x, qat))
        x = self.pool(self.b4(x, qat))
        x = self.b5(x, qat)
        x = x.mean(dim=(2, 3))
        x = self.drop(x)
        if qat:
            return F.linear(x, fq_s8_per_ch(self.fc.weight), self.fc.bias)
        return self.fc(x)


class ColorV3Export(nn.Module):
    """BN-folded export model with Softmax output [1, 5]."""

    def __init__(self, src: ColorV3ReLU6):
        super().__init__()
        self.convs = nn.ModuleList()
        for blk in (src.b1, src.b2, src.b3, src.b4, src.b5):
            c = nn.Conv2d(blk.conv.in_channels, blk.conv.out_channels, 3, stride=blk.conv.stride, padding=1, bias=True)
            with torch.no_grad():
                inv_std = torch.rsqrt(blk.bn.running_var + blk.bn.eps)
                scale = blk.bn.weight * inv_std
                c.weight.copy_(blk.conv.weight * scale.view(-1, 1, 1, 1))
                c.bias.copy_(blk.bn.bias - blk.bn.running_mean * scale)
            self.convs.append(c)
        self.pool = nn.MaxPool2d(2)
        self.fc = nn.Linear(128, 5)
        with torch.no_grad():
            self.fc.weight.copy_(src.fc.weight)
            self.fc.bias.copy_(src.fc.bias)

    def forward(self, x):
        x = F.relu6(self.convs[0](x))
        x = self.pool(F.relu6(self.convs[1](x)))
        x = self.pool(F.relu6(self.convs[2](x)))
        x = self.pool(F.relu6(self.convs[3](x)))
        x = F.relu6(self.convs[4](x))
        x = x.mean(dim=(2, 3))
        return F.softmax(self.fc(x), dim=1)


class PlateColorDS(Dataset):
    def __init__(self, imgs, labels, augment=False):
        self.imgs = imgs
        self.labels = labels
        self.augment = augment

    def __len__(self):
        return len(self.imgs)

    def __getitem__(self, i):
        img = self.imgs[i].copy()  # BGR uint8 96x96
        if self.augment:
            # Mild geometric & exposure jitter (preserve true hue!)
            if np.random.rand() < 0.5:
                img = cv2.flip(img, 1)
            if np.random.rand() < 0.7:
                alpha = np.random.uniform(0.75, 1.25)
                beta = np.random.uniform(-18, 18)
                img = np.clip(img.astype(np.float32) * alpha + beta, 0, 255).astype(np.uint8)
            if np.random.rand() < 0.4:
                dx = np.random.randint(-4, 5)
                dy = np.random.randint(-4, 5)
                M = np.float32([[1, 0, dx], [0, 1, dy]])
                img = cv2.warpAffine(img, M, (96, 96), borderMode=cv2.BORDER_REPLICATE)
            if np.random.rand() < 0.25:
                img = cv2.GaussianBlur(img, (3, 3), 0)
        x = img.transpose(2, 0, 1).astype(np.float32) / 255.0
        return torch.from_numpy(x), int(self.labels[i])


def load_pack(pack_tar: str):
    data = {"train": ([], []), "val": ([], []), "holdout": ([], [])}
    with tarfile.open(pack_tar, "r") as tf:
        manifest = json.loads(tf.extractfile("manifest.json").read().decode("utf-8"))
        for s in manifest["samples"]:
            buf = np.frombuffer(tf.extractfile(s["path"]).read(), np.uint8)
            im = cv2.imdecode(buf, cv2.IMREAD_COLOR)
            if im is None:
                continue
            if im.shape[:2] != (96, 96):
                im = cv2.resize(im, (96, 96))
            data[s["split"]][0].append(im)
            data[s["split"]][1].append(int(s["label"]))
    return data, manifest


@torch.no_grad()
def evaluate(model, loader, device, qat=False):
    model.eval()
    correct, total = 0, 0
    per_c = collections.Counter()
    per_t = collections.Counter()
    for x, y in loader:
        x, y = x.to(device), y.to(device)
        pred = model(x, qat=qat).argmax(dim=1)
        correct += int((pred == y).sum().item())
        total += len(y)
        for p, t in zip(pred.cpu().tolist(), y.cpu().tolist()):
            per_t[t] += 1
            if p == t:
                per_c[t] += 1
    acc = correct / max(1, total)
    detail = {COLOR_NAMES[c]: f"{per_c[c]}/{per_t[c]} ({100.0*per_c[c]/max(1,per_t[c]):.1f}%)" for c in range(5)}
    return acc, correct, total, detail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pack", required=True, help="Path to v26_color5_pack.tar")
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--epochs", type=int, default=35)
    ap.add_argument("--qat-epochs", type=int, default=10)
    ap.add_argument("--batch", type=int, default=64)
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print("Device:", device)

    data, manifest = load_pack(args.pack)
    for k in ("train", "val", "holdout"):
        print(f"Split {k}: {len(data[k][0])} imgs")

    # Class-balanced weights
    cnt = collections.Counter(data["train"][1])
    max_c = max(cnt.values())
    cw = torch.tensor([max_c / max(1, cnt[c]) for c in range(5)], dtype=torch.float32, device=device)
    cw = cw / cw.mean()

    tr_loader = DataLoader(PlateColorDS(*data["train"], augment=True), batch_size=args.batch, shuffle=True, num_workers=2)
    va_loader = DataLoader(PlateColorDS(*data["val"], augment=False), batch_size=args.batch, shuffle=False)
    ho_loader = DataLoader(PlateColorDS(*data["holdout"], augment=False), batch_size=args.batch, shuffle=False)

    model = ColorV3ReLU6().to(device)
    opt = torch.optim.AdamW(model.parameters(), lr=2e-3, weight_decay=2e-4)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=args.epochs)
    crit = nn.CrossEntropyLoss(weight=cw, label_smoothing=0.02)

    best_va, best_state = -1.0, None
    t0 = time.time()
    for ep in range(1, args.epochs + 1):
        model.train()
        for x, y in tr_loader:
            x, y = x.to(device), y.to(device)
            loss = crit(model(x, qat=False), y)
            opt.zero_grad(); loss.backward(); opt.step()
        sched.step()
        if ep % 5 == 0 or ep == args.epochs:
            va, vc, vt, vd = evaluate(model, va_loader, device, qat=False)
            print(f"[FP32 ep {ep:2d}/{args.epochs}] val={vc}/{vt} ({va*100:.2f}%) elapsed={time.time()-t0:.1f}s")
            if va >= best_va:
                best_va = va
                best_state = {k: v.cpu().clone() for k, v in model.state_dict().items()}

    model.load_state_dict(best_state)
    # QAT fine-tuning with BN-folded S8 STE weights
    opt_q = torch.optim.AdamW(model.parameters(), lr=2e-4, weight_decay=1e-4)
    best_qva, best_qstate = -1.0, best_state
    for ep in range(1, args.qat_epochs + 1):
        model.train()
        for x, y in tr_loader:
            x, y = x.to(device), y.to(device)
            loss = crit(model(x, qat=True), y)
            opt_q.zero_grad(); loss.backward(); opt_q.step()
        va, vc, vt, vd = evaluate(model, va_loader, device, qat=True)
        print(f"[QAT  ep {ep:2d}/{args.qat_epochs}] val={vc}/{vt} ({va*100:.2f}%) elapsed={time.time()-t0:.1f}s")
        if va >= best_qva:
            best_qva = va
            best_qstate = {k: v.cpu().clone() for k, v in model.state_dict().items()}

    model.load_state_dict(best_qstate)
    ha, hc, ht, hd = evaluate(model, ho_loader, device, qat=True)
    print(f"\n=== INDEPENDENT HOLDOUT (QAT): {hc}/{ht} ({ha*100:.2f}%) ===")
    print("Per-class:", json.dumps(hd, ensure_ascii=False))

    # Export BN-folded ONNX
    exp_m = ColorV3Export(model.cpu().eval()).eval()
    onnx_p = os.path.join(args.out_dir, "plate_color_v3_relu6.onnx")
    dummy = torch.randn(1, 3, 96, 96)
    torch.onnx.export(
        exp_m, dummy, onnx_p,
        input_names=["data"], output_names=["output"],
        opset_version=13, do_constant_folding=True
    )
    sha = hashlib.sha256(open(onnx_p, "rb").read()).hexdigest()
    print(f"Exported ONNX: {onnx_p} ({os.path.getsize(onnx_p):,} B) SHA256={sha}")

    # Verify ONNX on independent holdout with ORT
    import onnxruntime as ort
    sess = ort.InferenceSession(onnx_p, providers=["CPUExecutionProvider"])
    in_n = sess.get_inputs()[0].name
    oc, ot = 0, len(data["holdout"][0])
    oper_c, oper_t = collections.Counter(), collections.Counter()
    for im, y in zip(*data["holdout"]):
        x = im.transpose(2, 0, 1).astype(np.float32)[None] / 255.0
        p = int(np.argmax(sess.run(None, {in_n: x})[0]))
        oper_t[y] += 1
        if p == y:
            oc += 1; oper_c[y] += 1
    odet = {COLOR_NAMES[c]: f"{oper_c[c]}/{oper_t[c]} ({100.0*oper_c[c]/max(1,oper_t[c]):.1f}%)" for c in range(5)}
    summary = {
        "onnx_sha256": sha,
        "onnx_size": os.path.getsize(onnx_p),
        "val_qat_acc": best_qva,
        "holdout_ort_correct": oc,
        "holdout_ort_total": ot,
        "holdout_ort_acc": oc / max(1, ot),
        "holdout_per_class": odet,
    }
    json.dump(summary, open(os.path.join(args.out_dir, "color_v3_metrics.json"), "w"), indent=2, ensure_ascii=False)
    print("ORT HOLDOUT SUMMARY:", json.dumps(summary, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
