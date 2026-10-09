#!/usr/bin/env python3
"""把 deepcam-cn/yolov5-face 的 yolov5n-face.pt 导出成「3 个原始检测头」的 ONNX（不在图里做 decode），
便于 Pulsar2 INT8 量化（decode 后的坐标/概率混在一个张量里会严重掉点）。

输出: face_det_yolov5n_<H>x<W>.onnx, 输入 images[1,3,H,W] (RGB, 0~1)，
      输出 3 个 [1, 3*16, H/s, W/s] (s=8,16,32)，16 = 4(box)+1(obj)+10(5 关键点)+1(cls)

用法:
  git clone https://github.com/deepcam-cn/yolov5-face.git
  # 权重: README 中 yolov5n-face 的 Google Drive 链接 (id 18oenL6tjFkdR1f5IgpYeQfDFqU4w3jEr)
  python export_face_onnx.py --repo yolov5-face --weights yolov5n-face.pt --h 384 --w 640 --out ../models/onnx
"""
import argparse, os, sys
import torch, torch.nn as nn

ap = argparse.ArgumentParser()
ap.add_argument('--repo', required=True)
ap.add_argument('--weights', required=True)
ap.add_argument('--h', type=int, default=384)
ap.add_argument('--w', type=int, default=640)
ap.add_argument('--out', default='.')
a = ap.parse_args()
sys.path.insert(0, os.path.abspath(a.repo))

_orig_load = torch.load
torch.load = lambda *k, **kw: _orig_load(*k, **{**kw, 'weights_only': False})  # torch>=2.6 兼容
import models  # noqa
from models.experimental import attempt_load
from utils.activations import SiLU, Hardswish

model = attempt_load(a.weights, map_location=torch.device('cpu'))
model.eval()
for k, m in model.named_modules():
    m._non_persistent_buffers_set = set()
    if isinstance(m, models.common.Conv):
        if isinstance(m.act, nn.Hardswish): m.act = Hardswish()
        elif isinstance(m.act, nn.SiLU): m.act = SiLU()
    if isinstance(m, models.common.ShuffleV2Block):
        for br in (m.branch1, m.branch2):
            for i in range(len(br)):
                if isinstance(br[i], nn.SiLU): br[i] = SiLU()

det = model.model[-1]
def raw_forward(self, x):
    return [self.m[i](x[i]) for i in range(self.nl)]
det.forward = raw_forward.__get__(det, type(det))
print('anchors(px):', (det.anchor_grid if hasattr(det, 'anchor_grid') else det.anchors).flatten().tolist() if hasattr(det,'anchors') else '')
print('stride:', det.stride.tolist(), 'no:', det.no, 'na:', det.na)

os.makedirs(a.out, exist_ok=True)
f = os.path.join(a.out, f'face_det_yolov5n_{a.h}x{a.w}.onnx')
img = torch.zeros(1, 3, a.h, a.w)
torch.onnx.export(model, img, f, opset_version=13, input_names=['images'],
                  output_names=['p8', 'p16', 'p32'], do_constant_folding=True, dynamo=False)
import onnx
from onnxsim import simplify
m, ok = simplify(onnx.load(f)); assert ok
onnx.save(m, f)
print('saved', f)
