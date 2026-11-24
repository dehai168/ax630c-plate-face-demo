# v2.5-rc 十字段实验提案与 C-IF-004 评审记录

- 日期：2026-11-24
- 分支：`v2.5-rc`（独立非生产分支，基于 `cursor/v25-c-integration-audit-50d6` @ `aed6605` 并合入 `cb6aca5`）
- 授权来源：用户于 2026-11-24 明确授权在 Google Colab 环境开启新的 A/B 线量化与诊断实验（原门禁不降；不改生产 v2.4；本阶段不触碰 AX630C 板卡）。
- 当前发布状态：**v2.5 仍为 NO-GO**，待离线仿真、NPU 实推、板端完整视频、生产验收四级全部通过后方可发布。

---

## 提案 A-QNT-001：已接受微调识别器 `plate_rec_finetune.onnx` 的 Pulsar2 U16 量化与 val600 门禁核验

1. **问题（Question）**：已接受的微调识别 ONNX `45b005b5…` 在 Pulsar2 8.0（AX620E `NPU1`/`NPU2`、整网 U16 激活）量化后，其官方仿真输出在冻结 `canonical_val600` 上是否与浮点 ONNX 保持精度一致（Greedy 549/600、锁定 Prefix Beam 554/600）？
2. **证据（Evidence）**：
   - 生产 v2.4 `plate_rec` 已证明 SVTR（LayerNorm/MatMul/Softmax）在 U8 下余弦仅 ~0.88，整网 U16 激活可达高精度且板上约 6.7 ms/牌（见 `scripts/gen_pulsar2_configs.py`）。
   - `45b005b5…` 在 x86 ORT 冻结 `canonical_val600` 上已验收：Greedy 549/600，锁定 Prefix Beam 554/600（special 454/489、SU 100/111）。
3. **假设（Hypothesis）**：单一变量——使用与 v2.4 相同的 U16 激活配置（`layer_configs: DEFAULT -> U16`，BGR，mean 127.5，std 127.5，MinMax）量化 `45b005b5…`，仿真精度与浮点 ONNX 差距不超过 1 条（Prefix ≥ 553/600，EndToEnd cosine ≥ 0.995）。
4. **机制（Mechanism）**：Pulsar2 8.0 `build --target_hardware AX620E` 编译 `npu1` 与 `npu2` 版本并开启 `precision_analysis: True`（EndToEnd）；随后用 Pulsar2 仿真/Reference 对 `canonical_val600_original_prepare_uint8.npy`（600 条）跑前向，套用锁定 `decoder_core.py`（Prefix Beam）计算整牌精确率与 CER。
5. **与历史区别（History Difference）**：不重训、不动 R31（已永久拒绝）、不重调 Prefix/TTA 参数、不冒充生产 `728d8f20…` 的浮点对应链。
6. **数据（Data）**：
   - 模型：`models/onnx/plate_rec_finetune.onnx`（SHA256 `45b005b56e92df83bbba2199e97935c7897ff23996905d6e6eed0b3ea3af4ae1`）
   - 校准：`plate_rec_calib.tar`（仅训练集来源，不混入 val600）
   - 验收：`canonical_val600_original_prepare_uint8.npy`（SHA256 `9233e8a740c61b828b81f5b24f776518997663f7452309fa8daa9875acacf461`）与 `canonical_val600_ROWS_FROZEN.json`（`f3b71ed8…`）
7. **预期收益（Expected Improvement）**：产出可上板测试的 `plate_rec_v25_npu1.axmodel` / `npu2.axmodel`，闭合 A 线离线仿真到 NPU 产物的缺口。
8. **可能退化（Possible Degradation）**：若个别算子 U16 量化存在分布漂移，可能导致部分 8 字新能源或特种牌字符退化；编译耗时或模型体积略有波动。
9. **预算（Budget）**：Google Colab CPU/T4，单次构建与 600 条仿真 ≤ 20 分钟；0 训练、0 板端写入。
10. **成功 / 失败 / 早停（Success / Failure / Early Stop）**：
    - 成功：编译 + `compiler.check=1` 通过，EndToEnd cosine ≥ 0.995，val600 Prefix ≥ 553/600（special ≥ 453/489、SU ≥ 99/111）。
    - 失败：cosine < 0.99 或 val600 掉点 > 2 条 → 检查校准集分布/敏感层，绝不降门放行。
    - 早停：Pulsar2 报错缺算子或输出 shape/dtype 异常立即停止。

---

## 提案 B-QNT-001：canonical v2 五类颜色 `ecba573c…` 通过原 CAL64 门禁的两阶段精确量化方案

