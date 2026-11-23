# C 契约合并与下一完整十字段提案（2026-11-17T10:04:03+08:00，Asia/Shanghai (UTC+8)）

本阶段只做三件事：**消费 B14 已交付证据并合并模型输入契约**、**登记云端旧槽 ==3 结果（父转述，非本地重哈希，非本登记启动）**、**写出完整十字段预处理提案**。未改代码、未编译、未推理、未上板、未改任何模型、未与云端仓库并发编辑。

父方请直接读：

- [`MODEL_INPUT_BINDING_MATRIX.json`](MODEL_INPUT_BINDING_MATRIX.json)
- [`COLOR_PREPROCESS_POLICY_PROPOSAL.json`](COLOR_PREPROCESS_POLICY_PROPOSAL.json)
- [`CLOUD_NUMEL3_GUARD_REGISTRATION.json`](CLOUD_NUMEL3_GUARD_REGISTRATION.json)

## 云端 ==3（accept_no_release）

- 分支 `cursor/v25-c-integration-audit-50d6`，新提交 `2241e28c110f8a10b282619459743964e8b79d4e`（原 `3114764f0ee64ea576cc008a56748a0520194e81`）。`main` 仍为 `08eb9d6baa8e3c611dc8f3230df80fa1dcdecd07`。
- `src/plate_rec.cpp` SHA `2f1eedd78dd5705e4400ede44027098224ca1cf5239bc0d3e98dfc3cb45857a0`（16808 B），第 204 行唯一 `>=3`→`==3`。其余五源 SHA 不变。map3 / 空 `--color` HSV fallback 未改。
- 仅源码谓词 stdlib 小程序编译 0.214 秒：len 0/1/2 拒、3 接受、4/5/6/12 拒，fails=0。
- **不是**完整 `classify_color`、runtime、NPU2 或视频通过。未 merge / deploy / PR。本机未对 `2241e28` 独立重哈希。

## 模型绑定输入矩阵（字节身份 ≠ 继承）

| 模型 | SHA / 身份 | 颜色输入契约 | 状态 |
|---|---|---|---|
| 生产旧三类 | color_original `ce20ab62…`；R36 npu1 `[1,3]`/12B | raw rectified | **已知** |
| 仓库 9d41c7 | `9d41c7dc…`（与 ecba/9c840 **字节不同**） | 仅 demo 拼接；训练口径 | **TRAINING UNKNOWN**；继承 UNKNOWN |
| canonical float | `ecba573c…`；export 18 参数逐元素匹配 | special-double：先矫正，上 5/12 与下自 1/3 横拼，再 resize96 | **已证**；`source=='su'` 第 11 行提前返回不拼 |
| R84 学生 | `9c840305…`；R82 同拼接 | 同上 special-double 拼接 | **拼接已证**；R84 **不得**作为产品默认或启用路径 |

当前 C++ `classify_color5(r.rectified)` 与已证 canonical/R82 训练口径不符，是面向**未来通过 B 门的 canonical 派生候选**的真正集成缺项。不能因为 9d41 未绑定就不制定方案。也不能把该缺口写成全部接口已闭合。

部署侧（归档 `plate_rec.h` / integrated `main.cpp`，非 live checkout）：只有 `rectified`、`det_layer`、`layer`，**没有** special/su 的 `source` 字段。不能把预测或采用层数等同训练 source GT。

## 提案状态

[`COLOR_PREPROCESS_POLICY_PROPOSAL.json`](COLOR_PREPROCESS_POLICY_PROPOSAL.json) 为完整十字段（问题/证据/假设/机制/历史区别/数据/预期改善/可能退化/预算/成功失败早停），不是十条 hypothesis。`ready_to_apply=false`，待父方审阅。单一变量：raw vs 已验证 special-double 拼接；复用已冻结 `merge_double`，不发明第二套；不动旧三类、选字/跨帧、颜色规则、单层与 SU 例外。数据只用已准入 TRAIN 夹具（R55 TRAIN400 manifest `db2c85bd…`，21 double / 379 single），不改不调验证；不重跑 32/213/176/43。若需 sidecar 先方案，不擅自选默认、不启用失败 R84。
