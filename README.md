# AX630C 人脸 + 车牌 全流程测试 Demo

输入一段视频，输出叠加了算法结果的视频。整个流程都在 AX630C 上完成：

- 人脸：检测 + 5 点关键点 + ByteTrack 跟踪 + 每个 ID 选最佳帧
- 车牌：检测（4 角点 + 单/双层）+ ByteTrack 跟踪 + 透视矫正 + 字符识别 + 颜色识别 + 多帧投票 + 最佳帧

另外可以把每个 track 的最佳帧按 JPEG + JSON 存下来。这模拟的是“IPC 端选帧 → 推给主机”的那一步。

> 这个 demo 用来**上板验证功能和耗时**，不是量产代码。量产要换成 VDEC/IVPS/VENC 硬件链路，见第 8 节。

---

## 0.4 v2.4 更新：单/双层双假设 + U16 车牌检测

详见 CHANGELOG.md。`dual_hyp 1`（demo.cfg）对不确定的车牌按单层/双层各识别一次择优；板端车牌检测默认用 `plate_det_npu1_u16.axmodel`（整网 U16，真实图校准）。
用户复核标注的 300 张国内抓拍图：整牌完全匹配 x86 88.6% -> 91.5%，板端 81.7% -> 90.5%（留出 100 张：板端 80.2% -> 89.6%）。

## 0.3 v2.3 更新：黄绿（大型新能源）颜色

颜色分类网络只有蓝/绿/黄三类，大型新能源车的“黄绿”牌（左侧黄、右侧绿）会被判成绿或黄。v2.3 用号码规则补出“黄绿”：
- 新能源牌 8 位 = 省份 + 发牌机关字母 + 6 位序号。**小型**：动力类型字母在序号**第 1 位**（整牌第 3 位），如 `粤BD12345`，仍为 **绿**；
  **大型**：动力类型字母在序号**最后一位**（整牌第 8 位），如 `粤B12345D`，序号前 5 位为数字/字母。
- 动力类型字母：D 纯电、F 非纯电；扩展 A/B/C/E（纯电）、G/H/J/K（非纯电）。
- 判定：号码 8 位且格式合法 **且** 末位 ∈ `yellowgreen_letters` **且** 分类网络颜色 ∈ `yellowgreen_colors`（默认绿/黄）→ **黄绿**；否则 8 位牌仍为绿。
- 每帧识别结果和轨迹最终结果都会判定；轨迹级用投票后的最终号码 + 分类颜色投票再判一次，同牌合并后也再判。
- 叠加显示“黄绿”（底色黄绿），snap JSON 的 `color` / `color_cn` 为最终颜色，新增 `color_src`（rule = 由号码规则决定，cls = 分类网络）和 `color_voted`；plates.json 增加 `color_src`。
- 参数：`yellowgreen_rule`（`--legacy` 关闭）、`yellowgreen_letters`、`yellowgreen_colors`。单元测试：`tools/test_rules.cpp`（`tools/build_tools_x86.sh` 编译后运行 `build/x86/test_rules`）。

## 0.2 v2.2 更新：ROI 放大二次检测

问题：第 4986 帧附近一块清晰的绿牌（渝AB07288，原图约 190x80 像素）全程漏检。全图检测把 2592x1520 缩到 640x384（约 1/4），该牌 objectness 只有 0.10~0.37，
达不到建轨阈值 0.45；把同一位置裁 1296x760（等效 2 倍分辨率）再检，分数 0.6~0.86。属于检测模型在该尺度下的能力不足，不是过滤规则误杀。
v2.2：全图检测中分数在 [roi_seed_thr, plate_new_thr) 之间、宽度 ≥ roi_min_w、不在 OSD 区且未被高分框覆盖的候选，在其周围裁 原图/roi_zoom 大小的 ROI 再检测一次，
结果映射回原图，丢弃被 ROI 边界截断的框，与全图结果统一 NMS。每帧最多 roi_max 个 ROI。NPU 代价：每个 ROI = 1 次车牌检测推理（同 640x384 模型，无需重新编译），
本视频约 1/5 的帧触发，车牌检测平均耗时约 +20%，最坏每帧 2 倍。`roi_max 0` 关闭；`--legacy` 关闭。
配套：ROI 让轨迹在车牌还很小时就建立，小框上的无效识别会提前耗光 `rec_max` 预算并拉低合法比例。v2.2 规定宽度 < `rec_count_min_w`(80px) 且未识别出合法号码的识别不计入预算和非车牌比例（另有同等上限）。

