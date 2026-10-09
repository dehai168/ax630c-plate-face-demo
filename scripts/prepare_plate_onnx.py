#!/usr/bin/env python3
"""准备车牌三件套 ONNX（来源 HyperLPR3, Apache-2.0, 模型包 20230229）:
  1) 检测 y5fu_640x_sim.onnx -> 在 3 个检测头 Conv 处截断（去掉图内 decode），并把输入改成 [1,3,H,W]（默认 384x640）
     输出 [1,45,H/s,W/s]，45 = 3 anchors x 15 (4 box + 1 obj + 8 = 4 角点 + 2 cls 单/双层)
  2) 识别 rpv3_mdict_160_r3.onnx（PP-OCRv3 风格 CTC, 输入 [1,3,48,160] BGR (x-127.5)/127.5, 输出 [1,20,78] 已 softmax）
  3) 颜色/类型分类 litemodel_cls_96x_r1.onnx（输入 [1,3,96,96] BGR /255，输出 3 类: 蓝/绿/黄）
  后两个模型是旧版导出（initializer 也列在 graph.input 里），这里清理掉并 onnxsim，Pulsar2 才能正常解析。

用法:
  curl -L -o 20230229.zip http://hyperlpr.tunm.top/raw/20230229.zip && unzip 20230229.zip
  python prepare_plate_onnx.py --src 20230229/onnx --out ../models/onnx --h 384 --w 640
"""
import argparse, os
import onnx
from onnx import helper, shape_inference
from onnxsim import simplify

ap = argparse.ArgumentParser()
ap.add_argument('--src', required=True)
ap.add_argument('--out', default='.')
ap.add_argument('--h', type=int, default=384)
ap.add_argument('--w', type=int, default=640)
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

def strip_init_inputs(m):
    names = {i.name for i in m.graph.initializer}
    keep = [i for i in m.graph.input if i.name not in names]
    del m.graph.input[:]
    m.graph.input.extend(keep)
    return m

# ---- 1. detector ----
m = onnx.load(os.path.join(a.src, 'y5fu_640x_sim.onnx'))
convs = [n for n in m.graph.node if n.op_type == 'Conv']
init = {i.name: i for i in m.graph.initializer}
heads = [n for n in convs if n.input[1] in init and list(init[n.input[1]].dims)[0] == 45]
assert len(heads) == 3, [n.name for n in heads]
outs = [n.output[0] for n in heads]
ex = onnx.utils.Extractor(m)
sub = ex.extract_model(['input'], outs)
# 改输入尺寸: backbone 是 ShuffleNetV2 风格，channel shuffle 的 Reshape 常量里写死了 640x640 下的特征图尺寸
# (80/40/20)，按 stride 改成新尺寸 (H/s, W/s)；Resize 用的是 scales，不用改
from onnx import numpy_helper
import numpy as np
reshape_shape_names = {n.input[1] for n in sub.graph.node if n.op_type == 'Reshape'}
for ini in sub.graph.initializer:
    if ini.name in reshape_shape_names:
        arr = numpy_helper.to_array(ini).copy()
        if arr.ndim == 1 and len(arr) >= 4 and arr[-1] == arr[-2] and arr[-1] in (80, 40, 20):
            s_ = 640 // int(arr[-1])
            arr[-2], arr[-1] = a.h // s_, a.w // s_
            ini.CopyFrom(numpy_helper.from_array(arr.astype(np.int64), ini.name))
sub.graph.input[0].type.tensor_type.shape.dim[2].dim_value = a.h
sub.graph.input[0].type.tensor_type.shape.dim[3].dim_value = a.w
del sub.graph.value_info[:]
for o in sub.graph.output:
    o.type.tensor_type.ClearField('shape')
for o, nm in zip(sub.graph.output, ['p8', 'p16', 'p32']):
    pass
sub = shape_inference.infer_shapes(sub)
sub, ok = simplify(sub); assert ok
# 重命名输入输出
ren = {'input': 'images', outs[0]: 'p8', outs[1]: 'p16', outs[2]: 'p32'}
for n in sub.graph.node:
    for i, x in enumerate(n.input):
        if x in ren: n.input[i] = ren[x]
    for i, x in enumerate(n.output):
        if x in ren: n.output[i] = ren[x]
for v in list(sub.graph.input) + list(sub.graph.output):
    if v.name in ren: v.name = ren[v.name]
onnx.checker.check_model(sub)
f = os.path.join(a.out, f'plate_det_y5fu_{a.h}x{a.w}.onnx'); onnx.save(sub, f); print('saved', f)

# ---- 2/3. rec & cls ----
for src, dst in [('rpv3_mdict_160_r3.onnx', 'plate_rec_rpv3_48x160.onnx'),
                 ('litemodel_cls_96x_r1.onnx', 'plate_cls_96x96.onnx')]:
    m = strip_init_inputs(onnx.load(os.path.join(a.src, src)))
    m.graph.input[0].name  # 'data'
    m, ok = simplify(m); assert ok
    onnx.checker.check_model(m)
    f = os.path.join(a.out, dst); onnx.save(m, f); print('saved', f)
