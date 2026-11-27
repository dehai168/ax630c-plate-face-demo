# AX630C 人脸与国内车牌全流程识别系统 (`ax630c-plate-face-demo`)

[![Version](https://img.shields.io/badge/release-v2.7-blue.svg)](CHANGELOG.md)
[![Target](https://img.shields.io/badge/NPU-AX630C%20%2F%20AX620E-green.svg)](#4-快速上手板端部署)
[![Backend](https://img.shields.io/badge/backend-AXEngine%20%7C%20ONNX%20Runtime-orange.svg)](#5-从源码编译)

面向 **AX630C / AX620E** 边缘计算芯片的 C++14 端到端**人脸检测 + 国内全种类车牌检测、跟踪、矫正、字符识别与五色分类**流水线，支持 **AXEngine（板端 NPU）** 与 **ONNX Runtime（x86 PC 验证）** 双后端同代码运行。

- **人脸分支**：5 点关键点检测 + 五点几何/肤色抗误检过滤 + Buffered-IoU ByteTrack + 质量打分 Best-Shot 抓拍
- **车牌分支**：4 角点检测（单/双层）+ ROI 放大二次检测 + 自学习 OSD 屏蔽 + 透视拉正 / 双层拼接 + **31 省均衡 OCR（U16 + CTC Prefix Beam 语法解码）** + **`ColorV3-ReLU6` 五色分类** + 多帧投票与同牌合并 + JSON/视频/原图批测输出

---

## 1. 版本演进与实板基准（`v2.7`）

在 171 张覆盖全国 31 省、6 种颜色、单/双层及特种号牌（蓝/黄/绿/黄绿/警/学/挂/港澳/使馆）的人工标注测试集上，**AX630C（`PFD_VNPU=1`，NPU1 全 5 模型）** 实测对比如下：

| 评测指标 | `v2.6`（基线） | `v2.7 Phase 1`（C++ 语法解码 + 原图直通） | **`v2.7`（+ 31 省均衡 OCR）** | 相对 `v2.6` 提升 |
| :--- | :---: | :---: | :---: | :---: |
| **车牌检出率** | 93.57% (160/171) | 98.83% (169/171) | **98.83% (169/171)** | **+5.26%** |
| **OCR 整牌准确率（端到端）** | 78.36% (134/171) | 88.89% (152/171) | **91.81% (157/171)** | **+13.45%** |
| **OCR 整牌准确率（检出内）** | 83.75% (134/160) | 89.94% (152/169) | **92.90% (157/169)** | **+9.15%** |
| **颜色准确率（端到端 / 检出内）** | 89.47% / 95.62% | 97.08% / 98.22% | **97.66% / 98.82%** | **+8.19%** |
| **单/双层准确率（端到端 / 检出内）** | 91.23% / 97.50% | 95.32% / 96.45% | **95.32% / 96.45%** | **+4.09%** |
| **三项全对（号 + 色 + 层）** | 77.78% (133/171) | 87.72% (150/171) | **90.64% (155/171)** | **+12.86%** |

> 板端实时性能（AX630C, 1280×720, NPU1 全 5 模型）：**算法平均 10.2 ~ 10.8 FPS**（人脸检测 ~33 ms、车牌检测 ~28 ms、单牌 OCR ~37 ms、五色分类 ~2.4 ms）。

### 关键里程碑
- **`v2.7`**（当前最新）：C++ CTC Prefix Beam 加入中国车牌位置语法先验（彻底消除大型新能源 `D→0` 误识）、`dual_hyp` 双层方牌锁定、特种警牌（如 `蒙08888警`）放行、新增 `--image_list` 原分辨率批测模式；31 省均衡 OCR 微调模型经 Pulsar2 8.0（`AX620E`, `U16`）量化上线，纯省份错由 18 例降至 3 例。
- **`v2.6`**：全新轻量 `ColorV3-ReLU6` 五色网络（蓝/黄/绿/白/黑，`U16+S8` 量化门禁 64/64 全通过）默认上线 + C++ CTC Prefix Beam 解码。
- **`v2.5`**：混合微调 OCR（`U16`）上板 + GA36-2018 特种车牌规则（使/领/警/学/挂/港/澳）。
- **`v2.0 ~ v2.4`**：C-BIoU ByteTrack、OSD 自学习屏蔽、单/双层双假设（`dual_hyp`）、ROI 放大二次检测、黄绿大型新能源规则。

完整更新记录见 [CHANGELOG.md](CHANGELOG.md)。

---

## 2. 系统架构与流水线

```text
视频流 / 原图列表 (--image_list)
  ├─ 人脸分支: yolov5n-face (640x384) ─→ 5点几何 + 肤色过滤 ─→ ByteTrack(C-BIoU) ─→ 质量最优抓拍
  └─ 车牌分支: y5fu-plate (640x384, U16) + ROI 放大二次检测
                 └─→ 4角点几何过滤 + OSD自学习屏蔽 ─→ ByteTrack(C-BIoU)
                       ├─→ 透视拉正 (外扩 x2%/y4%)；双层牌按上5/12+下2/3拼接 + dual_hyp 择优
                       ├─→ OCR (48x160, U16) + CTC Prefix Beam 语法解码 (78类)
                       └─→ ColorV3-ReLU6 五色分类 (96x96) + 规则兜底 (黄绿/警/港澳/使/学/挂)
                             └─→ 多帧加权投票 + 同牌合并 ─→ 叠加输出 / Best-Shot / JSON
```

---

## 3. 模型清单（`models/`）

所有板端模型均使用 **Pulsar2 8.0**（`--target_hardware AX620E`）分别编译为 **`npu1`** 与 **`npu2`** 两套版本，输入格式为 `NHWC uint8`（均值方差已内嵌）。

| 用途 | 板端模型 (`models/axmodel/`) | 浮点 ONNX (`models/onnx/`) | 输入规格 | 量化精度 | 许可来源 |
| :--- | :--- | :--- | :--- | :---: | :--- |
| 人脸检测 + 5 点 | `face_det_{npu1,npu2}.axmodel` | `face_det_yolov5n_384x640.onnx` | `384×640` RGB | INT8 (U8) | yolov5-face (GPL-3.0) |
| 车牌检测 + 4 角点 | `plate_det_npu1_u16.axmodel` / `plate_det_{npu1,npu2}.axmodel` | `plate_det_y5fu_384x640.onnx` | `384×640` RGB | U16 (推荐) / INT8 | HyperLPR3 (Apache-2.0) |
| 车牌字符识别 (`v2.7`) | `plate_rec_{npu1,npu2}.axmodel` | `plate_rec_finetune.onnx` | `48×160` BGR | 整网 U16 | HyperLPR3 + v2.7 均衡微调 |
| 五色分类 (`v2.6`) | `plate_color5_{npu1,npu2}.axmodel` | —（`ColorV3-ReLU6`） | `96×96` BGR | U16+S8 | 本项目自研训练 |
| 三色分类（判层/兼容） | `plate_cls_{npu1,npu2}.axmodel` | `plate_cls_96x96.onnx` | `96×96` BGR | INT8 (U8) | HyperLPR3 (Apache-2.0) |

> 词表文件：[models/onnx/charset.json](models/onnx/charset.json)（78 项，index 0 为 CTC blank，支持 31 省汉字及 `使/领/警/学/挂/港/澳`）。

---

## 4. 快速上手（板端部署）

### 环境要求
- **硬件平台**：AX630C / AX620Q 系列（AX620E SDK，系统含 `libax_engine.so`、`libax_interpreter.so`、`libax_sys.so`，`glibc >= 2.29`）。
- 板端包已静态链接 `libstdc++` / `libgcc`，并随附 LGPL FFmpeg 动态库。

### 运行视频推理
```sh
cd ax630c_plate_face_demo_board
./run.sh input.mp4 output.mp4
```
常用环境变量与参数覆盖：
- `NPU=npu2 ./run.sh in.mp4 out.mp4`：切换 NPU2 模型（默认 `npu1`，开启 `PFD_VNPU=1` 时须用 `npu1`）。
- `COLOR5_MODEL=none ./run.sh in.mp4 out.mp4`：关闭五色网络，回退旧三类颜色逻辑。
- `EXTRA_ARGS="--osd 0 --min_vote 1.5" ./run.sh in.mp4 out.mp4`：通过 `EXTRA_ARGS` 覆盖 `demo.cfg` 中的任意参数。

### 运行原分辨率图片批测（`v2.7` 新增）
```sh
PFD_VNPU=1 LD_LIBRARY_PATH=$PWD/lib:/soc/lib:/opt/lib ./plate_face_demo \
  --config demo.cfg \
  --face models/face_det_npu1.axmodel \
  --plate models/plate_det_npu1_u16.axmodel \
  --rec models/plate_rec_npu1.axmodel \
  --color models/plate_cls_npu1.axmodel \
  --color5 models/plate_color5_npu1.axmodel \
  --image_list list.txt --out_dir out_imgs --result_json results.json
```

---

## 5. 从源码编译

### AX630C 板端交叉编译（Linux / WSL2）
需要 `gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu` 与 `ax620e_bsp_sdk`：
```sh
# 1. 首次编译依赖（FFmpeg 6.1.2 + OpenCV 4.10.0 aarch64）
TOOLCHAIN_DIR=/path/to/gcc-arm-9.2 FFMPEG_SRC=/path/to/ffmpeg-6.1.2 OPENCV_SRC=/path/to/opencv-4.10.0 \
  ./scripts/build_3rdparty_aarch64.sh

# 2. 编译板端可执行文件（输出 build/aarch64/plate_face_demo.stripped）
TOOLCHAIN_DIR=/path/to/gcc-arm-9.2 BSP_MSP_DIR=/path/to/ax620e_bsp_sdk/msp/out/arm64_glibc \
  ./build_aarch64.sh

# 3. 打包板端发布包
./scripts/package.sh
```

### x86 PC 端编译验证（ONNX Runtime）
```sh
ORT_DIR=/path/to/onnxruntime-linux-x64 ./build_x86.sh
```

---

## 6. 核心配置说明（`board/demo.cfg`）

| 类别 | 关键参数 | 默认值 | 说明 |
| :--- | :--- | :---: | :--- |
| **检测与跟踪** | `plate_thr` / `plate_new_thr` / `plate_buffer` | `0.35 / 0.45 / 0.4` | ByteTrack 高分/建轨阈值与 C-BIoU 外扩比例 |
| **格式与投票** | `strict_format` / `rec_min_conf` / `min_vote` | `1 / 0.7 / 1.0` | 严格车牌语法校验、单帧置信度下限与累计投票门槛 |
| **解码与双层** | `rec_prefix_beam` / `dual_hyp` / `double_layer_colors` | `1 / 1 / yellow` | CTC Prefix Beam 语法解码与单/双层双假设择优 |
| **五色与黄绿** | `yellowgreen_rule` / `yellowgreen_letters` | `1 / ABCDEFGHJK` | 8 位大型新能源黄绿判定规则（结合 `ColorV3-ReLU6`） |
| **ROI 二次检测** | `roi_max` / `roi_seed_thr` / `roi_zoom` | `1 / 0.12 / 2` | 低分远处小车牌区域 2× 放大复检 |
| **OSD 屏蔽** | `osd` / `osd_min_hits` / `osd_min_changes` | `1 / 25 / 2` | 自动学习固定字幕区域并屏蔽（`osd_mask` 支持手动配置） |
| **人脸过滤** | `face_geom` / `face_skin` / `face_display_hits` | `1 / 0.08 / 3` | 五点几何与 YCrCb 肤色过滤，防止轮胎/纹理误检 |

---

## 7. 目录结构

```text
ax630c-plate-face-demo/
├── CMakeLists.txt                   # 双后端构建配置 (BACKEND=AX | ORT)
├── CHANGELOG.md                     # 版本发布与板端实测记录
├── build_aarch64.sh / build_x86.sh  # 交叉编译与 PC 验证编译脚本
├── board/                           # 板端运行脚本 (run.sh)、默认配置 (demo.cfg) 与说明
├── src/                             # 核心 C++14 源码（检测/跟踪/矫正/Prefix Beam/过滤/绘制）
├── models/
│   ├── onnx/                        # ONNX 浮点模型与 charset.json
│   └── axmodel/                     # AX620E npu1 / npu2 量化模型
├── scripts/                         # 依赖编译、ONNX 准备、Pulsar2 配置与板端打包脚本
├── tools/                           # v2.6/v2.7 训练、合成数据生成与 Colab 量化流水线
├── docs/                            # 历史版本验收与量化技术报告
└── third_party_licenses/            # 第三方开源组件与模型许可声明 (NOTICE.md)
```

---

## 8. 许可与致谢

- 人脸检测模型基于 [deepcam-cn/yolov5-face](https://github.com/deepcam-cn/yolov5-face)（代码 GPL-3.0；WIDER FACE 权重限非商业研究，商用请自备数据重训）。
- 车牌检测、基础识别与三色模型基于 [HyperLPR3](https://github.com/szad670401/HyperLPR)（Apache-2.0）。
- 详细第三方声明请参阅 [third_party_licenses/NOTICE.md](third_party_licenses/NOTICE.md)。