## 0.1 v2.1 更新：单/双层按颜色修正

国内蓝牌、绿牌只有单层，只有黄牌（以及大车黄绿新能源等，按需配置）才有双层。v2 直接用检测器的单/双层类别，部分绿牌被显示为“(双)”。
v2.1：检测器判双层时，先在拉正后的整块车牌上做颜色分类，颜色不在 `double_layer_colors`（默认 `yellow`）中则按**单层**拉正识别，叠加/JSON 也显示单层；
检测器判单层时不改。若改成单层后识别结果格式不合法、且颜色置信度 < `layer_fallback_conf`(0.7)，再按双层识别一次，合法则采用（`layer_fallback 1`，防止暗光黄牌被判成绿/蓝）。还没识别过的新轨迹不显示“(双)”。
轨迹的层数 = 识别时实际采用层数的多数；snap JSON 同时给出 `det_layer` 与 `layer`，plates.json 增加 `layer` 字段，dump 的 SP 行末尾增加 `检测层/实际层`（S/D）。
`--legacy` 与 `double_layer_colors all` 恢复旧行为。工具：`tools/build_tools_x86.sh` 编译 image_eval/dump_dets（`LAYER_BY_COLOR=0` 关闭修正对比）。

## 0. v2 更新（2026-10-09）

模型未变（axmodel 不需要重新编译），只改 C++ 前后处理/跟踪；所有新参数可通过命令行或 `board/demo.cfg`（`--config`）配置；`--legacy` 完全恢复 v1 行为用于对比。

| 问题 | 原因 | 改动 | 主要参数 |
|---|---|---|---|
| 1. 近距离车牌漏检 | 检测器分数其实很高（0.73–0.82），但近处车牌每帧位移 40–110px（车牌宽 110–140px），相邻帧 IoU×score 太低导致跟踪断开/无法确认 | ByteTrack 加 Buffered IoU（C-BIoU）、关闭 IoU×score 融合、降低阈值并启用低分二次关联、hits 计数确认显示 | `plate_thr 0.35` `plate_new_thr 0.45` `plate_buffer 0.4` `fuse_score 0` `plate_display_hits 2` `plate_show_score 0.5` |
| 2. 屏幕字幕(0KM/H 等)误检 | 字幕位置固定、背景随相机运动变化 | OSD 自学习：同一位置反复出现、且框外背景环发生变化（说明相机在动而框没动）的“非车牌”轨迹 → 学成屏蔽区；另支持用户屏蔽区 | `osd 1` `osd_min_hits 25` `osd_min_changes 2` `osd_bg_diff 10` `osd_mask "x,y,w,h;..."` |
| 3. 广告牌/招牌误识别 | 无格式和几何约束 | 四角点几何（凸、顺序、宽高比、角度）；严格车牌格式（省份+7/8 位、新能源、挂/学/警）；识别置信度≥0.7 才投票、颜色置信度加权；合法识别占比<0.3 判非车牌；只显示合法文本或检测分≥0.5 的框 | `plate_geom` `strict_format` `rec_min_conf` `min_vote` `valid_ratio` |
| 4. 人脸漏检/误检(轮胎) | 阈值偏高；无关键点/外观约束 | 降低阈值+BIoU；5 点关键点几何检查（兼容侧脸）；框中部 YCrCb 肤色比例；最小尺寸；最少命中帧数 | `face_thr 0.45` `face_new_thr 0.5` `face_geom` `face_skin 0.08` `face_min_size 20` `face_display_hits 3` |
| 同牌断轨 | 短暂遮挡/漏检 | 编辑距离≤1 且间隔≤4s 的轨迹合并、显示 ID 复用 | `merge_gap 4` |

整段测试视频（14200 帧）对比（x86 ORT，同模型）：

