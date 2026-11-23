# B 线：五类颜色量化

证据副本：[evidence/B/](evidence/B/)。数值来自 2026-11-17 交付档案，本整理未重跑量化或 NPU。

## 原门（永久不降）

Max≤0.03，MAE≤0.005，argmax **64/64**。编译/check1 PASS 不等于精度 PASS，更不等于板端/视频/生产 PASS。

## 主要候选与结果

| 对象 | SHA256（完整） | 结果 |
|---|---|---|
| canonical 原 float | `ecba573c49a847321def9106b25d6db756fd98ddd98c0909007d074e4f104360` | 黄金对照 |
| R82 学生（亦 R84 输入） | `9c840305ea6f5cb439b78ac350c05cb2f1674357bb47652e34f2f9de83600827` | 学生 float |
| R69 MinMax | — | Max 0.06927111745 / MAE 0.006630630805 / 63/64，FAIL |
| R79 EasyQuant | — | Max 0.05534318089 / MAE 0.005364822141 / 62/64，FAIL |
| **R84 学生+MinMax** | compiled `f16afae621ba3fa54cee3ab9ac062b3e26925e6497d469fc342867d8c7c4f5a4`；量化图 `4495b34b55b4f660143a6e2a51020b990a903b86f6cab672a4f7670ead7e9632` | **Max 0.04685291648 / MAE 0.005205561544 / 63/64，FAIL** |

不得启用失败的 R84 作为产品默认。失败身份迁移存在（相对 R69/R79 的失败交集很小），不按失败样本调参。

## 必须保留的限制

- **CAL64 = SOURCE64**，64/64 参与官方量化参数观察（R79 还参与 EasyQuant），同时用于调试和数值验收，**不是独立 holdout**。464 = TRAIN400 + SOURCE64。CAL64 是合成压力输入，不是真实颜色 GT 准确率。不能换分割或降门槛。
- **B11**：学生 float 与量化残差向量余弦约 -0.6708，存在抵消；**不能减 norm 做贡献比例**。Q-F 重现原黄金拒绝（Max 0.0468529165 等）。见 [evidence/B/B11_REPORT.md](evidence/B/B11_REPORT.md)。
- **B14**：全 64 SOURCE64 PNG 按公开 V8 loader 语义复现，与固定 raw 字节/元素差 0。这**不证明**实际历史 loader trace，也不是全算子 bit-true。canonical special-double 在 `plate_optim_common.py` 中先矫正再横拼（上 5/12、下自 1/3）然后颜色 resize96。仓库 `9d41` 训练仍 UNKNOWN，不能据此直接改产品 C++ 默认。见 [evidence/B/B14_REPORT.md](evidence/B/B14_REPORT.md)、[evidence/B/CROSSLINE_DOUBLE_COLOR_EVIDENCE.json](evidence/B/CROSSLINE_DOUBLE_COLOR_EVIDENCE.json)。
- **B16**：官方 float debug 是量化后 dequantize 重跑，不是 preobserver 现场捕获。公开 ONNX run 与 calibration 执行器不同。见 [evidence/B/B16_REPORT.md](evidence/B/B16_REPORT.md)。
- **B17**：只跑了一次公开 CLI：help 成功，模型 CLI 失败，`NoneType.hardware_type` 出现在 session 构造，完成 **0/64**、输出 0。未重试、未 patch、未改私有 API。见 [evidence/B/B17_REPORT.md](evidence/B/B17_REPORT.md)。**不得重复该失败用法。**

## 不要重跑

- 已失败的同一量化产物同义重跑
- B17 同一公开 CLI / 对私有 API 打补丁后再「再试一次」
- 把 R52/R53 旧权重结论外推成 R84 的 NPU 证明
- 盲重试已结构失败的混精表达（R55/R57 等具体失败保留，不把「当前不重开」写成永久禁令）

若要新的量化或训练实验：当前未获新实验准入，需新的十字段提案与相应授权。原门不降。
