# B16 官方 float/preobserver 数值证人入口研究

完成时间：2026-11-17T10:07:57.004195+08:00（Asia/Shanghai）。冻结十项：`FROZEN_CONTRACT.json`。
本轮模型调用0、训练0、量化构建0、编译0、内部API调用0，无模型加载/推理。

## 结论与版本边界
- **未证实 stable8.0/e9a1aa15 无需量化重建的官方独立 preobserver 捕获入口**；这是有边界的未知，不是断言不存在。
- 实际历史版本见 R52/OFFICIAL_VERSION.txt:1–2；实际V8 run help:10–31宣告ONNX及perlayer支持。
- 公共 axnn8.0 wheel 不等于已证明与运行时逐字节相同。重新打开官方 config/run/precision 网页全部标V7.0；不能因搜索摘要称8.0就升级证据。旧Pulsar1/JOINТ资料排除。
- 链接、来源哈希及限制见 PUBLIC_EVIDENCE.json；未访问private_root/license/凭据/板/PC/生产仓库。

## 入口、精度、阶段与源码证据
1. 公开 build：`--quant.precision_analysis 1`，配置 `quant.precision_analysis`、`precision_analysis_method=PerLayer|EndToEnd`、`precision_analysis_mode=Reference|NPUBackend`、`input_sample_dir`。
2. R73 public build.py:686,698映射dump_float_data/precision_analysis_path；B16 quantize.py:143–153按原模型dtype/shape读取.npy/.bin。现有学生FP32，不能因内部激活U16就改校准输入精度。
3. **新静态证据** quantize.py:173–197先优化量化，327复制qgraph，352–354导出后dump；dump_debug_data.py:180–207 dequantize→TorchExecutor.forward→restore。floatdebug是量化后图浮点状态重跑，**不是observer现场快照**。
4. 这不能证明观察前图/权重与原图完全一致；BN融合、native优化、量化pass/权重恢复和内核阶段对应需官方说明。既有B15前端对齐不自动证明后续位精确。
5. dump并非被fetchs4096截断：flatten_tensor=False，public_fetch.py:124–125原样返回tensor；但dump_batch默认1，input_sample_dir包装一组样本字典，不能自动宣称64例捕获。
6. precision_analysis.py:287–295 EndToEnd传播量化层输出，PerLayer取float层输入后按qinfo量化；320–327 op_xrun运行量化算子。二者都不是preobserver float执行。
7. Reference是量化算子参考实现，NPUBackend是后端仿真而非物理NPU。公开文档cosine98%建议不是本项目验收门。
8. build.py:619–621 disable_compiler/ENABLE_COMPILER=0在量化之后，不能绕成“无量化独立捕获”。
9. `pulsar2 run --model <ONNX> --enable_perlayer_output`是合法公开宣告的float入口，读取metadata精度/形状raw；sim.py:213–217走ONNX解析/IRxrun，**并非calibration TorchExecutor+observer**。
10. 公共wheel普通ONNX分支213–217存在None axmodel_extra解引用静态疑点；不是已验证stable8运行时bug。未运行、未修补、未调用内部替代路径。
11. main.py:352–379仅此注册处见version/build/run/llm_build/llm_build2；没见独立calibration/precision命令，不扩大成全局不存在证明。源码函数可见不代表公开支持API。
详见 CAPABILITY_MATRIX.json、FLOAT_DUMP_STAGE_ADJUDICATION.json；build.py指R73归档public_source/axnn/yamain/command/build.py，其余摘录在本目录。

## 下一独立数值诊断：只写提案，不执行
- NEXT_NUMERICAL_DIAGNOSTIC_PROPOSAL.json十项齐全：公开float-run vs 已有B11固定64，只提供部分执行器数值证人，不能冒充preobserver。
- **NOT_STARTABLE**：先澄清exactV8 ONNX路径及图/执行器对应。若将来独立准入，64 batch1、extra0、CPU≤2、树RSS≤2GiB、forward≤120s，逐输出raw/hash；首个错哈希/非finite/不匹配/触顶即停，不重试。
- OFFICIAL_SUPPORT_GAP.json精确列出缺失命令、图/权重/精度阶段、observer更新/合并时序、固定raw枚举/调用数、逐层数值/极值、版本疑点；只报父，未外发。

## 仅一个后续修复研究假设：不可启动
**单冻结激活网格的预补偿对官方重新观察网格迁移脆弱；合法TRAIN-only证据支持的网格鲁棒蒸馏或可改善迁移。** 完整十项见 ONE_FUTURE_TECHNICAL_HYPOTHESIS.json。
- 依据B11首层official/train尺度2.5429992702474635×，116095/18874368学生激活越旧范围+4LSB、越官方范围0；这是描述证据，不是因果对照。
- S−F Max=.0413184762、Q−S Max=.0766730905，误差向量cosine=−.6708042995；存在抵消，不能相减范数当贡献比例。
- 相比R82唯一拟研究区别是网格鲁棒性，不是单冻结teacher3348网格原样重跑，不重开官方开关或CE选模。
- **2.543×、官方464尺度、CAL64/SOURCE64和失败ID不得用于训练扰动分布、loss、系数、checkpoint、停止或选择。** 目前没有合法TRAIN-only证据证明自然网格变化覆盖该迁移，故不可启动，也不硬造build。
- 普通官方S8/U16 MinMax推理端可实现不等于训练替身和observer/requant/accumulator等价；无已证实外部冻结observer/独立capture接口。若官方证人证明是执行路径伪影，或TRAIN证据无法支持相关变化，则拒绝此假设。

## 保护、资源和连续队列
- 原R84对原float Max=.04685291647911072、MAE=.005205561543925796、argmax63/64，继承R84/B11，不是本轮重测；原.03/.005/64门不改，仍NO-GO。
- CAL64=SOURCE64已用于464校准、历史搜索、调试及数值门，非独立留出；职责交叠不掩盖、不换分割、不偷换golden、不用失败例调参。
- 不需新图/标注；精确外部缺口是上述官方接口/版本语义，不是更多内存或盲重训。其他A/C继续，不把B接口未知扩大为项目停工。
- 更新B队列及根b_line_status引用，保留A/C并发内容。下一B17阻塞于官方支持说明，不重复无信息哈希/报告循环。
- 生成点研究用时363.169秒，≤600秒；生成进程限2CPU/2GiB。整轮资料研究未持续采样树RSS，不伪称实测整轮峰值；生成进程实测RSS见SUMMARY/VERIFICATION。