| 指标 | v1 | v2 |
|---|---|---|
| 车牌轨迹数 | 182 | 202 |
| 疑似 OSD/字幕轨迹 | 20 | 0 |
| 格式非法轨迹 | 4 | 0 |
| 不同车牌号 | 155 | 199 |
| 合并后车牌结果 | 182 | 196 |
| 车牌显示帧数 | 2771 | 3688 |
| 对 v2 的 196 个车牌：带正确文本的帧数 | 1611 | 3090 |
| v1 完全没出框的车牌 | – | 48（人工核对均为真车牌） |
| 人脸输出轨迹 | 26（其中 7 个轮胎/车身误检） | 67（轮胎 0；约 6 个其他误检） |
| 算法 FPS（x86） | 19.4 | 19.7 |

人脸模型许可（商用相关）：YuNet（opencv_zoo，MIT）、Ultra-Light-Fast-Generic（MIT）、Pytorch_Retinaface（MIT 代码）、SCRFD/InsightFace（训练数据和模型仅限非商业研究）、yolov5-face（GPL-3.0）。以上大多用 WIDER FACE 训练，其许可在 HF 卡片上标为 CC BY-NC-ND 4.0，商用建议用自有/授权数据重训。YuNet 在本视频抽帧上召回不如当前模型（34 vs 47），未切换。

---

## 1. 目录结构

```
ax630c_plate_face_demo/
├── CMakeLists.txt               BACKEND=AX(板端 AXEngine) / ORT(x86 ONNX Runtime)
├── build_x86.sh                 x86 编译（ONNX Runtime 后端，PC 上验证算法逻辑用）
├── build_aarch64.sh             aarch64 交叉编译（AXEngine 后端）
├── cmake/aarch64-none-linux-gnu.toolchain.cmake
├── src/
│   ├── main.cpp                 主流程：读视频 → 检测 → 跟踪 → 识别 → 最佳帧/投票 → 叠加 → 写视频
│   ├── infer/engine.h           推理后端抽象 InferEngine
│   ├── infer/engine_ax.cpp      AXEngine 后端（CMM 内存、NHWC u8 输入、cache flush/invalidate）
│   ├── infer/engine_ort.cpp     ONNX Runtime 后端（x86）
│   ├── detector.{h,cpp}         YOLOv5-face 风格带关键点检测器（人脸/车牌共用；letterbox + 解码 + NMS）
│   ├── plate_rec.{h,cpp}        车牌矫正（外扩）、双层拼接、CTC 解码、格式校验、颜色判断
│   ├── bytetrack.{h,cpp}        C++ ByteTrack（Kalman + IoU + 匈牙利(KM)，高/低分两阶段）
│   ├── text_render.{h,cpp}      中文叠加（预渲染字形图集，板端不用 FreeType）
│   ├── glyph_atlas.h            由 scripts/gen_glyph_atlas.py 生成（Noto Sans CJK，OFL）
│   └── timer.h
├── tools/video_io_selftest.cpp  视频读写自检（不用 NPU）
├── board/run.sh                 板端一键运行脚本
├── board/README_board.md
├── scripts/                     模型导出、校准数据、Pulsar2 编译、仿真验证、测试视频、三方库编译、打包
├── pulsar2_configs/             Pulsar2 编译配置（npu1/npu2）
├── models/onnx/                 4 个 ONNX
├── models/axmodel/              8 个 axmodel（4 个模型 × npu1/npu2）
├── test_data/                   测试视频 + 真值 json
└── results/x86/                 x86 跑出来的结果视频、截图、日志、最佳帧
```

## 2. 处理流程

```
视频帧(BGR) ─┬─ 人脸检测 yolov5n-face 640x384 ──→ ByteTrack(人脸) ──→ 最佳帧（置信度×尺寸×清晰度×正脸程度）
             │
             └─ 车牌检测 yolov5-face 结构 640x384 ─→ ByteTrack(车牌) ─┬→ 4 角点透视矫正（外扩 x2%/y4%）
                  (4 角点 + 单/双层两类)                              │   双层：上 5/12 与下 2/3 横向拼接
                                                                       ├→ 字符识别 HyperLPR3 rpv3 48x160 (CTC)
                                                                       ├→ 颜色：规则 + 3 类颜色网络 + HSV 兜底
                                                                       └→ 多帧加权投票 + 最佳帧
叠加：人脸框 + 5 点 + ID；车牌 4 角点 + ID + 车牌号 + 中文颜色；左上角显示各模块 ms 和 FPS
```

