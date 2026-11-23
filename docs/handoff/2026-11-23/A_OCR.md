# A 线：OCR 识别

证据副本：[evidence/A/](evidence/A/)。正文依据 2026-11-17 交付档案，本整理未重跑推理。

## 已接受范围（软件，非发布）

- 已接受微调：PyTorch `f3460c3485fa0b4c53217ab5604512dc4d119422823f3f5844fdd77caa27a272`；ONNX `45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1`。
- 锁定解码：CTC Prefix Beam（beam24 / top5 / alpha=1.0 / 分差阈值 2.0，重复须经 blank）。不得用 val/held 再调参。
- 锁定几何 TTA：gamma=0.1，原图 + 5 个安全内缩视图。不得再调参。
- canonical val600（识别-only，同一批 GT-ROI，非检测端到端）：分母 special 489 + SU 111 = 600。已接受管线 TorchCPU / 归档 ONNX **600 条文本全一致**：整牌 **554/600**（edits 89，CER 0.02077）。prepared uint8 SHA `9233e8a740c61b828b81f5b24f776518997663f7452309fa8daa9875acacf461`。
- 口径警告：历史 R5 GPU PyTorch special 455/489 已另记，不得混入本 554 门槛。

## 隔离 x86 时延（绝非板端）

- Prefix（stage4，官方 1 warmup + 3 轮，N=600，taskset CPU0）：本环境逐条中位数 p50 greedy 3.64 µs，**Prefix 1.418 ms**。接受范围仅 `ACCEPT_MEASUREMENT_REPRODUCIBILITY`。
- TTA（stage5，同一 accepted ONNX 与冻结 val600，与 R5 data.bin 600/600 字节一致）：前向 16800/16800。逐条 pipeline p50：**原视角 5.631 ms**，**六视角 TTA 26.906 ms**，差 21.275 ms。TTA 文本 600/600 匹配 accepted 600。不得用理论 6 倍或外推板端/NPU/全帧。

详见 [evidence/A/STAGE5_SUMMARY.md](evidence/A/STAGE5_SUMMARY.md)、[evidence/A/STAGE_SUMMARY.md](evidence/A/STAGE_SUMMARY.md)。

## 生产 rec 与 UNKNOWN 格

- 生产默认 rec `.axmodel` 身份已绑定：`/opt/demo_v24/ax630c_plate_face_demo_board/models/plate_rec_npu1.axmodel`，SHA256 `728d8f2065632dbd383c347d80f4fba12a38d270da9289223cc709285ba66576`（3596122 B）。与 R34 现场 SHA、v2.4 板端包及备份追溯一致。见 [evidence/A/STAGE3_SUMMARY.md](evidence/A/STAGE3_SUMMARY.md)。
- R44 备份中的 F7 `rec_real_minmax.axmodel`（`02a2f4f1…`）**不是**生产默认 rec。
- **官方浮点/export 对应链未找到。** 不得把附近 ONNX（如 `plate_rec_rpv3_48x160.onnx` `0d1d5007…`）或已接受 `f346`/`45b005` 未经证明地当作 728d8f20 的官方源。
- 「同生产权重 + 冻结 val600 + 锁定 Prefix+TTA」软件格保持 **UNKNOWN**。缺的是官方对应 manifest 和那份已导出的浮点文件，不是用户再给一份全量数据。

## 永久拒绝 / 不要重跑

- **R31** 候选 SHA `0565e53aed3d0465b126c5d3b971737fad721f0c093caf20781c2625d8d90814`：同一 600 输入下 551/600，门禁失败。不重跑、不追 held、不降门槛。
- 不要再做已完成的身份扫描或 logits 重哈希。
- A 侧「无需新模型的独立可自动执行项」在 2026-11-17 已记为穷尽。若要新修复或训练实验：当前未获新实验准入，需新的十字段提案与相应授权。

## 未测 / 交其他线

- 板端或 NPU 上的 Prefix/TTA 时延：未测。
- 6/9 分支仅可选，存在 GT 缺口；不要围绕单张 val 样例调参。