1. **问题（Question）**：在不改动黄金参考 `ecba573c…`、不降低原 CAL64 门禁（`Max ≤ 0.03`、`MAE ≤ 0.005`、`argmax 64/64`）的前提下，能否通过（B1）AX620E 权重量化精度提升（`weight_data_type=S16`）或（B2）严格数学等价的跨层均衡（Cross-Layer Equalization, CLE）消除通道离群值，使编译后的五类 `.axmodel` 一次性通过原 CAL64 门？
2. **证据（Evidence）**：
   - B 线阶段报告（[STAGE_SUMMARY.md](file:///d:/CodeProjectHub/ax630c-plate-face-demo/docs/handoff/2026-11-23/evidence/B/STAGE_SUMMARY.md)）已证实：
     - R69/R79/R84 全部 9 个计算层均使用 **S8 权重**（840,480 码）；R57 明确是 `weight_data_type=S8`，**从未实测过 S16 权重**。
     - R84 失败的直接原因之一是 QAT 学生存在 `S-F` 浮点漂移（Max 0.0413，argmax 62/64）且训练与官方 `relu0` 范围相差 2.543 倍（13.54 vs 34.39）。
     - `ColorV2`（[color_model_v2.py](file:///d:/CodeProjectHub/ax630c-plate-face-demo/scripts/color_model_v2.py)）是纯 `Conv-BN-ReLU-Conv-BN-ReLU-MaxPool` 链路，总参数仅 ~84 万（即使全网 S16 权重也仅 ~1.68 MB，小于 `plate_rec` 的 3.6 MB）。
3. **假设（Hypothesis）**：
   - **Stage B1（单变量：权重精度 S16）**：在未改动的 `ecba573c…` 上启用 Pulsar2 `data_type: U16` + `weight_data_type: S16`（先测全计算层，或仅敏感层 `conv0` + `fc`），权重离散步长缩小 256 倍且浮点漂移 `S-F = 0`，可使 CAL64 Max 远低于 0.03、MAE 远低于 0.005、argmax 64/64。
   - **Stage B2（仅在 AX620E 不支持 Conv S16 权重时启用，单变量：数学等价 CLE）**：利用 `ReLU(s·y) = s·ReLU(y)`（$s>0$），对折叠 BN 后的相邻 Conv-ReLU-Conv 施加迭代跨层通道缩放 $W_1' = S W_1, b_1' = S b_1, W_2' = W_2 S^{-1}$。该变换在浮点域与 `ecba573c…` **严格等价**（`max|F_cle - F_source| < 1e-5`，零漂移），同时把 `relu0` ch11 等离群通道的动态范围压平，使 S8 权重 + U16 激活满足 CAL64 门禁。
4. **机制（Mechanism）**：
   - B1：直接用 Pulsar2 8.0 编译 `ecba573c…`（`weight_data_type=S16`、`data_type=U16`、`MinMax`、`compiler.check=1`），在官方 Reference 通道跑全 64 CAL64 对比原始 `ecba573c…` 浮点黄金输出。
   - B2（备选）：在 Colab 用纯 Python/ONNX 脚本对 `ecba573c…` 做 BN 折叠 + CLE，先强制断言 64/64 输入上 `max|F_cle - F_source| < 1e-4` 且 `argmax 64/64`，再送 Pulsar2 8.0 MinMax（U16+S8）编译并跑 CAL64 Reference 验收。
5. **与历史区别（History Difference）**：
   - 不重复 R69/R79/R84 已失败产物，不跑 B17 崩溃 CLI，不修改私有 API，不做无界 QAT 引入 `S-F` 漂移，不按失败样本 ID 调参。
6. **数据（Data）**：
   - 黄金浮点：`models/onnx/plate_color_finetune.onnx`（SHA256 `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360`）
   - 校准/验收：`calib_color.tar`（保持与历史一致的 464/CAL64 门禁口径，并在 TRAIN400 独立子集额外记录统计，不换门禁）。
7. **预期收益（Expected Improvement）**：在零精度损失（对原浮点黄金）前提下产出通过原 CAL64 门的五类 `plate_color5_v25_npu1.axmodel` / `npu2.axmodel`。
8. **可能退化（Possible Degradation）**：
   - B1 若启用全层 S16 权重，模型体积增至 ~1.7 MB，板上单牌耗时可能从 ~0.5 ms 增至 ~1.0–1.5 ms（仍在车牌识别 ~6.7 ms 预算内，需上板实测确认）。
   - B2 若某些层输入/输出缩放后 bias 范围变大，需检查 S32 bias 是否溢出。
9. **预算（Budget）**：Google Colab CPU/T4，每阶段 ≤ 15 分钟；0 重训，0 改原浮点模型。
10. **成功 / 失败 / 早停（Success / Failure / Early Stop）**：
    - 成功：Pulsar2 `compiler.check=1` 通过，且官方 Reference 对原浮点黄金 `ecba573c…` 在全 64 CAL64 上同时满足 **`Max ≤ 0.03`、`MAE ≤ 0.005`、`argmax 64/64`**。
    - 失败：B1 与 B2 均未达门禁 → 记录逐层 Max/MAE/Cosine，停止盲试，评估范围匹配 QAT（B3 另立提案）。
    - 早停：B2 浮点等价断言 `max|F_cle - F_source| >= 1e-4` 或 Pulsar2 报硬件不支持 S16 时立即按分支切换/停止。

---

## C-IF-004 评审结论（五类颜色双层预处理契约）

- **现状**：[COLOR_PREPROCESS_POLICY_PROPOSAL.json](file:///d:/CodeProjectHub/ax630c-plate-face-demo/docs/handoff/2026-11-23/evidence/C/COLOR_PREPROCESS_POLICY_PROPOSAL.json) 记录 canonical `ecba573c…` 在训练 `special+double` 样本时先 `merge_double` 再 resize 96×96，但 `su` 样本不拼接；而线上运行时 `PlateRecognizer::recognize` 只有 `rectified/det_layer/layer`，没有 `source` 字段。
- **评审意见（供本轮 v2.5-rc 落地参考）**：
  1. **旧三类 `classify_color` 保持 100% 不变**（仍吃 `raw_rectified`，保留 `2241e28` 的 `outs[0].size() == 3` 防误接保护）。
  2. **五类 `classify_color5` 不静默改变默认路径**：新增显式可选配置开关（例如 `color5_double_stitch = false` 默认关闭，或由同目录 sidecar JSON 声明 `preprocess: "raw_rectified" | "merge_double_if_layer1"`）。
  3. **风险登记**：在 B 线产出过门五类 `.axmodel` 后，必须在离线测试集（含 special double + SU single/double）上对比 `raw_rectified` vs `merge_double_if_layer1` 的端到端五类准确率，用实测数据决定 v2.5 最终默认配置，不靠猜测改默认。