关键实现：

- **检测输出头不在图里解码**：只导出 3 个 stride 的原始卷积输出（p8/p16/p32），sigmoid 和解码放在 C++ 后处理里做，这样更适合量化。先用 obj logit 预筛掉候选，后处理基本是 0.x ms。
- **ByteTrack**：
  - 第一阶段：高分检测框匹配已有轨迹。
  - 第二阶段：低分检测框匹配剩下的 tracked 轨迹。
  - 然后处理未确认的轨迹，最后新建轨迹。
  - 分配用 KM 算法。丢失多少帧后删除默认等于视频 fps（`--track_lost`）。
  - 人脸和车牌各用一个 tracker。
- **识别调度**：
  - 车牌 track 至少命中 `--min_hits`（默认 3）帧才开始识别。
  - 投票稳定后就不再识别：第一名累计权重 ≥ 4，并且 ≥ 第二名的 2.5 倍。
  - 投票没稳定时，只有画面质量变好才会再识别一次。
  - 每个 track 最多识别 `--rec_max`（默认 20）次。这样识别不会每帧都跑。
- **投票权重**：conf × (0.5 + 0.5 × 质量分)，只统计格式合法且 conf > 0.5 的结果。
- **颜色**：
  - 先按文字规则：WJ/警 → 白；港/澳/使/领 → 黑；学/挂 → 黄；8 位 → 绿。
  - 规则不命中时用颜色网络（蓝/绿/黄）。
  - 饱和度很低时用 HSV 判成白或黑。
- **最佳帧输出**（`--snap_dir`）：track 结束时写出这些文件：
  - `face_XXXX_full.jpg`（大图）、`face_XXXX_crop.jpg`（小图）、`face_XXXX.json`
  - `plate_XXXX_full.jpg`、`plate_XXXX_crop.jpg`、`plate_XXXX_rect.jpg`（矫正图）、`plate_XXXX.json`

  json 里有帧号、框、关键点、置信度、车牌号、颜色和投票情况。车牌只有识别出合法车牌号才会存。

## 3. 模型

