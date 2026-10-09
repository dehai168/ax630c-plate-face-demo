#!/usr/bin/env python3
"""生成 scripts/pulsar2_configs/*.json（AX620E, npu1/npu2）
注意: Pulsar2 不会在模型里插入 RGB<->BGR 通道重排（见 Pulsar2 文档「常见配置示例-RGB 输入」），
所以检测模型配置为 src_format=RGB，板端程序在送 NPU 前自己做 BGR->RGB；识别/颜色模型本来就吃 BGR。"""
import json, os
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'pulsar2_configs')
os.makedirs(OUT, exist_ok=True)
def cfg(inp, calib, mean, std, fmt, npu, u16=False):
    q = {"input_configs": [{"tensor_name": inp, "calibration_dataset": calib, "calibration_format": "Image",
                            "calibration_size": -1, "calibration_mean": mean, "calibration_std": std}],
         "calibration_method": "MinMax", "precision_analysis": True, "precision_analysis_method": "EndToEnd"}
    if u16:  # 识别网络含 SVTR(LayerNorm/MatMul/Softmax)，纯 U8 量化输出余弦只有 ~0.88，整网 16bit 激活
        q["layer_configs"] = [{"start_tensor_names": ["DEFAULT"], "end_tensor_names": ["DEFAULT"], "data_type": "U16"}]
    return {"model_type": "ONNX", "npu_mode": npu, "quant": q,
            "input_processors": [{"tensor_name": inp, "tensor_format": fmt, "src_format": fmt, "src_dtype": "U8", "src_layout": "NHWC"}],
            "compiler": {"check": 0}}
M = {
  "face_det":  ("images", "CALIB_DIR/face_calib.tar",      [0, 0, 0], [255., 255., 255.], "RGB", False),
  "plate_det": ("images", "CALIB_DIR/plate_det_calib.tar", [0, 0, 0], [255., 255., 255.], "RGB", False),
  "plate_rec": ("data",   "CALIB_DIR/plate_rec_calib.tar", [127.5] * 3, [127.5] * 3, "BGR", True),
  "plate_cls": ("data",   "CALIB_DIR/plate_cls_calib.tar", [0, 0, 0], [255., 255., 255.], "BGR", False),
}
for n, (inp, cal, m, s, fmt, u16) in M.items():
    for npu in ("NPU1", "NPU2"):
        with open(os.path.join(OUT, f"{n}_{npu.lower()}.json"), "w") as f:
            json.dump(cfg(inp, cal, m, s, fmt, npu, u16), f, indent=2)
print("configs ->", OUT)
