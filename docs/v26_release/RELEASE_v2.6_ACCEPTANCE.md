# AX630C 车牌+人脸 Demo v2.6 正式发布验收记录（2026-11-24）

- 发布版本：`v2.6`（Git tag `v2.6`，合入 `main`；生产 `/opt/demo_v24` 保持零改动、随时可回退）
- Google Drive 持久化目录：`/MyDrive/ax630c_v25_rc_persist/v26/release/`
  - `ax630c_plate_face_demo_v2.6_board.tar.gz`（39,844,413 B，SHA256 `13054f25b999a37483d54719849e93909242599a98657891b650e1c9bd073977`）
  - `ax630c_plate_face_demo_v2.6_src.tar.gz`（104,457,725 B，SHA256 `f6cf4532200a3f9f6d438a40054736c8df96d0f20e19184587bd79e4f5807787`）
  - `ax630c_plate_face_demo_v2.6_SHA256SUMS.txt`

## 1. 核心特性与模型清单

1. **五类颜色 `ColorV3-ReLU6` 默认上线（闭合 B 线量化与 C-IF-004）**
   - 架构：5 层轻量 CNN，全层 `ReLU6` 封顶 `[0, 6]`，统一 `raw_rectified -> 96×96` BGR `/255` 输入，输出 `[蓝, 黄, 绿, 白, 黑]` 概率。
   - 数据：从标注平台 `10.30.25.73`（`cn_plate_special` + `cn_plate_val_su`）导出 5,266 张真实车牌（`train` 4,217 / `val` 524 / 独立 `holdout` 525；校准集 `calib_v26_color5.tar` 仅取自 `train` 400 张分层采样）。
   - ONNX：`plate_color_v3_relu6.onnx`（1,249,977 B，SHA256 `e9c9bd2dec1ce5a4276d9eff2e287ef29a8c96743f64be58d8de48fa07739e55`）。
   - Pulsar2 8.0 `U16+S8`（`compiler.check=1` PASS）：
     - `plate_color5_npu1.axmodel`（336,346 B，SHA256 `b7088a3e7699f71522cd10edd0ded4343fb196651fe28339ac40d054f590ab13`）
     - `plate_color5_npu2.axmodel`（333,794 B，SHA256 `9c3e90c529658505dee291e637751127821f3a6bf7e2e4b43facde2eb740d0ab`）
   - **独立 Holdout-64 数值门禁**：`Max = 0.01534`（≤ 0.030）、`MAE = 0.000690`（≤ 0.005）、`argmax` 一致率 **`64/64`（100%）** → **PASS**。
   - 独立 525 张真实 holdout 纯网络五类：白 `97.8%`（136/139）、黑 `94.9%`（94/99）、绿 `92.4%`、蓝 `92.1%`、黄 `83.6%`。
2. **C++ CTC Prefix Beam 解码**
   - `src/plate_rec.cpp` 实现 `decode_ctc_prefix_beam`（`beam=24`、`top_k=5`、重复字符须经 blank），`board/demo.cfg` 默认 `rec_prefix_beam 1`，零额外 NPU 调用。
3. **规则与运行脚本**
   - `src/color5_crop_policy.inc` 不再覆盖合法白/黑预测；`board/run.sh` 默认加载 `models/plate_color5_${NPU}.axmodel`（可通过 `COLOR5_MODEL=none` 回退旧三类）。

## 2. AX630C 物理板端实测验收（2026-11-24）

测试环境：AX630C（Linux 4.19.125 aarch64，AX Engine `2.10.1s`，`PFD_VNPU=1`，独立目录 `/opt/demo_v26_rc`，生产 `/opt/demo_v24` 零改动）。

- **加载模型**：`face_det_npu1`、`plate_det_npu1_u16`、`plate_rec_npu1`（v2.5 U16）、`plate_cls_npu1`、`plate_color5_npu1`（ColorV3-ReLU6）全部正常加载。
- **完整 720 帧真实视频（`test_video.mp4`，1280×720 @ 15fps，48s）**：
  - 退出码：**`VIDEO_EXIT=0`**，无崩溃、无 OOM（剩余可用内存 113 MB，ext4 `errors_count` 零新增）。
  - 输出：`out_v26.mp4`（31.6 MB）、`out_v26_plates.json`（1.8 KB，5 块蓝牌均标明 `"color_src": "color5"`，5 块绿牌新能源 `"color_src": "rule"`）、59 张人脸抓拍 + 10 张车牌抓拍。
  - 平均吞吐：算法 FPS **`10.73`**，端到端 FPS **`4.95`**。
  - 逐模块平均耗时（n=720）：`face_infer` 32.85 ms、`plate_infer` 27.80 ms、`plate_rec`（含 C++ Prefix Beam）**6.09 ms/帧**、`plate_color`（含 ColorV3-ReLU6）**0.67 ms/帧**。