| 用途 | 文件 | 输入 | 来源 | License |
|---|---|---|---|---|
| 人脸检测 + 5 点 | face_det_yolov5n_384x640.onnx / face_det_{npu1,npu2}.axmodel | 1x3x384x640 (RGB, /255) | [deepcam-cn/yolov5-face](https://github.com/deepcam-cn/yolov5-face) 官方 yolov5n-face.pt，用 scripts/export_face_onnx.py 导出原始检测头 | **代码 GPL-3.0**；权重用 WIDER FACE 训练（数据集限非商业研究用途） |
| 车牌检测 4 角点 + 单/双层 | plate_det_y5fu_384x640.onnx / plate_det_{npu1,npu2}.axmodel | 1x3x384x640 (RGB, /255) | [HyperLPR3](https://github.com/szad670401/HyperLPR) 20230229 版 y5fu_320x_sim.onnx。用 scripts/prepare_plate_onnx.py 切掉图内解码、修正 ShuffleNet reshape 的固定尺寸，改成 640x384 | Apache-2.0 |
| 车牌字符识别 | plate_rec_rpv3_48x160.onnx / plate_rec_{npu1,npu2}.axmodel | 1x3x48x160 (BGR, (x-127.5)/127.5) | HyperLPR3 rpv3_mdict_160_r3.onnx（CTC，78 类） | Apache-2.0 |
| 车牌颜色 | plate_cls_96x96.onnx / plate_cls_{npu1,npu2}.axmodel | 1x3x96x96 (BGR, /255) | HyperLPR3 litemodel_cls_96x_r1.onnx（蓝/绿/黄） | Apache-2.0 |

**axmodel 的编译方式**：

- 工具和目标：Pulsar2 8.0（lite package），`--target_hardware AX620E`。npu1 和 npu2 各编一套。
- 量化：检测、颜色模型全部 INT8（U8）；识别模型整网 U16（INT8 时余弦相似度只有 0.875，换 U16 后是 0.999）。
- 输入：axmodel 的输入都是 **NHWC uint8**，mean/std 已经编进模型。检测模型的输入是 RGB，C++ 里会把 BGR 转成 RGB；识别和颜色模型的输入是 BGR。
- 校准集：每个模型 64 张。
  - 人脸：4 张 NASA 公有领域合影，做了多尺度和翻转。
  - 车牌：CCPD2020 绿牌 train + CCPD 子集（蓝牌）里的另一批图，不和测试视频用的图重叠。

Pulsar2 编译时自带的逐层比对结果，是在校准数据上 axmodel 和浮点模型的余弦相似度：

| 模型 | 输出余弦（npu1，npu2 相同） |
|---|---|
| face_det | p8 0.9959 / p16 0.9949 / p32 0.9843 |
| plate_det | p8 0.9989 / p16 0.9978 / p32 0.9952 |
| plate_rec (U16) | 0.99908 |
| plate_cls | 1.00000 |

另外用 Pulsar2 `run` 仿真器，在没参加校准的图上对比了 ONNX 和 axmodel 的**最终结果**（检测框、车牌号），结果见第 7 节。

## 4. 板端运行（拿到板子后）

**板子要求**：

- AX630C / AX620Q 系列（AX620E SDK），系统里有 AX 运行库：`libax_engine.so`、`libax_interpreter.so`、`libax_sys.so`。一般在 `/soc/lib` 或 `/opt/lib`。
- glibc ≥ 2.29。
- 程序已经静态链接 libstdc++ 和 libgcc，FFmpeg 动态库随包带了。

```sh
# PC 上
scp ax630c_plate_face_demo_board.tar.gz root@<板子IP>:/root/
# 板子上
cd /root && tar xzf ax630c_plate_face_demo_board.tar.gz && cd ax630c_plate_face_demo_board
./video_io_selftest test_video.mp4 /tmp/t.mp4   # 可选：只测视频读写
./run.sh                                         # test_video.mp4 → out.mp4，最佳帧写到 snaps/
```

- **结束时**会打印各模块的平均耗时、algo FPS、端到端 FPS，以及识别出的车牌列表。
- **用自己的视频**：`./run.sh in.mp4 out.mp4`。
- **换模型版本**：`NPU=npu2 ./run.sh`。
- **其他参数**用 `EXTRA_ARGS="..." ./run.sh` 透传。
- **VNPU**：程序默认关闭虚拟 NPU（`AX_ENGINE_VIRTUAL_NPU_DISABLE`）。`PFD_VNPU=1` 会打开虚拟 NPU。打开后只能跑 npu1 模型，是否需要打开以板子 SDK 文档为准。
- **SDK 库兜底**：板子里找不到 `libax_engine.so` 时，run.sh 会自动用包里 `lib_sdk_fallback/` 的 SDK 库（V2.0.0_P7）。这批库必须和板子的内核驱动版本匹配，不匹配就要用板子自带的库。

直接调用程序：
```sh
LD_LIBRARY_PATH=./lib:/soc/lib:/opt/lib ./plate_face_demo -i in.mp4 -o out.mp4 \
  --face models/face_det_npu1.axmodel --plate models/plate_det_npu1.axmodel \
  --rec models/plate_rec_npu1.axmodel [--color models/plate_cls_npu1.axmodel] [--snap_dir snaps/]
```

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--face` / `--plate` | — | 至少给一个；给 `--plate` 时必须同时给 `--rec` |
| `--color` | 无 | 颜色网络。不给的话只用规则 + HSV |
| `--snap_dir` | 无 | 最佳帧 JPEG + JSON 的输出目录 |
| `--face_thr` / `--plate_thr` | 0.5 / 0.5 | 检测阈值 |
| `--nms` | 0.45 | NMS IoU |
| `--track_lost` | fps | 轨迹丢失多少帧后删除 |
| `--min_hits` | 3 | 车牌 track 命中几帧后开始识别 |
| `--rec_max` | 20 | 每个车牌 track 最多识别几次 |
| `--double_mode` | merge | 双层车牌：`merge` 上下拼成一行识别 / `separate` 上下分开识别 |
| `--expand_x` / `--expand_y` | 0.02 / 0.04 | 矫正时向外扩的比例 |
| `--max_frames` | 全部 | 只处理前 N 帧 |
| `--quiet` | — | 不打印逐帧日志 |

输出后缀是 `.avi` 时用 MJPG 编码，其它后缀用 mp4v（MPEG-4 Part 2）。FFmpeg 只编了内置编解码器，**不含 H.264 编码器**（x264 是 GPL）。解码支持 H.264、HEVC、MPEG-4、MJPEG。

**单模型 NPU 纯推理耗时**：用板子自带的 `ax_run_model`（如果有）跑，例如 `ax_run_model -m models/plate_det_npu1.axmodel -w 10 -r 100`。demo 里显示的 infer 时间包含 CPU 侧的 resize/拷贝/cache 操作。

## 5. 编译

### 5.1 x86（ONNX Runtime 后端，PC 上验证算法）
依赖：

- CMake ≥ 3.16
- OpenCV 4.x（带 videoio/FFmpeg）
- [ONNX Runtime 1.20.1 linux-x64](https://github.com/microsoft/onnxruntime/releases)，解压后用 `ORT_DIR` 指定

```sh
ORT_DIR=/path/to/onnxruntime-linux-x64-1.20.1 ./build_x86.sh
./build/x86/plate_face_demo -i test_data/test_video.mp4 -o out.mp4 \
  --face models/onnx/face_det_yolov5n_384x640.onnx --plate models/onnx/plate_det_y5fu_384x640.onnx \
  --rec models/onnx/plate_rec_rpv3_48x160.onnx --color models/onnx/plate_cls_96x96.onnx --snap_dir snaps
```

### 5.2 aarch64（AXEngine 后端）
依赖：

1. 交叉工具链：`gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu`（ARM 官方，sysroot 里是 glibc 2.30，和 AX620E SDK 用的一样）。
2. AX620E BSP SDK 的 `msp/out/arm64_glibc`（需要里面的 `include/` 和 `lib/libax_engine.so` 等）。可以从 [AXERA-TECH/ax620e_bsp_sdk](https://github.com/AXERA-TECH/ax620e_bsp_sdk) 获取。
3. aarch64 版 FFmpeg + OpenCV，用脚本编：
   ```sh
   TOOLCHAIN_DIR=/opt/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu \
   FFMPEG_SRC=/path/ffmpeg-6.1.2 OPENCV_SRC=/path/opencv-4.10.0 ./scripts/build_3rdparty_aarch64.sh
   # 结果在 3rdparty/aarch64/：FFmpeg 是 .so（LGPL 动态链接），OpenCV 是静态库
   ```

然后：
```sh
TOOLCHAIN_DIR=... BSP_MSP_DIR=/path/ax620e_bsp_sdk/msp/out/arm64_glibc ./build_aarch64.sh
# 产物：build/aarch64/plate_face_demo（.stripped 是去符号版本）、build/aarch64/video_io_selftest
TOOLCHAIN_DIR=... BSP_MSP_DIR=... ./scripts/package.sh   # 打板端包和源码包
```

### 5.3 模型导出与编译
```sh
python scripts/export_face_onnx.py   ...   # yolov5n-face.pt → 原始检测头 ONNX（需要 yolov5-face 仓库）
python scripts/prepare_plate_onnx.py ...   # HyperLPR3 ONNX → 车牌检测/识别/颜色 ONNX
python scripts/prepare_calib.py      ...   # 生成 4 个校准 tar（CCPD + NASA 图）
python scripts/gen_pulsar2_configs.py      # → pulsar2_configs/*_{npu1,npu2}.json
PULSAR2=/path/pulsar2 CALIB_DIR=/path/calib ./scripts/compile_axmodels.sh   # → models/axmodel/
python scripts/verify_axmodel_sim.py ...   # Pulsar2 仿真器：ONNX vs axmodel 结果对比
```
每个脚本的具体参数用 `-h` 查看。

## 6. 测试视频

`test_data/test_video.mp4`：1280x720，15 fps，48 秒（720 帧）。

- 内容：12 段，每段约 2.6 秒，段之间插 1.4 秒黑帧，用来把各段的跟踪隔开。
- 版式：有“左车牌 + 右人脸”的分屏，也有整屏车牌、整屏人脸，带缓慢平移和缩放，模拟运动。
- 真值：在 `test_video_gt.json`。可以用 `scripts/check_against_gt.py` 自动对比。
- 素材：
  - 车牌图来自 [CCPD](https://github.com/detectRecog/CCPD)（MIT）。蓝牌选自 base/fn/rotate/tilt/weather 子集，绿牌来自 CCPD2020。
  - 人脸图是 NASA 国际空间站乘组合影（Wikimedia Commons，Public Domain）。
- 视频由 `scripts/make_test_video.py` 生成，可以换素材重新生成。

## 7. 在开发机（x86 box）上实际验证过的内容

> 下面**都不是板端数据**。板端精度和性能需要上板实测。

1. **x86 全流程跑通**（ONNX Runtime CPU 后端，跑的是同一套 C++ 代码，只是推理后端不同），结果在 `results/x86/`：
   - 文件：`out_x86.mp4`（结果视频）、`frame_150.png` / `frame_390.png` / `frame_505.png`（截图）、`log.txt`、`snaps/`。
   - 车牌：10 段车牌里 8 段的车牌号和颜色与真值完全一致。没对上的 2 段：
     - 1 段把省份“皖”认成了“辽”。
     - 1 段（CCPD fn 子集，车牌在原图最边缘、近距离被截断）没有输出合法车牌号。
   - 人脸：每段 4~7 张人脸都检出了，5 点位置正确。每段的人脸 track 数略多于实际人数，说明人脸被遮挡或靠近画面边缘时有少量 ID 切换。
   - 注意：CCPD 很可能在 HyperLPR3 的训练集里，所以这只是功能正确性检查，**不能当成识别精度**。
   - 这台 x86 机器的 CPU 耗时只能说明流程能跑，和板端没有可比性。
2. **aarch64 交叉编译成功**：
   - 产物是 aarch64 ELF，依赖的最高 glibc 符号版本是 2.29，libstdc++ 已静态链接。
   - 在 qemu-aarch64 下能加载、能解析参数。到 `AX_SYS_Init` 时报错 0x800b0310，这在没有 AX 硬件的环境下是预期的。
   - `video_io_selftest` 在 qemu-aarch64 下读 mp4、写 mp4/avi 都正常，说明交叉编译的 FFmpeg + OpenCV 能用。
3. **8 个 axmodel 编译成功**，Pulsar2 余弦比对见第 3 节。
4. **Pulsar2 仿真器对比**（`scripts/verify_axmodel_sim.py`，npu1）：结果见本节末尾的“仿真对比结果”。

## 8. 已知限制 & 量产建议

- **没有在真实板子上跑过**。NPU 实际耗时、CMM 内存占用、和 AI-ISP 等业务共享 NPU 时的表现，都要上板实测。
- **视频链路全是 CPU 软件实现**：OpenCV + FFmpeg 软件解码、resize、画框、MPEG-4 软件编码。在 A53 双核上，720p 的软件编解码本身就会占掉相当多 CPU，所以 demo 的端到端 FPS 不能代表量产能力。**量产应该改成**：
  - 摄像头 / VIN 或 **VDEC** 硬件解码，拿到 NV12；
  - **IVPS** 硬件做缩放、裁剪、颜色转换，送 NPU（模型可以改成 NV12 输入，见 Pulsar2 的 `csc_mode` / `src_format`）；
  - 叠加用 **IVPS/RGN（OSD）**；
  - **VENC** 硬件编码 H.264/H.265，以及 JPEG 抓拍。
  - 推流或推大小图给主机，走 RTSP 或私有协议。
- **检测分辨率**：640x384 是在 720p 下兼顾小目标和速度定的。远处的小车牌或小人脸可能漏检。可以改分辨率或做 ROI 二次检测，但 NPU 耗时会跟着变。
- **双层车牌**：拼接逻辑写好了，但测试视频里没有真实双层牌（CCPD 基本是单层蓝牌和绿牌），这条路径没有用真实数据验证过。
- **颜色**：颜色网络只有蓝/绿/黄三类。白、黑主要靠文字规则和 HSV，警牌、使馆牌、港澳牌这些没有验证过。
- **人脸 ID 切换**：ByteTrack 只用了运动信息和 IoU，没有外观特征。快速运动、遮挡后可能换 ID。要更稳可以加 ReID，或者针对车载场景调 `--track_lost` 和阈值。
- **输出编码**：FFmpeg 没带 GPL 的 x264，mp4 用的是 mpeg4（MPEG-4 Part 2），大多数播放器能放。如果需要 H.264 文件，在 PC 上用 ffmpeg 转一下，或者改用 VENC。
- **模型精度**：没有在车载实拍数据上评估过。上线前要用自己场景的数据评估，必要时微调或重新训练（尤其是人脸模型，见第 9 节的 license 问题）。

## 9. License 与商用风险

| 组件 | License | 风险 |
|---|---|---|
| yolov5-face 代码 / yolov5n-face 权重 | GPL-3.0；训练数据 WIDER FACE 限非商业研究用途 | **商用风险高**。GPL 传染性加上训练数据许可的问题，量产建议换成许可宽松的人脸检测（例如自己用可商用数据训练、买商业授权，或者选 Apache/MIT 且训练数据可商用的模型）。本 demo 只用这些权重，没有链接 GPL 代码，但权重本身的授权仍然不明确 |
| HyperLPR3 车牌检测/识别/颜色模型 | Apache-2.0 | 风险低。需要保留版权和 License 声明。训练数据的来源 HyperLPR 没有完全说明，商用前建议自己确认 |
| ByteTrack | MIT（本 demo 是按论文思路自己写的 C++ 实现） | 风险低 |
| OpenCV 4.10 | Apache-2.0 | 风险低 |
| FFmpeg 6.1.2 | LGPL-2.1，动态链接，没开 GPL 组件 | 需要满足 LGPL：提供 FFmpeg 源码或获取方式，并允许用户替换 .so。量产改用 VDEC/VENC 后可以去掉 |
| Noto Sans CJK（叠加字形） | SIL OFL 1.1 | 风险低 |
| AX SDK / Pulsar2 / ax-samples | AXERA 授权 / BSD-3 | 按 AXERA 的协议 |
| 测试素材：CCPD（MIT）、NASA 照片（Public Domain） | — | 只用于测试 |
| 评估过但没采用：AXERA HF 上的 yolov7-face（模型卡写 MIT，但上游 yolov7-face 是 GPL-3.0）、Plate-axera（AGPL-3.0，只有 AX650） | — | — |

## 10. 上板前要自己补的事

1. 确认板子固件里有 AX 运行库：`ls /soc/lib/libax_engine.so /opt/lib/libax_engine.so`，版本要和 Pulsar2 8.0 编出来的 axmodel 兼容（AX620E SDK V2.0 以上）。没有的话用 `lib_sdk_fallback/`，或者从对应 SDK 拷过来。
2. 确认 glibc ≥ 2.29（`ldd --version` 或 `ls /lib/libc-*`）。
3. 确认 CMM 内存够。npu1 的 4 个 axmodel 文件合计约 7.3 MB，运行时还要加上 IO 和中间缓冲，实际占用以板上实测为准；如果 CMM 被 ISP 等业务占满，模型可能加载失败。
4. 用 `ax_run_model` 测各模型的纯 NPU 耗时，用 `./run.sh` 测全流程，再对比 npu1 和 npu2。
5. 用自己车载场景的视频跑一遍。重点看：远处小车牌、夜间、逆光、双层牌、黄牌和其他特殊牌。
6. 量产链路换成 VIN/VDEC → IVPS → NPU → RGN/VENC，人脸模型换成能商用的。
