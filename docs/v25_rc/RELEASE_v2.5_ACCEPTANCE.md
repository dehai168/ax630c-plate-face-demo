# AX630C 车牌+人脸 Demo v2.5 正式发布验收记录（2026-11-24）

- 发布版本：`v2.5`（Git tag `v2.5`，合入 `main`；生产 `/opt/demo_v24` 保持未改动、可随时回退）
- Google Drive 持久化发布目录：`/MyDrive/ax630c_v25_rc_persist/release/`
  - `ax630c_plate_face_demo_v2.5_board.tar.gz`（39,247,209 B，SHA256 `bd89c4fb46572cc36a2736f0a5c88483c637590249438f9f199c627e97a169a3`）
  - `ax630c_plate_face_demo_v2.5_src.tar.gz`（103,758,763 B，SHA256 `d06df4400ec8c525096c003130816fb3cfef5f6d6c3186d80c1eb029f159f29f`）
  - `ax630c_plate_face_demo_v2.5_SHA256SUMS.txt`

## 1. 发布内容与模型清单

- **A 线微调识别器 U16 量化（npu1 / npu2）**：
  - 源 ONNX：`models/onnx/plate_rec_finetune.onnx`（SHA256 `45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1`）
  - Pulsar2 8.0（`e9a1aa15`，AX620E，`calib_rec.tar`，整网 U16，`compiler.check=1` PASS，EndToEnd Cosine **0.99932**）：
    - `plate_rec_npu1.axmodel`（3,596,050 B，SHA256 `431db6dd56a5c8b1a223c38ebaa9d83d90758aa87cc14eb3c5c1238050867ac7`）
    - `plate_rec_npu2.axmodel`（3,215,286 B，SHA256 `c607962213854b25ac46806ae67abac8c525cd6e637c3e408ba88b0f238c5679`）
  - 10 例冻结 `canonical_val600` 使馆特种牌仿真 CTC 文本 **10/10** 与浮点及 GT 全一致。
- **C 线集成与规则**：
  - R46 GA36-2018 特种车牌结构语法（使/领/警/学/挂/港/澳）。
  - R40 独立可选 `--color5` 展示头（默认关闭，`COLOR5_MODEL=... ./run.sh` 可显式启用）+ `2241e28` 旧三类 `outs[0].size() == 3` 严格防误接保护。
  - 默认颜色头继续使用已生产验证的 3 类 `plate_cls_npu1.axmodel` / `npu2.axmodel`；五类颜色默认化移至 v2.6（待 ReLU6 量化友好结构重训过 CAL64 门禁）。

## 2. AX630C 物理板端实测结果（2026-11-24）

测试环境：AX630C（Linux 4.19.125 aarch64，AX Engine `2.10.1s`，`PFD_VNPU=1`，独立非生产目录 `/opt/demo_v25_rc`，生产 `/opt/demo_v24` 零改动）。

### 2.1 完整 720 帧真实视频（`test_video.mp4`，1280×720 @ 15fps，48s）
- **退出码**：`VIDEO_EXIT=0`（无崩溃、无 OOM，结束时剩余可用内存 114 MB）
- **输出产物**：`out_v25.mp4`（31.5 MB）、`out_v25_plates.json`（1.8 KB）、59 张人脸抓拍 + 10 张车牌抓拍
- **识别轨迹**：10/10 车牌轨迹完整稳定输出（5 蓝牌 + 5 绿牌新能源）、59 人脸轨迹
- **平均吞吐**：算法 FPS **`10.82`**，端到端 FPS（含解码/叠加/mp4v 编码）**`4.90`**
- **逐模块平均耗时（n=720 帧）**：
  - `decode`: 22.41 ms
  - `face_pre` / `face_infer` / `face_post`: 6.91 / 33.14 / 0.69 ms
  - `plate_pre` / `plate_infer` / `plate_post` / `plate_roi`: 6.62 / 27.72 / 0.49 / 1.76 ms
  - `plate_rec`: **4.81 ms/帧**（`rec_per_plate` 26.63 ms 含双假设/矫正）
  - `plate_color`: 0.40 ms/帧（`color_per_plate` 2.24 ms）

### 2.2 300 张实拍图评测（`/opt/cn_val/list.txt`，338 检出框）
- **退出码**：`EVAL_RC=0`，全量 300 图稳定跑完，零异常。
- **单牌识别延迟**：约 6.6 ms/牌（与 v2.4 持平）。
