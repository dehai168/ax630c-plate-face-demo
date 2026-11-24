# v2.5-rc Colab 执行与持久化汇总（2026-11-24）

- 分支：`v2.5-rc`（非生产；`main` 与生产 `v2.4` 未改动）
- Colab Notebook：`https://colab.research.google.com/drive/1YoQhY2goGQ9c77wkAYUVU_qKH8ndlXwi`
- Google Drive 持久化目录：`/content/drive/MyDrive/ax630c_v25_rc_persist/`
- Pulsar2 版本：`8.0`（commit `e9a1aa15`，AX620E）
- 当前发布结论：**v2.5 仍为 NO-GO**（A/C 线离线与编译产物已齐备，B 线原 CAL64 门禁仍未过，且尚未在 AX630C 物理板卡完成 NPU/完整视频验收）。

---

## 1. 已完成并持久化到 Google Drive 的产物

| 线 | 产物路径（Drive `ax630c_v25_rc_persist/`） | 大小 | SHA256 | 状态 |
|---|---|---:|---|---|
| **A 线（OCR U16 NPU1）** | `models/plate_rec_v25_npu1.axmodel` | 3,596,050 B | `431db6dd56a5c8b1a223c38ebaa9d83d90758aa87cc14eb3c5c1238050867ac7` | `compiler.check=1` PASS；EndToEnd Cosine **0.99932**；10 例 `val600` 仿真 CTC 文本 **10/10** 与浮点及 GT 全一致 |
| **A 线（OCR U16 NPU2）** | `models/plate_rec_v25_npu2.axmodel` | 3,215,286 B | `c607962213854b25ac46806ae67abac8c525cd6e637c3e408ba88b0f238c5679` | `compiler.check=1` PASS；EndToEnd Cosine **0.99932** |
| **C 线（aarch64 二进制）** | `models/plate_face_demo_v25rc.stripped` | 7,166,640 B | `ac7deea0f14e4aa970cfc2ed787118d95d913c0356b677f4e23a77b9754af62d` | `build_aarch64.sh` 零错误编出（含 R40+R46+`==3` 防误接） |
| **v2.5-rc1 板端完整包** | `release/ax630c_plate_face_demo_v2.5-rc1_board.tar.gz` | 39,247,031 B | `291fa545c8c46e5ee4bf8527b388f2b06abb5a17c7557f5042e44079b58bddbe` | 内含 `v2.5-rc1` 二进制、`plate_rec_v25` U16（`npu1`/`npu2`）、v2.4 `plate_det_u16`/`face_det`/`plate_cls` 3 类默认、`lib/`、`run.sh`、`SHA256SUMS` |
| 日志与指标 | `logs/`（18 份）与 `metrics/`（8 份） | — | 见 Drive | 全部落盘可复核 |

---

## 2. B 线（五类颜色 `ecba573c…`）本轮实证结论

原门禁不变：`Max ≤ 0.03`、`MAE ≤ 0.005`、`argmax 64/64`（CAL64 对原浮点黄金 `ecba573c…`）。

| 实验 | 方案 | 编译 (`check=1`) | CAL64 Max | CAL64 MAE | argmax | 结论 |
|---|---|---|---:|---:|---:|---|
| B1 | 全层 `U16` + `S16` 权重 | FAIL (`conv24` 累加器溢出) | — | — | — | **实证 AX620E 硬件限制** |
| B1d | `layer_names` 对 `conv0..conv21`+`fc` 启用 `S16`，`conv24` 保持 `S8` | PASS | 0.9828 | 0.2930 | 10/64 | **证实 AX620E `U16×S16` Conv 累加器溢出** |
| B1e | 仅 `conv0..conv10`+`fc` 启用 `S16` | PASS | 0.6770 | 0.1098 | 35/64 | 同上，**彻底排除 S16 权重路线** |
| B2 | 精确数学等价 CLE（浮点差 `2.98e-7`）+ `U16/S8` | PASS (Cosine 1.00000) | 0.0718 | 0.00733 | 59/64 | Reference 与 `compiled.axmodel` 吻合，未过门 |
| R86 | 400 张 `TRAIN400` 范围对齐 QAT 蒸馏 + `U16/S8` | PASS (Cosine 1.00000) | 0.8686 | 0.1372 | 28/64 | 小样本蒸馏在合成 CAL64 压力分布上漂移，未过门 |
| *历史 R84* | *3348 图 QAT 蒸馏 + `U16/S8`* | *PASS* | *0.04685* | *0.00521* | *63/64* | *目前最接近但仍 FAIL* |

**关键工程判断**：
1. AX620E 在 `U16` 激活下**只能使用 `S8` 权重**（`S16` 权重会触发硬件累加器溢出）。
2. 在现有 8 层 VGG 风格 `ColorV2`（`ecba573c…`）架构 + `U16/S8` 硬件约束下，所有合法后训练量化与小样本蒸馏路线均已实证触顶；若坚持五类颜色必须通过原 CAL64 门禁，需改用 **GPU 上基于完整 3348+ 数据的范围对齐 QAT**，或**训练更浅/更抗 S8 量化的轻量化五类颜色结构**（如带 `ReLU6` 激活上限的 MobileNet/RepVGG 风格小网络，彻底消除 `relu0` 34.39 离群值）。
3. 若希望**最快发布可用的 v2.5**，A 线微调识别器 U16 `.axmodel`（`npu1`/`npu2`）与 C 线 `plate_face_demo_v25rc.stripped`（含 R46 特种牌格式规则、旧三类颜色 `==3` 防误接保护）**现已全部就绪**，可直接上板实测并发布（五类颜色头继续留在 `--color5` 可选实验位，待新架构/GPU QAT 过门后在 v2.6 默认启用）。
